"""Compare captured Stage1 FP64 with the Stage1 r7 offline r8brain reference.

This is an integration comparison using the same SRC code. It cannot replace a
RAW-before-SRC capture or prove what Tidal/Windows delivered to the provider.
"""
from __future__ import annotations

import json
from pathlib import Path

import numpy as np

from analyze_pcm import DTYPE


ROOT = Path(__file__).resolve().parent
RATE = 192000
LIVE_PATH = ROOT / "stage1-fp64.bin"
REF_PATH = ROOT / "offline-r8brain-fp64.bin"


def load():
    live_records = np.memmap(LIVE_PATH, dtype=DTYPE, mode="r", offset=256)
    with REF_PATH.open("rb") as f:
        frames = int(np.frombuffer(f.read(8), dtype="<u8")[0])
    reference = np.memmap(REF_PATH, dtype="<f8", mode="r", offset=8, shape=(2, frames))
    live = np.ascontiguousarray(live_records["samples"][:, :2, :].transpose(1, 0, 2)).reshape(2, -1)
    return live_records, live, reference


def exact_offsets(live, reference):
    left_bits = live[0].view("<u8")
    candidates = set()
    for second in (3.2, 10.4, 17.1, 22.3, 36.7, 46.3):
        index = int(second * RATE)
        if index >= reference.shape[1]:
            continue
        value = reference[0, index].view("<u8")
        if reference[0, index] == 0:
            continue
        for found in np.flatnonzero(left_bits == value):
            offset = int(found) - index
            lo = max(0, -offset)
            hi = min(reference.shape[1], live.shape[1] - offset)
            probe = max(lo, min(index, hi - 4096))
            if hi - lo >= 4096 and np.array_equal(
                live[:, probe + offset:probe + offset + 4096].view("<u8"),
                reference[:, probe:probe + 4096].view("<u8"),
            ):
                candidates.add(offset)
    return sorted(candidates)


def rms_envelope(samples, frames=19200):
    count = samples.shape[1] // frames
    view = samples[:, :count * frames].reshape(2, count, frames)
    return np.sqrt(np.mean(view * view, axis=2))


def coarse_offset(live, reference):
    a = rms_envelope(reference)
    b = rms_envelope(live)
    length = a.shape[1]
    best = None
    for lag in range(-length + 200, b.shape[1] - 200):
        lo = max(0, -lag)
        hi = min(length, b.shape[1] - lag)
        x = a[:, lo:hi].ravel()
        y = b[:, lo + lag:hi + lag].ravel()
        x = x - x.mean()
        y = y - y.mean()
        denom = float(np.linalg.norm(x) * np.linalg.norm(y))
        score = float(np.dot(x, y) / denom) if denom else -1.0
        if best is None or score > best[0]:
            best = (score, lag * 19200)
    return best


def refine_offset(live, reference, coarse):
    k = 36 * RATE
    n = 8192
    if k >= reference.shape[1] - n:
        k = reference.shape[1] // 2
    center = k + coarse
    left = max(0, center - 2 * 19200)
    right = min(live.shape[1], center + 2 * 19200 + n)
    if right - left < n:
        return coarse, None
    haystack = live[0, left:right]
    needle = reference[0, k:k + n]
    fft_len = 1 << (len(haystack) + n - 2).bit_length()
    corr = np.fft.irfft(np.fft.rfft(haystack, fft_len) * np.conj(np.fft.rfft(needle, fft_len)), fft_len)
    valid = corr[:len(haystack) - n + 1]
    at = int(np.argmax(np.abs(valid)))
    offset = left + at - k
    x = reference[0, k:k+n]
    y = live[0, k+offset:k+offset+n]
    correlation = float(np.dot(x, y) / (np.linalg.norm(x) * np.linalg.norm(y)))
    return offset, correlation


def compare_offset(live, reference, offset):
    lo = max(0, -offset)
    hi = min(reference.shape[1], live.shape[1] - offset)
    if hi <= lo:
        return None
    result = {"offset_live_minus_reference_frames": offset,
              "reference_start_frame": lo, "compared_frames": hi - lo,
              "compared_seconds": (hi - lo) / RATE}
    first = None
    for ch, name in enumerate(("L", "R")):
        x = reference[ch, lo:hi]
        y = live[ch, lo+offset:hi+offset]
        unequal = x.view("<u8") != y.view("<u8")
        signed_zero = unequal & (x == 0.0) & (y == 0.0)
        difference = y - x
        result[name] = {
            "different_samples": int(np.count_nonzero(unequal)),
            "signed_zero_only_differences": int(np.count_nonzero(signed_zero)),
            "other_bit_differences": int(np.count_nonzero(unequal & ~signed_zero)),
            "max_abs_error": float(np.max(np.abs(difference))),
            "rms_error": float(np.sqrt(np.mean(difference * difference))),
            "gain_estimate_read_only": float(np.dot(x, y) / np.dot(x, x)) if np.dot(x, x) else None,
        }
        if np.any(unequal):
            i = int(np.flatnonzero(unequal)[0])
            if first is None or i < first[0]:
                first = (i, name)
    if first is not None:
        i, ch_name = first
        ch = 0 if ch_name == "L" else 1
        lo_window, hi_window = max(0, i - 8), min(hi - lo, i + 9)
        result["first_difference"] = {
            "reference_frame": lo + i, "live_capture_frame": lo + offset + i,
            "channel": ch_name, "window_reference": reference[ch, lo+lo_window:lo+hi_window].tolist(),
            "window_live": live[ch, lo+offset+lo_window:lo+offset+hi_window].tolist(),
        }
    return result


def main():
    records, live, reference = load()
    offsets = exact_offsets(live, reference)
    coarse = coarse_offset(live, reference)
    if offsets:
        # One-second tones recur at integer-second offsets. Disambiguate using
        # the unique dense-stereo section, without altering samples or gain.
        unique_start, unique_frames = 37 * RATE, RATE
        scores = {}
        for offset in offsets:
            a = unique_start + offset
            if a < 0 or a + unique_frames > live.shape[1]:
                continue
            scores[offset] = int(np.count_nonzero(
                live[:, a:a + unique_frames].view("<u8") !=
                reference[:, unique_start:unique_start + unique_frames].view("<u8")))
        if not scores:
            raise RuntimeError("No exact anchor includes the unique dense-stereo section")
        best = min(scores, key=lambda o: (scores[o], abs(o - coarse[1])))
        alignment = {"method": "bitwise anchors resolved by unique dense-stereo section",
                     "exact_anchor_offsets": offsets,
                     "dense_stereo_bit_mismatches_by_offset": {str(k): v for k, v in scores.items()},
                     "coarse_envelope_correlation": coarse[0], "offset": best}
    else:
        best, corr = refine_offset(live, reference, coarse[1])
        alignment = {"method": "RMS envelope then waveform correlation", "exact_anchor_offsets": [],
                     "coarse_envelope_correlation": coarse[0], "waveform_correlation": corr,
                     "offset": best}
    result = {"schema": "entropy-stage1-reference-integration-v1",
              "source": "Stage1 r7 MultiChannelSrcEngine.cpp and bundled r8brain 7.3; WAV PCM24 decoded to FP64",
              "reference_frames": reference.shape[1], "live_frames": live.shape[1],
              "alignment": alignment, "comparison": compare_offset(live, reference, best),
              "first_stage1_sequence": int(records["block_sequence"][0]),
              "first_stage1_frame": int(records["first_frame"][0])}
    marker_checks = {}
    for name, channel, start, stop in (("L_997Hz_sync", 0, 0.20, 0.60),
                                       ("R_1237Hz_sync", 1, 0.90, 1.30)):
        lo, hi = int(start * RATE), int(stop * RATE)
        expected = reference[channel, lo:hi]
        actual = live[channel, lo + best:hi + best]
        marker_checks[name] = {
            "reference_frame_range": [lo, hi],
            "live_frame_range": [lo + best, hi + best],
            "numeric_differences": int(np.count_nonzero(expected != actual)),
            "max_abs_error": float(np.max(np.abs(actual - expected))),
        }
    result["sync_marker_checks"] = marker_checks
    first = result["comparison"].get("first_difference")
    if first is not None:
        ref_frame = first["reference_frame"]
        live_frame = first["live_capture_frame"]
        record_index, record_offset = divmod(live_frame, 1024)
        lo = max(0, ref_frame - 16)
        hi = min(reference.shape[1], ref_frame + 17)
        record = records[record_index]
        window = {
            "kind": "first bit difference; numeric difference is zero",
            "reference_frame": ref_frame,
            "live_capture_frame": live_frame,
            "stage1_block_sequence": int(record["block_sequence"]),
            "stage1_first_frame": int(record["first_frame"]),
            "stage1_frame_absolute": int(record["first_frame"]) + record_offset,
            "stage1_stream_epoch": int(record["stream_epoch"]),
            "stage1_music_generation": int(record["source_generation"][0]),
            "stage1_music_session_revision": int(record["music_session_revision"]),
            "stage1_producer_qpc_ticks": int(record["qpc_timestamp"]),
            "stage1_receive_qpc_ticks": int(record["receive_qpc"]),
            "qpc_frequency_hz": 10000000,
            "window_reference_start_frame": lo,
            "channels": {},
        }
        for channel, name in enumerate(("L", "R")):
            x = reference[channel, lo:hi]
            y = live[channel, lo + best:hi + best]
            window["channels"][name] = {
                "reference_fp64_bits_le_hex": [f"{int(v):016x}" for v in x.view("<u8")],
                "live_fp64_bits_le_hex": [f"{int(v):016x}" for v in y.view("<u8")],
            }
        (ROOT / "first-bit-difference-window.json").write_text(json.dumps(window, indent=2), encoding="utf-8")
    (ROOT / "reference-comparison.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(json.dumps({"alignment": alignment, "comparison": {k: v for k, v in result["comparison"].items()
                             if k != "first_difference"}}, indent=2))


if __name__ == "__main__":
    main()
