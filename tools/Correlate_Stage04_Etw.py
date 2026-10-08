"""Intersect raw-QPC CSwitch/DPC/ISR intervals with measured callbacks.
No CPU-cycle-to-time conversion and no deadline outcome is changed.
"""
import bisect
import collections
import csv
import json
import pathlib
import sys

def require(condition, message):
    if not condition:
        raise ValueError(message)

class IntervalIndex:
    def __init__(self, intervals):
        merged = []
        for start, end in sorted(intervals):
            if end < start:
                raise ValueError("Invalid interrupt interval")
            if merged and start <= merged[-1][1]:
                merged[-1] = (merged[-1][0], max(end, merged[-1][1]))
            else:
                merged.append((start, end))
        self.intervals = merged
        self.ends = [end for _, end in merged]
    def overlap(self, start, end):
        total = 0
        index = bisect.bisect_right(self.ends, start)
        while index < len(self.intervals) and self.intervals[index][0] < end:
            a, b = self.intervals[index]
            total += max(0, min(b, end) - max(a, start))
            index += 1
        return total

def split_callback(running, ends, interrupts, start, end):
    scheduled = interrupted = 0
    index = bisect.bisect_right(ends, start)
    while index < len(running) and running[index][0] < end:
        a, b, cpu = running[index]
        a, b = max(a, start), min(b, end)
        if b > a:
            scheduled += b-a
            if cpu in interrupts:
                interrupted += interrupts[cpu].overlap(a, b)
        index += 1
    wall = end-start
    if not 0 <= interrupted <= scheduled <= wall:
        raise ValueError("Overlapping/incomplete scheduler intervals")
    return scheduled, interrupted, scheduled-interrupted, wall-scheduled

def analyze(base):
    decoded = base / "decoded"
    metadata = json.loads((decoded / "trace-metadata.json").read_text())
    require(metadata["clock_type"] == 1 and metadata["qpc_frequency"] > 0, "Trace clock is not valid raw QPC")
    require(all(metadata[k] == 0 for k in ("process_trace_error", "events_lost", "buffers_lost", "state_mismatches", "invalid_interrupt_intervals")), "Trace has loss or invalid state")
    running = collections.defaultdict(list)
    with (decoded / "running-intervals.csv").open(newline="") as file:
        for row in csv.DictReader(file):
            running[int(row["thread_id"])].append((int(row["start_qpc"]), int(row["end_qpc"]), int(row["cpu"])))
    for intervals in running.values():
        intervals.sort()
        require(all(intervals[i][1] <= intervals[i+1][0] for i in range(len(intervals)-1)), "Overlapping thread execution")
    ends = {tid: [b for _, b, _ in values] for tid, values in running.items()}
    irq = collections.defaultdict(list)
    with (decoded / "interrupt-intervals.csv").open(newline="") as file:
        for row in csv.DictReader(file):
            irq[int(row["cpu"])].append((int(row["start_qpc"]), int(row["end_qpc"])))
    interrupts = {cpu: IntervalIndex(values) for cpu, values in irq.items()}
    total_thread_us = {tid: sum((b-a)-interrupts.get(cpu, IntervalIndex([])).overlap(a,b) for a,b,cpu in values)*1e6/metadata["qpc_frequency"] for tid,values in running.items()}
    (base / "decoded-thread-total.json").write_text(json.dumps(total_thread_us,indent=2)+"\n")
    def stats(values):
        data = sorted(values)
        return ((data[4999]+data[5000])/2, data[9899], data[-1])
    results = {}
    for mode in ("normal", "mmcss"):
        groups = collections.defaultdict(list)
        fields = ["module", "program", "format", "frames", "iteration", "wall_us", "executing_us", "descheduled_us", "interrupt_us", "scheduled_us", "thread_cycles", "context_switches", "attribution"]
        folder = base / mode
        with (folder / "benchmark-samples.csv").open(newline="") as file, (folder / "etw-deadline-misses.csv").open("w", newline="") as output:
            writer = csv.DictWriter(output, fieldnames=fields);writer.writeheader()
            for row in csv.DictReader(file):
                a, b, tid = int(row["callback_start_qpc"]), int(row["callback_end_qpc"]), int(row["thread_id"])
                require(metadata["first_qpc"] <= a <= b <= metadata["last_qpc"] and int(row["qpc_frequency"]) == metadata["qpc_frequency"], "Callback outside trace or clock mismatch")
                scheduled, interrupted, executing, descheduled = split_callback(running[tid], ends[tid], interrupts, a, b)
                require(scheduled > 0, "No scheduler coverage for callback")
                scale = 1e6 / metadata["qpc_frequency"]
                wall, execution, delay, interrupt = ((b-a)*scale, executing*scale, descheduled*scale, interrupted*scale)
                key = tuple(row[k] for k in ("module", "program", "format", "frames"))
                groups[key].append((wall, execution, delay, interrupt))
                budget = int(row["frames"])*1e6/192000
                if wall > budget:
                    parts = []
                    if execution > budget:
                        parts.append("ON_CPU_NON_IRQ_INTERVAL_EXCEEDS_DEADLINE")
                    if delay > 0:
                        parts.append("DESCHEDULING")
                    if interrupt > 0:
                        parts.append("DPC_ISR")
                    attribution = "+".join(parts) if parts else "UNEXPLAINED"
                    writer.writerow({**dict(zip(fields[:5], [row[k] for k in fields[:5]])), "wall_us": wall, "executing_us": execution, "descheduled_us": delay,
                        "interrupt_us": interrupt, "scheduled_us": scheduled*scale, "thread_cycles": row["thread_cycles"], "context_switches": row["context_switches"], "attribution": attribution})
        header = ["module", "program", "format", "frames", "samples"]
        for name in ("wall", "executing", "descheduled", "interrupt"):
            header += [f"{name}_{stat}_us" for stat in ("median", "p99", "maximum")]
        header += ["wall_deadline_misses", "execution_deadline_misses", "status"]
        total_misses = execution_misses = 0
        with (folder / "etw-callback-cost.csv").open("w", newline="") as output:
            writer = csv.writer(output);writer.writerow(header)
            for key, values in groups.items():
                require(len(values) == 10000, "Incomplete configuration")
                budget = int(key[3])*1e6/192000
                wall_miss = sum(v[0] > budget for v in values);cpu_miss = sum(v[1] > budget for v in values)
                total_misses += wall_miss;execution_misses += cpu_miss
                writer.writerow([*key, len(values), *[value for col in range(4) for value in stats([v[col] for v in values])], wall_miss, cpu_miss, "FAIL" if wall_miss else "PASS"])
        require(len(groups) == 80, "Incomplete benchmark")
        results[mode] = {"configurations": 80, "callbacks": 800000, "wall_deadline_misses": total_misses, "execution_deadline_misses": execution_misses}
    (base / "attribution-summary.json").write_text(json.dumps(results, indent=2)+"\n")
    print(json.dumps(results, indent=2))

if __name__ == "__main__":
    analyze(pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "local/stage04-blockers/etw"))
