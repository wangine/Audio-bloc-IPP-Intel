"""Native profile preflight and missing/stale physical gate, never hardware."""
import os,pathlib,subprocess,sys,time,uuid
sys.path.insert(0,str(pathlib.Path(__file__).parent))
from Stage05IpcIntegration import Client
from DiagnosticQualification import qualify
root=pathlib.Path(__file__).resolve().parents[1]
endpoint='EntropyAudioCore.Stage07.Preflight.'+uuid.uuid4().hex
core=subprocess.Popen([os.environ['STAGE05_CORE'],'--control-server','--endpoint',endpoint,'--offline-test-signal','--run-seconds','60'],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
c=None
try:
 c=Client(endpoint)
 assert not c.call('arm')['ok'],'ARM without gate accepted'
 assert c.call('headroom-policy',mode=2,manualGainDb=-4)['ok']
 time.sleep(.1)
 for mode,gain in [(2,-4),(2,0),(1,0),(0,-12)]:
  assert c.call('headroom-policy',mode=mode,manualGainDb=gain)['ok'];time.sleep(.1)
  before=c.call('telemetry')['data']['headroom']
  r=qualify(c);assert r['ok'],r
  after=c.call('telemetry')['data']['headroom']
  assert r['data']['deadlineMisses']==0 and r['data']['maximumOutputPeak']>1e-12,r
  assert r['data']['mmcssRegistered'] and r['data']['mmcssPriorityCritical'],r
  assert before['mode']==after['mode']==mode and before['manualGainDb']==after['manualGainDb']==gain
 assert c.call('headroom-transition',value=10000)['ok']
 assert c.call('headroom-policy',mode=2,manualGainDb=-4)['ok']
 r=qualify(c);assert r['ok'],r
 assert r['data']['warmupBlocks']>=1875 and r['data']['deadlineMisses']==0,r
 h=c.call('telemetry')['data']['headroom'];assert h['transition']==10000 and h['manualGainDb']==-4
 # Configuration changes remain explicitly qualified in their own process.
 for action,fields in [('headroom-attack',{'value':150}),('headroom-release',{'value':5000}),
                       ('headroom-reserve',{'group':0,'value':3}),
                       ('matrix-route',{'source':0,'destination':1,'enabled':True}),
                       ('parameter',{'id':'delayMs1','value':1})]:
  changed=c.call(action,**fields);assert changed['ok'],changed
  r=qualify(c);assert r['ok'] and r['data']['deadlineMisses']==0,r
 # A failed silent configuration may never reuse the last successful PASS.
 routes=c.call('telemetry')['data']['matrix']['routes']
 assert c.call('matrix-clear')['ok']
 for _ in range(2):
  failed=qualify(c);assert not failed['ok'] and 'no active output signal' in failed['error'],failed
 for source,mask in enumerate(routes):
  for destination in range(16):
   if mask&(1<<destination):assert c.call('matrix-route',source=source,destination=destination,enabled=True)['ok']
 r=qualify(c);assert r['ok'],r
 assert not c.call('telemetry')['data']['armed'],'preflight automatically armed physical output'
 print('PASS independent strict native profile preflight, explicit diagnostic, native policy preservation, silent output rejection, missing gate refusal')
 assert c.call('shutdown')['ok'];c.close();c=None
 core.wait(timeout=8);assert core.returncode==0
finally:
 if c:c.close()
 if core.poll() is None:core.terminate()
 out,err=core.communicate(timeout=8)
 if err:print(err.decode(errors='replace'))
