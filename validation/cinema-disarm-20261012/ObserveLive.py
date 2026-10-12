import json,pathlib,sys,time
p=pathlib.Path(__file__).parent
sys.path.insert(0,'D:/Projects/3Stage-3/Audio-bloc-IPP-Intel/Tests')
from Stage05IpcIntegration import Client
c=Client('EntropyAudioCore.Control.v1')
prefix=sys.argv[1] if len(sys.argv)>1 else 'v15-live'
seconds=int(sys.argv[2]) if len(sys.argv)>2 else 300
try:
 end=time.monotonic()+seconds
 with (p/(prefix+'-telemetry.jsonl')).open('w') as f:
  while time.monotonic()<end:
   t=c.call('telemetry',viewContractVersion=2)['data']
   f.write(json.dumps(dict(time=time.time(),telemetry=t))+'\n');f.flush();time.sleep(.1)
 (p/(prefix+'-final.json')).write_text(json.dumps(t,indent=2))
 print('live complete', {k:t[k] for k in ['armed','physicalArmed','sourceWaitCount','sourceResumeCount','liveFault']})
finally:c.close()
