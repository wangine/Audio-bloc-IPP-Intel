"""Bounded, independent coefficient transfer oracle. Private audio stays local.

No production DSP function is imported. NumPy FFT evaluates the exported real
coefficients, including input trim, integer/fractional delay and manual AP.
ASRC arithmetic/indexing is checked separately by the scalar C++ coefficient
oracle at its effective varying-rate trajectory, not by bit comparing clocks.
"""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import numpy as np

FS, FRAMES, BLOCK, FFT = 192000, 1152000, 1024, 4194304
START, END = int(.75 * FS), int(5.5 * FS)
TOLERANCE = 2e-9  # fixed before evaluating the independent electric oracle
BANDS = [(200, 400), (400, 800), (800, 1600), (1600, 4000)]


def read(p):
    return json.loads(Path(p).read_text(encoding="utf-8-sig"))


def write(p, value):
    Path(p).write_text(json.dumps(value, indent=2, allow_nan=False) + "\n", encoding="utf-8")


def sha(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1024 * 1024), b""):
            h.update(b)
    return h.hexdigest().upper()


class Tap:
    def __init__(self, path, channels=16):
        self.samples = np.memmap(path, dtype="<f8", mode="r").reshape(-1, channels, BLOCK)

    def ch(self, channel):
        return self.samples[:, channel, :].reshape(-1)


def metrics(x):
    return window_metrics(x[START:END])


def window_metrics(x):
    peak, rms = float(np.max(np.abs(x))), float(np.sqrt(np.mean(x * x)))
    return {"peak": peak, "rms": rms, "rmsDbFS": db(rms),
            "crestDb": db(peak / rms) if rms else None}


def db(x):
    return float(20 * np.log10(x)) if x > 0 else None


def section(coef, z):
    b0, b1, b2, a1, a2 = coef
    return (b0 + b1*z + b2*z*z) / (1 + a1*z + a2*z*z)


def transfer(c, freq, params):
    z = np.exp(-2j * np.pi * freq / FS)
    h = np.ones(z.shape, dtype=np.complex128)
    if not c["bypass"]:
        if c["fir"]:
            taps = list(c["firHead"])
            for row in c["firTailSpectra"]:
                part = np.asarray(row)
                taps.extend(np.fft.ifft(part[:, 0] + 1j * part[:, 1]).real[:128])
            if freq.size == FFT // 2 + 1:
                h = np.fft.rfft(taps, FFT)
            else:
                h = np.zeros(z.shape, dtype=np.complex128)
                power = np.ones(z.shape, dtype=np.complex128)
                for tap in taps:
                    h += tap * power
                    power *= z
        else:
            for coeff in c["hp"] if c["hpOn"] else []:
                h *= section(coeff, z)
            for coeff in c["lp"] if c["lpOn"] else []:
                h *= section(coeff, z)
        if c["apOn"]:
            h *= section(c["ap"], z)
    if c["delayMode"] and c["delayOrder"]:
        a = c["delayCoefficients"]
        h *= z ** c["integerTap"] * np.polynomial.polynomial.polyval(z, a[::-1]) / np.polynomial.polynomial.polyval(z, a)
    else:
        whole = int(np.floor(c["delaySamples"]))
        frac = c["delaySamples"] - whole
        h *= z ** whole * (1 - frac + frac * z)
    source = 1 if 1 <= c["role"] <= 5 else 2 if 6 <= c["role"] <= 10 else 3
    trim = 10 ** (params[f"musicTrimDb{source}"] / 20)
    return h * c["gain"] * trim * int(c["enabled"])


def compare(actual, expected):
    a, e = actual[START:END], expected[START:END]
    error = a - e
    maximum = float(np.max(np.abs(error)))
    size = len(a) // 3840 * 3840
    ae = np.sqrt(np.mean(a[:size].reshape(-1, 3840)**2, axis=1))
    ee = np.sqrt(np.mean(e[:size].reshape(-1, 3840)**2, axis=1))
    A, E = np.fft.rfft(a), np.fft.rfft(e)
    freq = np.fft.rfftfreq(len(a), 1/FS)
    bands = []
    for lo, hi in BANDS:
        mask = (freq >= lo) & (freq < hi)
        energy = float(np.vdot(E[mask], E[mask]).real)
        ratio = np.vdot(E[mask], A[mask])/energy if energy > 1e-24 else None
        band_rms = float(np.sqrt(2*energy)/len(a))
        bands.append({"bandHz": [lo, hi], "expectedBandRms": band_rms,
                      "excitation": "INFORMATIVE" if band_rms > 100*TOLERANCE else "INSUFFICIENT",
                      "relativeMagnitudeDb": db(abs(ratio)) if ratio is not None else None,
                      "relativePhaseDegrees": float(np.angle(ratio, deg=True)) if ratio is not None else None})
    excited = float(np.sqrt(np.mean(e*e))) > 100*TOLERANCE and float(np.sqrt(np.mean(a*a))) > 100*TOLERANCE
    return {"status": "PASS" if maximum < TOLERANCE and excited else "FAIL" if maximum >= TOLERANCE else "BLOCKED SILENT WINDOW", "maximumAbsoluteError": maximum,
            "rmsError": float(np.sqrt(np.mean(error*error))), "actual": metrics(actual),
            "expected": metrics(expected), "maximumEnvelopeRmsError20ms": float(np.max(np.abs(ae-ee))),
            "complexTransferRelativeToExpected": bands}


def correlation(a, b):
    x, y = a[START:END], b[START:END]
    norm = float(np.linalg.norm(x)*np.linalg.norm(y))
    return float(np.dot(x, y)/norm) if norm else None


def asrc_evidence(root):
    summaries = []
    for name, directory in (("active", root), ("without-manual-lr-ap", root/"without-manual-lr-ap")):
        domains = read(directory/"asrc-summary.json")
        for d in domains:
            timeline = np.genfromtxt(directory/f"asrc-positions-{d['domain']}.csv", delimiter=",", names=True)
            contiguous = bool(np.max(np.abs(timeline["firstGraphFrame"][1:]-timeline["endGraphFrame"][:-1])) == 0)
            d["effectivePositionContinuous"] = contiguous
            d["mapping"] = "frame f: start+sum(i=0..f-1, stepBegin+(stepEnd-stepBegin)*i/1023); sequential FP64 native origin = graph+4196"
            tap = Tap(directory/f"post-asrc-{d['domain']}.f64", 8)
            f = np.arange(BLOCK)[None, :]
            begin = timeline["stepBegin"][:, None]
            end = timeline["stepEnd"][:, None]
            graph_positions = (timeline["firstGraphFrame"][:, None] + f*begin + (end-begin)*f*(f-1)/(2*(BLOCK-1))).reshape(-1)
            # Selection uses the common Graph window, never output frame index
            # or two unmatched windows on different DAC clocks. Scalar checks
            # use the sequential FP64 path; this analytic curve selects bounds.
            selected = (graph_positions >= START) & (graph_positions < END)
            d["metricsGraphWindowSeconds"] = [.75, 5.5]
            d["perOutputMetrics"] = [window_metrics(tap.ch(c)[selected]) for c in range(8)]
        summaries.append({"variant": name, "domains": domains})
    return summaries


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("replay", type=Path)
    ap.add_argument("active_export", type=Path)
    ap.add_argument("original_profile", type=Path)
    ap.add_argument("report_directory", type=Path)
    args = ap.parse_args()
    out = args.report_directory
    out.mkdir(parents=True, exist_ok=True)
    active = read(args.active_export)["telemetry"]["data"]
    root = args.replay
    freq = np.fft.rfftfreq(FFT, 1/FS)
    results, cases = [], {}
    for variant in ("active", "without-manual-lr-ap"):
        directory = root / variant
        config, state = read(directory / "coefficients.json"), read(directory / "applied-state.json")
        params = {p["id"]: p["value"] for p in state["parameters"]}
        input_tap = Tap(directory / "graph-input.f64", 13)
        xo, matrix, head = (Tap(directory / f"{p}.f64") for p in ("post-crossover", "post-matrix", "post-headroom"))
        entries = []
        spectra = {}
        for c in config["channels"]:
            if not c["enabled"] or not 1 <= c["role"] <= 12:
                continue  # nonlinear bass/PCA is measured, never replaced by an LTI oracle
            source = 0 if c["role"] <= 5 else 1 if c["role"] <= 10 else 2
            if source not in spectra:
                spectra[source] = np.fft.rfft(input_tap.ch(source), FFT)
            expected = np.fft.irfft(spectra[source] * transfer(c, freq, params), FFT)[:FRAMES]
            row = compare(xo.ch(c["channel"]), expected)
            row.update(channel=c["channel"]+1, role=c["role"], delaySamples=c["delaySamples"])
            points = np.array([200., 400., 800., 1000., 1600., 2000., 4000.])
            hp = transfer(c, points, params)
            row["expectedElectricTransfer"] = [{"frequencyHz": float(f), "gainDb": db(abs(h)),
                                                "phaseDegrees": float(np.angle(h, deg=True))} for f, h in zip(points, hp)]
            entries.append(row)
        matrix_errors, head_errors = [], []
        routes = state["matrix"]["routes"]
        for dest in range(16):
            expected = np.zeros(FRAMES)
            for source, mask in enumerate(routes):
                if state["matrix"]["activeSourceMask"] & (1<<source) and state["matrix"]["enabledDestinationMask"] & mask & (1<<dest):
                    expected += xo.ch(source)
            matrix_errors.append(float(np.max(np.abs(matrix.ch(dest)[START:END]-expected[START:END]))))
            gain = state["headroom"]["groups"][0]["gain"]
            head_errors.append(float(np.max(np.abs(head.ch(dest)[START:END]-matrix.ch(dest)[START:END]*gain))))
        identity = np.genfromtxt(directory / "identity.csv", delimiter=",", names=True, dtype=np.uint64)
        continuous = bool(np.all(np.diff(identity["replayFrame"]) == BLOCK) and np.all(np.diff(identity["sourceFirstFrame"]) == BLOCK)
                          and np.all(np.diff(identity["sequence"]) == 1) and len(np.unique(identity["epoch"])) == 1)
        cases[variant] = {"xo": xo, "head": head, "input": input_tap, "params": params, "model": config, "state": state}
        results.append({"variant": variant, "linearChannels": entries, "matrixMaximumAbsoluteError": max(matrix_errors),
                        "headroomMaximumAbsoluteError": max(head_errors), "frameContinuity": continuous,
                        "headroomGain": gain, "perOutputMetrics": [metrics(head.ch(c)) for c in range(16)]})
    original, diagnostic = cases["active"], cases["without-manual-lr-ap"]
    changes = [key for key in original["params"] if original["params"][key] != diagnostic["params"][key]]
    assert changes == [f"allPass{c}" for c in (1, 2, 3, 5, 6, 7)], changes
    for key in ("matrix", "headroom"):
        # State sequence/counters are diagnostic metadata, compare actual controls separately below.
        fields = ("routes", "activeSourceMask", "enabledDestinationMask") if key == "matrix" else ("mode", "manualGainDb", "attack", "release", "transition", "manualTargetGain")
        assert all(original["state"][key][k] == diagnostic["state"][key][k] for k in fields)
    ab = []
    center, scoop = original["xo"].ch(3), original["xo"].ch(11)
    for label, lanes in (("L", (0, 1, 2)), ("R", (4, 5, 6))):
        a = sum(original["xo"].ch(c) for c in lanes)
        b = sum(diagnostic["xo"].ch(c) for c in lanes)
        ab.append({"side": label, "lrOriginal": metrics(a), "lrWithoutManualAP": metrics(b),
                   "lrOriginalCorrelationWithCenter": correlation(a, center), "lrNoAPCorrelationWithCenter": correlation(b, center),
                   "lrOriginalCorrelationWithScoop": correlation(a, scoop), "lrNoAPCorrelationWithScoop": correlation(b, scoop),
                   "graphUnitWeightSumWithCenterOriginal": metrics(a+center), "graphUnitWeightSumWithCenterNoAP": metrics(b+center),
                   "graphUnitWeightSumWithCenterScoopOriginal": metrics(a+center+scoop),
                   "graphUnitWeightSumWithCenterScoopNoAP": metrics(b+center+scoop)})
    unchanged = {str(ch+1): float(np.max(np.abs(original["xo"].ch(ch)-diagnostic["xo"].ch(ch)))) for ch in (3, 7, 11)}
    asrc = asrc_evidence(root)
    ok = all([c["channel"] for c in r["linearChannels"]] == [1, 2, 3, 4, 5, 6, 7]
             and r["frameContinuity"] and r["matrixMaximumAbsoluteError"] < TOLERANCE and r["headroomMaximumAbsoluteError"] < TOLERANCE
             and all(c["status"] == "PASS" for c in r["linearChannels"]) for r in results)
    ok = ok and all(d["maximumAbsoluteError"] < 1e-8 and d["resyncCount"] == 0 and d["effectivePositionContinuous"]
                    and int(d["oracleSamples"]) > 100 for v in asrc for d in v["domains"])
    report = {"numericStatus": "PASS" if ok else "FAIL", "toleranceAbsolute": TOLERANCE,
              "windowSeconds": [.75, 5.5], "fftZeroPaddingSamples": FFT, "sameFragmentReused": True,
              "originalProfileSha256": sha(args.original_profile), "variants": results, "onlyChangedParameters": changes,
              "unchangedCenterSubScoopMaximumAbsoluteError": unchanged, "diagnosticElectricSums": ab,
              "sumCaveat": "Unit electrical weights on shared Graph timeline, not acoustic prediction or an aligned live A/B DAC sum.",
              "measuredStage2Center": metrics(original["input"].ch(2)),
              "stage2CenterCorrelationWithL": correlation(original["input"].ch(2), original["input"].ch(0)),
              "stage2CenterCorrelationWithR": correlation(original["input"].ch(2), original["input"].ch(1)),
              "asrcOfflineScalarCoefficientOracle": asrc, "stage2AppliedEqCenterAlignment": "BLOCKED: no acknowledged runtime export; saved JSON is not applied evidence",
              "liveABAlignment": {"status": "NOT VERIFIED", "estimatedDifferenceFrames": active["domains"][0]["estimatedABDifferenceFrames"],
                                  "analogSkewMeasured": False},
              "limits": ["No new recording; historical six-second source cannot prove current Stage2 runtime EQ/Center settings.",
                         "Nonlinear SUB/SCOOP measured and kept unchanged, not given a false LTI reference.",
                         "ASRC uses virtual calibrated sink clocks, not a replay of historical hardware callback timestamps."]}
    write(out / "numeric.json", report)
    lines = ["# ACTIVE_AUDIO_PATH", "", "One applied Core snapshot, MUSIC, FP64/192 kHz. Original profile preserved.",
             "", f"Original SHA256: `{report['originalProfileSha256']}`.",
             "Headroom MANUAL_SAFE -3 dB; current=target=0.7079457843841379; attack 217 ms / release 4000 ms.",
             "Guardian bypass; PCM24 V2 TPDF, clip-and-report. DAC pin numbers below are one based.", "",
             "| XO | Role | Matrix destination / physical pin | Gain dB | Total delay samples | Manual AP Hz | Fractional mode | HP / LP Hz | FIR |",
             "|---|---|---|---:|---:|---:|---|---|---|"]
    params, state = original["params"], original["state"]
    for c in original["model"]["channels"]:
        ch = c["channel"]+1
        pins = [f"D{d+1} DAC {'A' if d<8 else 'B'}:{d%8+1}" for d in range(16) if state["matrix"]["routes"][ch-1] & (1<<d)]
        lines.append(f"| CH{ch} | {state['outputs'][ch-1]['label']} | {', '.join(pins) or 'unrouted'} | {params[f'musicGainDb{ch}']:.3f} | {c['delaySamples']:.6f} | {params[f'apFreq{ch}'] if c['apOn'] else 'off'} | {['Linear','Thiran1','Thiran3','Thiran5'][c['delayMode']]} | {params[f'hpHz{ch}'] if c['hpOn'] else 'off'} / {params[f'lpHz{ch}'] if c['lpOn'] else 'off'} | {'yes' if c['fir'] else 'no'} |")
    lines += ["", "Input L/R/C trims: -1.1999988556 dB each; applied control floats are retained in the numeric model.",
              "LR ways, Center and SUB all route to DAC A; SCOOP routes to DAC B.",
              "Applied Stage2 EQ/static/dynamic/Center/alignment: BLOCKED (runtime acknowledgement unavailable).",
              "Source code confirms Center's raw-LR pre-EQ tap. The historical fragment includes the real Center, with measured RMS/correlation in numeric.json.",
              "", f"Live A/B estimate: {report['liveABAlignment']['estimatedDifferenceFrames']:.6f} frames; software alignment not verified, analog skew not measured.",
              "Offline zero-origin ASRC mapping preserves epoch/source identity in identity.csv and stepBegin/stepEnd in positions CSV.",
              "No endpoint identifiers, source recording or profile contents are published."]
    (out / "ACTIVE_AUDIO_PATH.md").write_text("\n".join(lines)+"\n", encoding="utf-8")
    print(f"{report['numericStatus']} numeric; six LR/one Center coefficient oracles per variant; changes {changes}")
    for r in results:
        print(r["variant"], [(c["channel"], c["maximumAbsoluteError"]) for c in r["linearChannels"]])
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
