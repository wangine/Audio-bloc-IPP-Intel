"""Native profile preflight and missing/stale physical gate, never hardware."""
import os,pathlib,subprocess,sys,time,uuid
sys.path.insert(0,str(pathlib.Path(__file__).parent))
from Stage05IpcIntegration import Client
root=pathlib.Path(__file__).resolve().parents[1]
endpoint='EntropyAudioCore.Stage07.Preflight.'+uuid.uuid4().hex
core=subprocess.Popen([os.environ['STAGE05_CORE'],'--control-server','--endpoint',endpoint,'--offline-test-signal','--run-seconds','30'],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
c=None
try:
 c=Client(endpoint)
 assert not c.call('arm')['ok'],'ARM without gate accepted'
 assert c.call('headroom-policy',mode=2,manualGainDb=-4)['ok']
 time.sleep(.1)
 for mode,gain in [(2,-4),(2,0),(1,0),(0,-12)]:
  assert c.call('headroom-policy',mode=mode,manualGainDb=gain)['ok'];time.sleep(.1)
  before=c.call('telemetry')['data']['headroom']
  r=c.call('preflight-live');assert r['ok'],r
  after=c.call('telemetry')['data']['headroom']
  assert r['data']['deadlineMisses']==0 and r['data']['maximumOutputPeak']>1e-12,r
  assert r['data']['mmcssRegistered'] and r['data']['mmcssPriorityCritical'],r
  assert before['mode']==after['mode']==mode and before['manualGainDb']==after['manualGainDb']==gain
 assert c.call('headroom-transition',value=10000)['ok']
 assert c.call('headroom-policy',mode=2,manualGainDb=-4)['ok']
 r=c.call('preflight-live');assert r['ok'],r
 assert r['data']['warmupBlocks']>=1875 and r['data']['deadlineMisses']==0,r
 h=c.call('telemetry')['data']['headroom'];assert h['transition']==10000 and h['manualGainDb']==-4
 print('PASS independent actual native profile preflight, nonzero signal, unchanged explicit policy, strict deadline, missing gate refusal')
 assert c.call('shutdown')['ok'];c.close();c=None
 core.wait(timeout=8);assert core.returncode==0
finally:
 if c:c.close()
 if core.poll() is None:core.terminate()
 out,err=core.communicate(timeout=8)
 if err:print(err.decode(errors='replace'))
