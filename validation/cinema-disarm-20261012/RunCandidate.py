import pathlib,sys,subprocess,time,json,ctypes
root=pathlib.Path('D:/Projects/3Stage-3');repo=root/'Audio-bloc-IPP-Intel'
sys.path.insert(0,str(repo/'Tests'))
from Stage05IpcIntegration import Client
out=pathlib.Path(__file__).parent
package=repo/'release/0.1.9-v2-r3/EntropyAudioCore-0.1.9-v2-r3-64k-win-x64'
c=Client('EntropyAudioCore.Control.v1')
try:
 t=c.call('telemetry')['data'];(out/'pre-candidate.json').write_text(json.dumps(t,indent=2))
 pid=int(t['pid']);k=ctypes.WinDLL('kernel32',use_last_error=True);k.OpenProcess.argtypes=[ctypes.c_uint32,ctypes.c_int,ctypes.c_uint32];k.OpenProcess.restype=ctypes.c_void_p;k.WaitForSingleObject.argtypes=[ctypes.c_void_p,ctypes.c_uint32];k.CloseHandle.argtypes=[ctypes.c_void_p]
 handle=k.OpenProcess(0x100000,False,pid)
 if not handle:raise RuntimeError('Cannot observe existing Core shutdown')
 reply=c.call('shutdown');print('old shutdown',reply,flush=True)
 if not reply['ok']:raise RuntimeError(reply)
finally:c.close()
try:
 if k.WaitForSingleObject(handle,15000)!=0:raise RuntimeError('Old Core retained; no second owner launched')
finally:k.CloseHandle(handle)
log=(out/'candidate-core.log').open('w')
p=subprocess.Popen([str(package/'runtime/EntropyAudioCore.exe'),'--control-server','--endpoint','EntropyAudioCore.Control.v1','--stage2-input','--authorized-live-test','--profile',str(package/'Profiles/Cinema-20261012.entropy')],cwd=str(package/'runtime'),stdout=log,stderr=log,creationflags=subprocess.CREATE_NO_WINDOW)
print('Candidate PID',p.pid,flush=True)
c=Client('EntropyAudioCore.Control.v1')
try:
 t=c.call('telemetry')['data']
 if int(t['pid'])!=p.pid:raise RuntimeError('Candidate does not own endpoint')
 reply=c.call('arm');print('candidate ARM',reply,flush=True)
 if not reply['ok']:raise RuntimeError(reply)
 keys=('armed','physicalArmed','lifecycleStatus','liveFault','inputMuted','sourceWaitCount','sourceResumeCount','inputs','scheduler','domains','lastSourceWaitCause','inputResumeError','crossoverPeaks','outputs','cinemaPeaks','headroom','version')
 end=time.monotonic()+300
 with (out/'candidate-live.jsonl').open('w') as f:
  while time.monotonic()<end:
   t=c.call('telemetry',viewContractVersion=2)['data']
   f.write(json.dumps(dict(time=time.time(),telemetry={key:t.get(key) for key in keys}))+'\n');f.flush();time.sleep(.1)
 (out/'candidate-final.json').write_text(json.dumps(t,indent=2))
 print('Live complete',json.dumps({key:t.get(key) for key in ('armed','physicalArmed','liveFault','sourceWaitCount','sourceResumeCount')}),flush=True)
finally:c.close();log.close()
