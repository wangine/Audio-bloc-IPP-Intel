"""One control-side state/profile export; close Qt first, keep Core running.

No ARM/DISARM, source capture or process lifecycle command is sent. Output is
private and must stay under local/, outside Git/release uploads.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path

ap = argparse.ArgumentParser()
ap.add_argument("output", type=Path)
ap.add_argument("--original", type=Path, required=True)
ap.add_argument("--endpoint", default="EntropyAudioCore.Control.v1")
args = ap.parse_args()
root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("ipc_contract", root/"Tests/Stage05IpcIntegration.py")
contract = importlib.util.module_from_spec(spec)
spec.loader.exec_module(contract)
args.output.mkdir(parents=True, exist_ok=True)
copy = (args.output/"ACTIVE-original-copy.entropy").resolve()
export = args.output/"active-core.json"
if copy.exists() or export.exists():
    raise RuntimeError("Use a fresh export directory; existing exports are preserved")
before = hashlib.sha256(args.original.read_bytes()).hexdigest()
client = contract.Client(args.endpoint)
try:
    result = {"describe": client.call("describe"), "telemetry": client.call("telemetry", viewContractVersion=2)}
    if not all(v.get("ok") for v in result.values()):
        raise RuntimeError("Read-only control export failed")
    saved = client.call("save-profile", path=str(copy))
    if not saved.get("ok"):
        raise RuntimeError("Applied profile copy export failed")
    result["profileCopyReceipt"] = saved
    export.write_text(json.dumps(result, indent=2), encoding="utf-8")
    after = hashlib.sha256(args.original.read_bytes()).hexdigest()
    if before != after:
        raise RuntimeError("Original profile changed during export")
    print("Export saved locally; original unchanged; no source/DAC/lifecycle commands")
finally:
    client.close()
