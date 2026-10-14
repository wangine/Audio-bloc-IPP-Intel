"""Read-only audit of two StagePublish V2 passive captures.

The record layout is pinned by PassiveCapture.cpp and stage_bus_v1.h.  This
script never connects to a live audio process; all work is on saved files.
"""
from __future__ import annotations

import collections
import hashlib
import json
import os
import struct
from pathlib import Path

import numpy as np


ROOT = Path(__file__).resolve().parent
DTYPE = np.dtype([
    ("receive_qpc", "<u8"),
    ("session_id", "V16"),
    ("stream_epoch", "<u8"),
    ("block_sequence", "<u8"),
    ("first_frame", "<u8"),
    ("control_revision", "<u8"),
    ("source_generation", "<u8", (3,)),
    ("qpc_timestamp", "<u8"),
    ("music_session_revision", "<u8"),
    ("frames", "<u4"),
    ("validity_mask", "<u4"),
    ("discontinuity_mask", "<u4"),
    ("source_state", "<u4", (3,)),
    ("source_rate", "<u4", (3,)),
    ("reserved", "<u4"),
    ("samples", "<f8", (3, 1024)),
])
assert DTYPE.itemsize == 24712, DTYPE.itemsize


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(4 * 1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def open_capture(path: Path):
    size = path.stat().st_size
    if size < 256 or (size - 256) % DTYPE.itemsize:
        raise ValueError(f"Truncated or bad-size capture: {path}: {size}")
    with path.open("rb") as f:
        session = f.read(256)
    magic, abi, header_bytes = struct.unpack_from("<QII", session)
    fields = struct.unpack_from("<8I", session, 32)
    plane_stride, slot_stride, slots_offset, mapping_bytes, qpc_frequency = struct.unpack_from("<5Q", session, 64)
    meta = {
        "magic_hex": f"{magic:016x}", "abi_hex": f"{abi:08x}",
        "header_bytes": header_bytes, "session_id": session[16:32].hex(),
        "layout_id": fields[0], "channels": fields[1], "sample_rate": fields[2],
        "sample_format": fields[3], "planar": fields[4],
        "frames_per_block": fields[5], "capacity": fields[6],
        "block_header_bytes": fields[7], "plane_stride": plane_stride,
        "slot_stride": slot_stride, "slots_offset": slots_offset,
        "mapping_bytes": mapping_bytes, "qpc_frequency": qpc_frequency,
        "producer_pid": struct.unpack_from("<I", session, 168)[0],
    }
    if meta["sample_rate"] != 192000 or meta["sample_format"] != 1 or meta["planar"] != 1:
        raise ValueError(f"Unexpected transport format: {meta}")
    records = np.memmap(path, dtype=DTYPE, mode="r", offset=256)
    return meta, records


def summary(meta, rows):
    n = len(rows)
    if not n:
        return {"session": meta, "records": 0}
    seq = rows["block_sequence"]
    first = rows["first_frame"]
    epoch = rows["stream_epoch"]
    seq_bad = np.flatnonzero(np.diff(seq.astype(np.int64)) != 1)
    frame_bad = np.flatnonzero(np.diff(first.astype(np.int64)) != 1024)
    epoch_bad = np.flatnonzero(np.diff(epoch.astype(np.int64)) != 0)
    generation = rows["source_generation"][:, 0]
    gen_bad = np.flatnonzero(np.diff(generation.astype(np.int64)) != 0)
    revision = rows["music_session_revision"]
    rev_bad = np.flatnonzero(np.diff(revision.astype(np.int64)) != 0)
    counts = lambda field: {str(int(k)): int(v) for k, v in zip(*np.unique(rows[field], return_counts=True))}
    transition = lambda indices: [{"record": int(i + 1), "before": int(seq[i]), "after": int(seq[i + 1]),
                                     "first_frame_after": int(first[i + 1])} for i in indices[:20]]
    return {
        "session": meta, "records": n, "frames_recorded": n * 1024,
        "duration_pcm_seconds": n * 1024 / 192000,
        "first_sequence": int(seq[0]), "last_sequence": int(seq[-1]),
        "first_frame": int(first[0]), "last_first_frame": int(first[-1]),
        "first_epoch": int(epoch[0]), "last_epoch": int(epoch[-1]),
        "first_source_generation_music": int(generation[0]),
        "last_source_generation_music": int(generation[-1]),
        "first_music_session_revision": int(revision[0]),
        "last_music_session_revision": int(revision[-1]),
        "sequence_breaks": transition(seq_bad), "frame_breaks": transition(frame_bad),
        "epoch_changes": transition(epoch_bad), "music_generation_changes": transition(gen_bad),
        "music_revision_changes": transition(rev_bad),
        "validity_mask_counts": counts("validity_mask"),
        "discontinuity_mask_counts": counts("discontinuity_mask"),
        "source_rate_music_counts": {str(int(k)): int(v) for k, v in zip(*np.unique(rows["source_rate"][:, 0], return_counts=True))},
        "receive_qpc_first": int(rows["receive_qpc"][0]),
        "receive_qpc_last": int(rows["receive_qpc"][-1]),
        "producer_qpc_first": int(rows["qpc_timestamp"][0]),
        "producer_qpc_last": int(rows["qpc_timestamp"][-1]),
    }


def compare(stage1, stage2, delay):
    """Compare Stage2 L/R to Stage1 L/R shifted by the declared LRC delay."""
    input_by_seq = {(int(r["stream_epoch"]), int(r["block_sequence"])): i for i, r in enumerate(stage1)}
    shift_blocks, shift_inner = divmod(delay, 1024)
    # Source for output sample zero is (s-shift_blocks-1, 1024-shift_inner).
    matched = 0
    matched_samples = 0
    bit_mismatches = [0, 0]
    sumsq = [0.0, 0.0]
    maxerr = [0.0, 0.0]
    first_difference = None
    header_mismatches = []
    missing = 0
    for j, out in enumerate(stage2):
        epoch, sequence = int(out["stream_epoch"]), int(out["block_sequence"])
        i_now = input_by_seq.get((epoch, sequence))
        i_a = input_by_seq.get((epoch, sequence - shift_blocks - 1))
        i_b = input_by_seq.get((epoch, sequence - shift_blocks))
        if i_now is None or i_a is None or i_b is None:
            missing += 1
            continue
        now, a, b = stage1[i_now], stage1[i_a], stage1[i_b]
        if int(now["first_frame"]) != int(out["first_frame"]):
            header_mismatches.append({"sequence": sequence, "stage1_first": int(now["first_frame"]),
                                      "stage2_first": int(out["first_frame"])})
            continue
        if int(b["first_frame"]) != int(a["first_frame"]) + 1024:
            missing += 1
            continue
        matched += 1
        for ch in (0, 1):
            expected = np.concatenate((a["samples"][ch, 1024 - shift_inner:], b["samples"][ch, :1024 - shift_inner])) if shift_inner else b["samples"][ch]
            actual = out["samples"][ch]
            unequal = expected.view("<u8") != actual.view("<u8")
            count = int(np.count_nonzero(unequal))
            bit_mismatches[ch] += count
            diff = actual - expected
            if np.all(np.isfinite(diff)):
                sumsq[ch] += float(np.dot(diff, diff))
                maxerr[ch] = max(maxerr[ch], float(np.max(np.abs(diff))))
            else:
                maxerr[ch] = float("inf")
            if count and first_difference is None:
                sample = int(np.flatnonzero(unequal)[0])
                lo, hi = max(0, sample - 8), min(1024, sample + 9)
                first_difference = {
                    "stage2_record": j, "block_sequence": sequence,
                    "first_frame": int(out["first_frame"]), "output_frame_offset": sample,
                    "output_first_frame_plus_offset": int(out["first_frame"]) + sample,
                    "channel": "L" if ch == 0 else "R",
                    "expected_window": [float(x) for x in expected[lo:hi]],
                    "actual_window": [float(x) for x in actual[lo:hi]],
                    "window_start_offset": lo,
                    "source_epoch": epoch,
                    "source_generation_music": int(out["source_generation"][0]),
                    "music_session_revision": int(out["music_session_revision"]),
                }
        matched_samples += 1024
    return {
        "declared_lr_delay_samples": delay,
        "matched_blocks": matched, "missing_or_unmatched_blocks": missing,
        "header_first_frame_mismatches_first_20": header_mismatches[:20],
        "compared_samples_per_channel": matched_samples,
        "bit_mismatches": {"L": bit_mismatches[0], "R": bit_mismatches[1]},
        "max_abs_error": {"L": maxerr[0], "R": maxerr[1]},
        "rms_error": {"L": (sumsq[0] / matched_samples) ** 0.5 if matched_samples else None,
                      "R": (sumsq[1] / matched_samples) ** 0.5 if matched_samples else None},
        "first_difference": first_difference,
    }


def main():
    a_path = ROOT / "stage1-fp64.bin"
    b_path = ROOT / "stage2-fp64.bin"
    a_meta, a = open_capture(a_path)
    b_meta, b = open_capture(b_path)
    result = {
        "schema": "entropy-pcm-frontier-analysis-v1",
        "stage1": summary(a_meta, a), "stage2": summary(b_meta, b),
        "stage1_to_stage2_lr": compare(a, b, 2662),
        "files": {p.name: {"bytes": p.stat().st_size, "sha256": sha256(p)} for p in
                  (a_path, b_path, ROOT / "reference-24-96.wav")},
    }
    (ROOT / "analysis.json").write_text(json.dumps(result, indent=2, allow_nan=False), encoding="utf-8")
    print(json.dumps({
        "stage1_records": len(a), "stage2_records": len(b),
        "stage1_sequence_breaks": len(result["stage1"]["sequence_breaks"]),
        "stage2_sequence_breaks": len(result["stage2"]["sequence_breaks"]),
        "lr": result["stage1_to_stage2_lr"],
    }, indent=2, allow_nan=False))


if __name__ == "__main__":
    main()
