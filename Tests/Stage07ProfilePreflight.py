"""Native profile preflight and missing/stale physical gate, never hardware."""
import os,pathlib,subprocess,sys,time,uuid
sys.path.insert(0,str(pathlib.Path(__file__).parent))
from Stage05IpcIntegration import Client
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
  r=c.call('preflight-live');assert r['ok'],r
  after=c.call('telemetry')['data']['headroom']
  assert r['data']['deadlineMisses']==0 and r['data']['maximumOutputPeak']>1e-12,r
  assert r['data']['mmcssRegistered'] and r['data']['mmcssPriorityCritical'],r
  assert r['data'].get('cacheHit') is False,('changed policy reused preflight',r)
  cached=c.call('preflight-live');assert cached['ok'],cached
  assert cached['data'].get('cacheHit') is True,('unchanged profile reran expensive preflight',cached)
  assert cached['data']['validationRun']==r['data']['validationRun'],(r,cached)
  for field in ['iterations','warmupBlocks','medianUs','p99Us','maximumUs','deadlineMisses','maximumOutputPeak']:
   assert cached['data'][field]==r['data'][field],(field,r,cached)
  for action,fields in [('monitor',{'channel':3}),('gesture',{'id':'delayMs1','begin':True}),('gesture',{'id':'delayMs1','begin':False})]:
   assert c.call(action,**fields)['ok']
   viewed=c.call('preflight-live');assert viewed['ok'] and viewed['data']['cacheHit'],('view-only command discarded validated configuration',action,viewed)
   assert viewed['data']['validationRun']==r['data']['validationRun'],(action,viewed)
  assert before['mode']==after['mode']==mode and before['manualGainDb']==after['manualGainDb']==gain
 assert c.call('headroom-transition',value=10000)['ok']
 assert c.call('headroom-policy',mode=2,manualGainDb=-4)['ok']
 r=c.call('preflight-live');assert r['ok'],r
 assert r['data']['warmupBlocks']>=1875 and r['data']['deadlineMisses']==0,r
 h=c.call('telemetry')['data']['headroom'];assert h['transition']==10000 and h['manualGainDb']==-4
 # Live telemetry advances between requests; its peaks/counters must not force
 # another expensive check of the same requested configuration.
 time.sleep(.1)
 cached=c.call('preflight-live');assert cached['ok'] and cached['data']['cacheHit'],cached
 previous=cached['data']['validationRun']
 for action,fields in [('headroom-attack',{'value':150}),('headroom-release',{'value':5000}),
                       ('headroom-reserve',{'group':0,'value':3}),
                       ('matrix-route',{'source':0,'destination':1,'enabled':True}),
                       ('parameter',{'id':'delayMs1','value':1})]:
  changed=c.call(action,**fields);assert changed['ok'],changed
  r=c.call('preflight-live');assert r['ok'],r
  assert not r['data']['cacheHit'] and int(r['data']['validationRun'])>int(previous),(action,r)
  previous=r['data']['validationRun']
  cached=c.call('preflight-live');assert cached['ok'] and cached['data']['cacheHit'],cached
 # A failed silent configuration may never reuse the last successful PASS.
 routes=c.call('telemetry')['data']['matrix']['routes']
 assert c.call('matrix-clear')['ok']
 for _ in range(2):
  failed=c.call('preflight-live');assert not failed['ok'] and 'no active output signal' in failed['error'],failed
 for source,mask in enumerate(routes):
  for destination in range(16):
   if mask&(1<<destination):assert c.call('matrix-route',source=source,destination=destination,enabled=True)['ok']
 r=c.call('preflight-live');assert r['ok'] and not r['data']['cacheHit'],r
 assert int(r['data']['validationRun'])>int(previous),r
 assert not c.call('telemetry')['data']['armed'],'preflight automatically armed physical output'
 print('PASS independent strict native profile preflight, unchanged configuration cache, setting invalidation, failed PASS exclusion, missing gate refusal')
 assert c.call('shutdown')['ok'];c.close();c=None
 core.wait(timeout=8);assert core.returncode==0
finally:
 if c:c.close()
 if core.poll() is None:core.terminate()
 out,err=core.communicate(timeout=8)
 if err:print(err.decode(errors='replace'))
