"""Read-only attribution of the existing Stage020 taps; no capture or DSP replay."""
import argparse
import csv
import json
from pathlib import Path
import numpy as np

def read(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))

def tap(path, channel, channels):
    raw = np.memmap(path, dtype="<f8", mode="r")
    assert raw.size % (channels * 1024) == 0
    return raw.reshape(-1, channels, 1024)[:, channel, :].reshape(-1)

def metric(x):
    assert np.all(np.isfinite(x))
    a = np.abs(x)
    indices = np.flatnonzero(a > 1)
    first = int(indices[0]) if len(indices) else None
    return {"samplePeak": float(a.max()), "overrangeSamples": int(len(indices)),
            "firstFrame": first, "firstValue": float(x[first]) if first is not None else None,
            "firstSeconds": first / 192000 if first is not None else None}

def main():
    p = argparse.ArgumentParser()
    p.add_argument("taps", type=Path)
    p.add_argument("pcm_deltas", type=Path)
    p.add_argument("output", type=Path)
    args = p.parse_args()
    state = read(args.taps / "active/applied-state.json")
    counters = read(args.pcm_deltas)
    routes = state["matrix"]["routes"]
    findings = []
    for domain in counters["domains"]:
        d = domain["dac"]
        for pin in domain["pins"]:
            if not int(pin["overrangeSamples"]):
                continue
            dest = int(pin["matrixDestination"]) - 1
            contributors = [i for i, r in enumerate(routes) if int(r) & (1 << dest)]
            assert len(contributors) == 1, "this observed route must be one-to-one"
            source = contributors[0]
            stages = {}
            for name, lane in [("post-crossover", source), ("post-matrix", dest), ("post-headroom", dest)]:
                stages[name] = metric(tap(args.taps / "active" / (name + ".f64"), lane, 16))
            asrc = metric(tap(args.taps / ("post-asrc-" + str(d) + ".f64"), int(pin["physicalPin"]) - 1, len(domain["pins"])))
            assert asrc["overrangeSamples"] == int(pin["overrangeSamples"])
            stages["post-asrc"] = asrc
            # The saved clock trajectory maps output frames to graph positions.
            # Approximation used only for event attribution, never as DSP oracle.
            with open(args.taps / ("asrc-positions-" + str(d) + ".csv"), newline="") as f:
                rows = list(csv.DictReader(f))
            frame = asrc["firstFrame"]
            row = rows[frame // 1024]
            offset = frame % 1024
            mapped = float(row["firstGraphFrame"]) + offset * float(row["stepBegin"]) + (float(row["stepEnd"]) - float(row["stepBegin"])) * offset * (offset - 1) / (2 * 1023)
            findings.append({"dac": d, "physicalPin": pin["physicalPin"], "matrixDestination": dest + 1,
                             "crossoverChannel": source + 1, "firstOverrangeTap": next(k for k, v in stages.items() if v["overrangeSamples"]),
                             "stages": stages, "asrcFirstMappedGraphFrameApproximate": mapped,
                             "observedMissingMarginDb": float(20 * np.log10(asrc["samplePeak"]))})
    result = {"scope": "historical Stage020 active fragment; no current-live inference",
              "rangeStatus": "FAIL OBSERVED OVERRANGE" if findings else "PASS NO OBSERVED OVERRANGE",
              "effectiveMode": state["headroom"]["effectiveMode"],
              "appliedGain": state["headroom"]["groups"][0]["gain"],
              "truePeak": "NOT IMPLEMENTED", "profileChanged": False, "findings": findings}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))

if __name__ == "__main__":
    main()
