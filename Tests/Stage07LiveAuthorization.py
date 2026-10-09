"""Explicit session authorization never fabricates an offline PASS or auto-ARMs."""
import json, os, pathlib, subprocess, sys, uuid
sys.path.insert(0,str(pathlib.Path(__file__).parent))
from Stage05IpcIntegration import Client
exe=os.environ['STAGE05_CORE']
for authorized in (False,True,False)*10:
 endpoint='EntropyAudioCore.Authorization.'+uuid.uuid4().hex
 # Missing profile prevents connecting to any actual producer. No DAC opens.
 missing=pathlib.Path(__file__).resolve().parents[1]/'local/stage07'/('absent-'+uuid.uuid4().hex+'.entropy')
 args=[exe,'--control-server','--endpoint',endpoint,'--stage2-input','--profile',str(missing),'--run-seconds','15']
 if authorized:args.append('--authorized-live-test')
 p=subprocess.Popen(args,stdout=subprocess.PIPE,stderr=subprocess.PIPE);c=None
 try:
  c=Client(endpoint);d=c.call('describe')['data'];t=c.call('telemetry')['data']
  assert d['physicalArmAllowed']==authorized,d
  assert not d['offlineGatePassed'] and d['authorizedLiveTest']==authorized,d
  assert not t['armed'] and t['profileRequired']
  assert all(d['state']=='DISARMED' and not d.get('physicalSinkOpen',False) for d in t.get('domains',[]))
  r=c.call('arm');assert not r['ok'],r
  assert ('profile' in r['error'].lower()) if authorized else ('gate' in r['error'].lower()),r
  assert not c.call('telemetry')['data']['armed']
  assert c.call('shutdown')['ok'];c.close();c=None;p.wait(timeout=5);assert p.returncode==0
 finally:
  if c:c.close()
  if p.poll() is None:p.terminate()
  out,err=p.communicate(timeout=5)
  if err:print(err.decode(errors='replace'))
# The explicit physical session flag cannot convert an offline fixture to live.
r=subprocess.run([exe,'--control-server','--authorized-live-test','--offline-test-signal'],capture_output=True)
assert r.returncode!=0 and b'requires --stage2-input' in r.stderr,r.stderr
print('PASS explicit session authorization, no auto-ARM, no fabricated PASS, no persistence, original input/profile gate retained; no hardware')
