import json,pathlib,sys,time
p=pathlib.Path(__file__).parent
sys.path.insert(0,'D:/Projects/3Stage-3/Audio-bloc-IPP-Intel/Tests')
from Stage05IpcIntegration import Client
c=Client('EntropyAudioCore.Control.v1')
try:
 end=time.monotonic()+125
 with (p/'timing-telemetry.jsonl').open('w') as f:
  while time.monotonic()<end:
   t=c.call('telemetry',viewContractVersion=2)['data']
   f.write(json.dumps(dict(time=time.time(),telemetry=t))+'\n');f.flush();time.sleep(.1)
finally:c.close()
