"""Summarize measured CSVs without changing any test outcome or tolerance."""
import csv
import json
import pathlib
import sys

root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "local/stage04-results")
output = pathlib.Path(sys.argv[2] if len(sys.argv) > 2 else "validation/stage04")
output.mkdir(parents=True, exist_ok=True)
def rows(name):
    with (root / name).open(encoding="utf-8-sig", newline="") as file:
        return list(csv.DictReader(file))
numeric, bench, clocks = rows("numerical.csv"), rows("benchmark.csv"), rows("clock-domains.csv")
assert len(numeric) == 96 and len(bench) == 80 and len(clocks) == 8, "Incomplete evidence"
summary = {
    "numeric_pass": sum(r["status"] == "PASS" for r in numeric),
    "benchmark_pass": sum(r["status"] == "PASS" for r in bench),
    "benchmark_fail": sum(r["status"] == "FAIL" for r in bench),
    "deadline_misses": sum(int(r["deadline_misses"]) for r in bench),
    "clock_domain_fail": sum(r["status"] != "PASS" for r in clocks),
    "maximum_absolute_error": max(float(r["max_abs"]) for r in numeric),
    "maximum_rms_error": max(float(r["rms_error"]) for r in numeric),
    "maximum_phase_difference_rad": max(abs(float(r["phase_difference_rad"])) for r in numeric),
    "maximum_latency_difference_samples": max(abs(int(r["latency_difference_samples"])) for r in numeric),
    "bit_mismatches": sum(int(r["bit_mismatches"]) for r in numeric),
    "pcm_byte_mismatches": sum(int(r["pcm_byte_mismatches"]) for r in numeric),
    "minimum_sine_amplitude": min(float(r["amplitude"]) for r in numeric if r["signal"] == "0"),
    "maximum_sine_amplitude": max(float(r["amplitude"]) for r in numeric if r["signal"] == "0"),
    "minimum_first_active_sample": min(int(r["first_active"]) for r in numeric),
    "maximum_first_active_sample": max(int(r["first_active"]) for r in numeric),
}
(output / "measured-summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
lines = ["| Module | Frames | Median range µs | p99 range µs | Worst maximum µs | Misses |", "|---|---:|---:|---:|---:|---:|"]
for module in ("Complete", "AudioGraph", "FIFO", "ClockSync", "PCM24"):
    for frames in (128, 256, 512, 1024):
        group = [r for r in bench if r["module"] == module and int(r["frames"]) == frames]
        def span(key):
            values = [float(r[key]) for r in group]
            return f"{min(values):.3f}–{max(values):.3f}"
        lines.append(f"| {module} | {frames} | {span('median_us')} | {span('p99_us')} | {max(float(r['max_us']) for r in group):.3f} | {sum(int(r['deadline_misses']) for r in group)} |")
(output / "benchmark-table.md").write_text("\n".join(lines)+"\n", encoding="utf-8")
print(json.dumps(summary, indent=2))
