"""Summarize executed offline measurements; never manufactures missing results."""
import csv
import json
import math
import sys
from collections import Counter
from pathlib import Path

root = Path(sys.argv[1])
options = set(sys.argv[2:])
assert options <= {"--merge-shards", "--merge-benchmark"}, "Unknown summary option"

def rows(path):
    with path.open(encoding="utf-8-sig", newline="") as stream:
        return list(csv.DictReader(stream))

initial = root / "numerical-initial-complete.csv"
if "--merge-shards" in options:
    numerical = rows(initial)
    for shard in range(8):
        numerical.extend(rows(root / f"shard-{shard}" / "numerical.csv"))
    parameters = rows(root / "parameters.csv")[:39*160]
    for shard in range(8):
        parameters.extend(rows(root / f"shard-{shard}" / "parameters.csv"))
    assert len(parameters) == 17920
    assert len({(r['program'],r['variant'],r['block'],r['output'],r['parameter']) for r in parameters}) == 17920
    with (root / "parameters-complete.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(parameters[0]))
        writer.writeheader()
        writer.writerows(parameters)
    for row in numerical:
        if "native_tolerance_diagnostic" not in row:
            row["native_tolerance_diagnostic"] = (
                "WITHIN_NATIVE_TOLERANCE" if row["status"] == "PASS"
                else "OUTSIDE_NATIVE_TOLERANCE")
        if row["oracle"] == "V07":
            row["status"] = "BLOCKED"
    with (root / "numerical-complete.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(numerical[0]))
        writer.writeheader()
        writer.writerows(numerical)
else:
    numerical = rows(root / "numerical.csv")

def metric_summary(data):
    result = {"rows": len(data), "statuses": dict(Counter(r["status"] for r in data))}
    for key in ("max_absolute_error", "rms_error", "analytic_amplitude_error",
                "analytic_phase_error_rad", "phase_difference_rad", "peak_lag_samples",
                "amplitude_error", "phase_error_rad", "max_error"):
        if data and key in data[0]:
            values = [(abs(float(r[key])), r) for r in data if r[key] and math.isfinite(float(r[key]))]
            if not values:
                result[key] = None
                continue
            value, worst = max(values, key=lambda pair: pair[0])
            result[key] = {"maximum": value, "case": worst}
    for key in ("bit_mismatches", "nonfinite", "cpp_new", "cpp_delete", "heap_alloc", "heap_free", "wait", "lock", "io"):
        if data and key in data[0]:
            result[key] = sum(int(r[key]) for r in data)
    if data and "callbacks" in data[0]:
        result["callbacks"] = sum(int(r["callbacks"]) for r in data)
    if data and "sine_amplitude" in data[0]:
        differences = [abs(float(r["sine_amplitude"])-float(r["reference_sine_amplitude"])) for r in data]
        result["oracle_amplitude_difference_max"] = max(v for v in differences if math.isfinite(v)) if any(math.isfinite(v) for v in differences) else None
    return result

cases = Counter((r["case"], r["block"], r["frequency_hz"]) for r in numerical)
assert len(cases) == 2128 and set(cases.values()) == {32}, (len(cases), Counter(cases.values()))
assert len(numerical) == 68096
full_keys = {(r["case"], r["block"], r["frequency_hz"], r["oracle"], r["output"]) for r in numerical}
assert len(full_keys) == 68096, "Duplicate oracle/output row"
frequencies = {str(f) for f in (10,20,40,80,160,315,630,1000,2000,4000,8000,16000,24000,40000,60000)}
for program in ("MUSIC", "CINEMA"):
    for variant in ("unity", "IIR-BW12", "FIR2049", "FIR4097", "FIR8193", "FIR8193-AP", "IIR-AP"):
        for block in ("1","7","31","64","127","256","511","1024"):
            expected = {(f"{program}-{variant}-sine", block, f) for f in frequencies}
            expected |= {(f"{program}-{variant}-{kind}", block, "0") for kind in ("impulse","sweep","multitone","noise")}
            assert expected <= cases.keys(), "Missing required configuration/stimulus"
summary = {"numerical_cases": len(cases), "numerical": {}}
for oracle in ("native", "V07"):
    subset = [r for r in numerical if r["oracle"] == oracle]
    summary["numerical"][oracle] = metric_summary(subset)
    summary["numerical"][oracle]["tolerance_diagnostic"] = dict(Counter(r["native_tolerance_diagnostic"] for r in subset))
for name in ("identity", "controls", "delay-impulses", "transition-windows", "realtime"):
    path = root / f"{name}.csv"
    if path.exists():
        summary[name] = metric_summary(rows(path))
if (root / "transitions.csv").exists():
    data = rows(root / "transitions.csv")
    summary["transitions"] = {o: metric_summary([r for r in data if r["oracle"] == o]) for o in ("native", "V07")}
if "--merge-benchmark" in options:
    data = rows(root / "benchmark-initial.csv") + rows(root / "benchmark-resume" / "benchmark.csv")
    keys = Counter((r["module"], r["program"], r["configuration"], r["frames"]) for r in data)
    assert len(data) == 224 and len(keys) == 224 and set(keys.values()) == {1}, "Benchmark incomplete/duplicate"
    with (root / "benchmark.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(data[0]))
        writer.writeheader()
        writer.writerows(data)
if (root / "benchmark.csv").exists():
    data = rows(root / "benchmark.csv")
    summary["benchmark"] = {"rows": len(data), "statuses": dict(Counter(r["status"] for r in data)),
        "maximum_utilization": max(float(r["max_utilization"]) for r in data)}
    summary["benchmark"]["fir8193"] = [r for r in data if r["configuration"] == "FIR8193"]
    summary["benchmark"]["failures"] = [r for r in data if r["status"] != "PASS"]
(root / "summary.json").write_text(json.dumps(summary, indent=2, allow_nan=False), encoding="utf-8")
print(json.dumps({"cases": len(cases), "checks": {k: v.get("statuses", {}) for k, v in summary.items() if isinstance(v, dict)}}, indent=2))
