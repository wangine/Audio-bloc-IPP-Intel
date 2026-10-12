import pathlib,sys,json,datetime,time
root=pathlib.Path('D:/Projects/3Stage-3')
sys.path.insert(0,str(root/'Audio-bloc-IPP-Intel/Tests'))
from Stage05IpcIntegration import Client
out=pathlib.Path(__file__).parent
if len(sys.argv)>1:out=out/sys.argv[1];out.mkdir(exist_ok=True)
client=Client('EntropyAudioCore.Control.v1')
try:
    initial=client.call('telemetry',viewContractVersion=2)
    (out/'initial.json').write_text(json.dumps(initial,indent=2))
    keys=('armed','physicalArmed','lifecycleStatus','liveFault','inputMuted','sourceWaitCount','sourceResumeCount','inputs','scheduler','domains')
    print(json.dumps({k:initial['data'].get(k) for k in keys},indent=2),flush=True)
    saved=client.call('save-profile',path=str(out/'stage3-before.entropy'))
    (out/'profile-save.json').write_text(json.dumps(saved,indent=2))
    print('DISARM',client.call('disarm'),flush=True)
    armed=client.call('arm')
    (out/'arm.json').write_text(json.dumps(armed,indent=2))
    print('ARM',json.dumps(armed),flush=True)
    if not armed['ok']:raise RuntimeError(armed)
    end=time.monotonic()+120
    with (out/'telemetry.jsonl').open('w') as f:
        while time.monotonic()<end:
            r=client.call('telemetry',viewContractVersion=2)
            t=r['data']
            f.write(json.dumps(dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),telemetry={k:t.get(k) for k in keys+('lastSourceWaitCause','inputResumeError','crossoverPeaks','outputs','cinemaPeaks','headroom')}))+'\n')
            f.flush();time.sleep(.1)
    print(json.dumps({k:r['data'].get(k) for k in keys},indent=2),flush=True)
finally:client.close()
