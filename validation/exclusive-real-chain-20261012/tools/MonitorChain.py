import sys,pathlib,json,time,datetime
root=pathlib.Path('D:/Projects/3Stage-3')
sys.path.insert(0,str(root/'Audio-bloc-IPP-Intel/Tests'))
from Stage05IpcIntegration import Client
out=root/'Diagnostics/Exclusive-RealChain-20261012'
client=Client('EntropyAudioCore.Control.v1')
try:
    description=client.call('describe');(out/'stage3-description.json').write_text(json.dumps(description,indent=2))
    initial=client.call('telemetry',viewContractVersion=2);(out/'stage3-before.json').write_text(json.dumps(initial,indent=2))
    saved=client.call('save-profile',path=str(out/'stage3-before.entropy'))
    (out/'stage3-save-profile.json').write_text(json.dumps(saved,indent=2))
    if not saved['ok']:raise RuntimeError(saved)
    print(json.dumps({k:initial['data'].get(k) for k in ('armed','physicalArmed','lifecycleStatus','headroom','domains')}),flush=True)
    end=time.monotonic()+65
    with (out/'baseline-stage3-telemetry.jsonl').open('w') as file:
        while time.monotonic()<end:
            reply=client.call('telemetry',viewContractVersion=2)
            if not reply['ok']:raise RuntimeError(reply)
            file.write(json.dumps(dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),telemetry=reply['data']))+'\n')
            file.flush();time.sleep(.05)
    print('Read-only real-chain telemetry complete.',flush=True)
finally:client.close()
