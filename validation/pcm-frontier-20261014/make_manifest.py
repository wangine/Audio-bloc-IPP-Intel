"""Write SHA256/identity manifest for the Stage A evidence directory."""
from __future__ import annotations

import datetime as dt
import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parent
PROJECT = Path(r"D:\Projects\Entropy-Stage3-Recovery-20261013")
STAGE1 = Path(r"D:\Projects\3Stage-3\Stage1-v0003")
STAGE2 = Path(r"D:\Projects\Entropy_Stage3_Expander\candidate-v0010")


def identity(path: Path) -> dict:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(4 * 1024 * 1024), b""):
            h.update(chunk)
    return {"path": str(path), "bytes": path.stat().st_size, "sha256": h.hexdigest()}


def main():
    analysis = json.loads((ROOT / "analysis.json").read_text(encoding="utf-8"))
    comparison = json.loads((ROOT / "reference-comparison.json").read_text(encoding="utf-8"))
    external = {
        "reference_flac": Path(r"C:\Users\bobi\Downloads\Entropy_TIDAL_Diagnostic_24bit_96000Hz.flac"),
        "architecture_contract_zip": Path(r"C:\Users\bobi\Downloads\ENTROPY-ARHITECTURA-SI-TESTE-PCM.zip"),
        "stage1_core": STAGE1 / "release/stage1-native-music-v0003-r7/core/EntropyMusicSourceCore.exe",
        "stage1_raw_provider": STAGE1 / "release/stage1-native-music-v0003-r7/core/providers/EntropyMusicRawProviderV2.dll",
        "stage1_r7_src_source": STAGE1 / "delivery/r7-verified-local/stage1-native-music-v0003-r7-source/ThirdParty/NativeSrc/Modules/SRC/src/MultiChannelSrcEngine.cpp",
        "stage2_engine": STAGE2 / "release/stage2-music-v0010-dynamic-recovery-candidate/EntropyDspEngine.exe",
        "stage3_r3_core": PROJECT / "live/R3-64k/runtime/EntropyAudioCore.exe",
        "passive_capture_tool": Path(r"D:\Projects\3Stage-3\Diagnostics\Exclusive-RealChain-20261012\tools\PassiveCapture.exe"),
    }
    outputs = [
        "FRONTIER-REPORT.md", "reference-24-96.wav", "offline-r8brain-fp64.bin",
        "stage1-fp64.bin", "stage2-fp64.bin", "analysis.json",
        "reference-comparison.json", "first-bit-difference-window.json",
        "analyze_pcm.py", "compare_reference.py", "make_manifest.py", "verify_artifacts.py",
        "offline_ref/CMakeLists.txt", "offline_ref/build.cmd", "offline_ref/offline_ref.cpp",
        "offline_ref/build/offline_ref.exe",
    ]
    wav = ROOT / "reference-24-96.wav"
    with wav.open("rb") as f:
        f.read(44)
        raw_hash = hashlib.sha256(f.read()).hexdigest()
    manifest = {
        "schema": "entropy-stage-a-pcm-frontier-manifest-v1",
        "generated_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "scope": "Tidal live 24/96 loop, passive StagePublish V2, Stage1 r7 offline r8brain integration reference; no DAC test",
        "source_checkout": {
            "path": str(PROJECT / "source"),
            "branch": "chatgpt/stage3-audio-recovery-20261013",
            "head": "a016495d9dc1cffa3cb3e1e46063f57801727667",
            "worktree_clean": True,
        },
        "reference_raw_pcm24_le_stereo_sha256": raw_hash,
        "external_inputs_and_binaries": {name: identity(path) for name, path in external.items()},
        "outputs": [identity(ROOT / name) for name in outputs],
        "capture_terminal": {
            "stage1": {"requested_seconds": 112, "records": 16927, "empty_polls": 5859,
                       "terminal_enum": 2, "terminal_name": "Gap", "qpc_frequency_hz": 10000000},
            "stage2": {"requested_seconds": 112, "records": 17109, "empty_polls": 5919,
                       "terminal_enum": 2, "terminal_name": "Gap", "qpc_frequency_hz": 10000000},
        },
        "stage1_session": analysis["stage1"]["session"],
        "stage2_session": analysis["stage2"]["session"],
        "comparison": {
            "stage1_to_stage2_lr": analysis["stage1_to_stage2_lr"],
            "reference_to_stage1_alignment": comparison["alignment"],
            "reference_to_stage1_sync_markers": comparison["sync_marker_checks"],
            "reference_to_stage1_summary": {k: v for k, v in comparison["comparison"].items()
                                             if k != "first_difference"},
        },
    }
    (ROOT / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    print(json.dumps({"output_count": len(outputs), "raw24_sha256": raw_hash,
                      "stage1_capture_sha256": manifest["outputs"][3]["sha256"],
                      "stage2_capture_sha256": manifest["outputs"][4]["sha256"]}))


if __name__ == "__main__":
    main()
