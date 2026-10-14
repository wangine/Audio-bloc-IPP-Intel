"""Final read-only integrity and result check for this Stage A evidence set."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parent
manifest = json.loads((ROOT / "manifest.json").read_text(encoding="utf-8"))
analysis = json.loads((ROOT / "analysis.json").read_text(encoding="utf-8"))
reference = json.loads((ROOT / "reference-comparison.json").read_text(encoding="utf-8"))
window = json.loads((ROOT / "first-bit-difference-window.json").read_text(encoding="utf-8"))


def check_file(item):
    path = Path(item["path"])
    assert path.is_file(), path
    assert path.stat().st_size == item["bytes"], path
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(4 * 1024 * 1024), b""):
            h.update(chunk)
    assert h.hexdigest() == item["sha256"], path


for item in manifest["outputs"]:
    check_file(item)
for item in manifest["external_inputs_and_binaries"].values():
    check_file(item)
assert manifest["reference_raw_pcm24_le_stereo_sha256"] == "9f6a8741f11cf692f124d673f05ae07a13f1814da0282c58b37c0fb859d68f1e"
assert analysis["stage1"]["records"] == 16927
assert analysis["stage2"]["records"] == 17109
for role in ("stage1", "stage2"):
    assert not analysis[role]["sequence_breaks"]
    assert not analysis[role]["frame_breaks"]
    assert not analysis[role]["epoch_changes"]
    assert analysis[role]["session"]["sample_rate"] == 192000
    assert analysis[role]["source_rate_music_counts"] == {"96000": analysis[role]["records"]}
assert analysis["stage1"]["session"]["session_id"] == analysis["stage2"]["session"]["session_id"]
lr = analysis["stage1_to_stage2_lr"]
assert lr["declared_lr_delay_samples"] == 2662
assert lr["compared_samples_per_channel"] == 17330176
assert lr["bit_mismatches"] == {"L": 0, "R": 0}
assert lr["max_abs_error"] == {"L": 0.0, "R": 0.0}
assert not lr["header_first_frame_mismatches_first_20"]
comp = reference["comparison"]
assert reference["alignment"]["offset"] == 444416
assert comp["compared_frames"] == 10368000
for channel in ("L", "R"):
    assert comp[channel]["other_bit_differences"] == 0
    assert comp[channel]["max_abs_error"] == 0.0
    assert comp[channel]["rms_error"] == 0.0
for check in reference["sync_marker_checks"].values():
    assert check["numeric_differences"] == 0
assert window["stage1_block_sequence"] == 12228622
assert window["stage1_frame_absolute"] == 12522107905
assert window["channels"]["L"]["reference_fp64_bits_le_hex"][1] == "8000000000000000"
assert window["channels"]["L"]["live_fp64_bits_le_hex"][1] == "0000000000000000"
print(f"PASS {len(manifest['outputs'])} outputs + {len(manifest['external_inputs_and_binaries'])} external identities; full 54 s numeric comparison and Stage1-to-Stage2 LR bits")
