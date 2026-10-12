import pathlib, sys, json, datetime
root = pathlib.Path('D:/Projects/3Stage-3')
sys.path.insert(0, str(root / 'Audio-bloc-IPP-Intel/Tests'))
from Stage05IpcIntegration import Client
client = Client('EntropyAudioCore.Control.v1')
try:
    reply = client.call('telemetry', viewContractVersion=2)
    assert reply['ok']
    data = reply['data']
    result = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(), telemetry=data)
    path = root / 'Diagnostics/Exclusive-RealChain-20261012/final-restored-live-status.json'
    path.write_text(json.dumps(result, indent=2))
    print(json.dumps({k: data.get(k) for k in ('pid', 'armed', 'physicalArmed', 'lifecycleStatus', 'liveFault', 'inputMuted', 'sourceWaitCount', 'sourceResumeCount', 'scheduler')}, indent=2))
finally:
    client.close()
