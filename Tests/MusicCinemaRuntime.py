"""Isolated process/IPC regression. Synthetic PCM only. Physical output prohibited."""
import json, os, pathlib, subprocess, time
from Stage05IpcIntegration import Client
ROOT=pathlib.Path(__file__).resolve().parents[1]
exe=os.environ['STAGE05_CORE']; publisher=os.environ['MUSIC_CINEMA_PUBLISHER']
def run(args):
 endpoint='EntropyAudioCore.MusicCinema.Test.'+str(os.getpid())
 p=subprocess.Popen([exe,'--control-server','--endpoint',endpoint,*args,'--run-seconds','30'],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
 try:c=Client(endpoint)
 except Exception:
  p.terminate();print(p.communicate(timeout=5));raise
 return p,c
def stop(p,c):
 c.close();p.terminate();p.communicate(timeout=5)
def wait(c,predicate):
 until=time.monotonic()+5
 while time.monotonic()<until:
  s=c.call('telemetry',viewContractVersion=2)['data']
  if predicate(s):return s
  time.sleep(.025)
 raise AssertionError({k:s[k] for k in ("blocks","selectedProgram","inputState","inputs") if k in s})
def main():
 music=subprocess.Popen([publisher,'4','21','20'],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
 cinema=None;p=c=None
 try:
  music_endpoint=music.stdout.readline().strip()
  p,c=run(['--stage2-input','--input-only','--music-endpoint',music_endpoint])
  s=wait(c,lambda s:int(s['blocks'])>15)
  assert s['inputContractVersion']==1 and s['inputs']['music']['channels']==3 and s['inputs']['cinema']['state']=='WAITING'
  assert not s['armed'] and not c.call('arm')['ok']
  before=int(s['blocks']);r=c.call('program',program=1);assert not r['ok'] and 'CINEMA' in r['error']
  assert c.call('parameter',id='musicGainDb1',value=-1)['ok']
  s=wait(c,lambda s:int(s['blocks'])>before+20);assert s['selectedProgram']==0
  path=ROOT/'local/stage05/music-cinema-runtime.entropy';assert c.call('save-profile',path=str(path))['ok']
  doc=json.loads(path.read_text());assert doc['stage3_inputs']['version']==1
  bad=ROOT/'local/stage05/music-cinema-runtime-bad.entropy';doc['stage3_inputs']['music']='\\\\remote\\pipe\\bad';bad.write_text(json.dumps(doc))
  assert not c.call('load-profile',path=str(bad))['ok'];assert c.call('telemetry')['data']['selectedProgram']==0
  stop(p,c);p=c=None
  # Legacy saved Cinema stays selected while its decoder is absent.
  p,c=run(['--offline-test-signal']);assert c.call('program',program=1)['ok'];wait(c,lambda s:s['program']==1 and int(s['crossoverApplied'])>0)
  legacy=ROOT/'local/stage05/music-cinema-legacy.entropy';assert c.call('save-profile',path=str(legacy))['ok'];stop(p,c);p=c=None
  p,c=run(['--stage2-input','--input-only','--profile',str(legacy),'--music-endpoint',music_endpoint])
  s=wait(c,lambda s:s['selectedProgram']==1);assert s['inputState']=='WAITING CINEMA' and not s['armed'];assert int(s['blocks'])==0
  assert c.call('program',program=0)['ok'];wait(c,lambda s:int(s['blocks'])>10)
  stop(p,c);p=c=None
  # Both synthetic sources: preserve native transition and fail locally on source loss.
  cinema=subprocess.Popen([publisher,'6','22','20'],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True);cinema_endpoint=cinema.stdout.readline().strip()
  p,c=run(['--stage2-input','--input-only','--music-endpoint',music_endpoint,'--cinema-endpoint',cinema_endpoint])
  wait(c,lambda s:s['inputs']['cinema']['state']=='RECEIVING' and int(s['blocks'])>10)
  assert c.call('program',program=1)['ok'];s=wait(c,lambda s:s['selectedProgram']==1 and int(s['crossoverApplied'])>=int(s['crossoverAccepted']))
  assert c.call('program',program=0)['ok'];wait(c,lambda s:s['selectedProgram']==0 and int(s['crossoverApplied'])>=int(s['crossoverAccepted']))
  assert c.call('program',program=1)['ok'];s=wait(c,lambda s:s['selectedProgram']==1 and int(s['crossoverApplied'])<int(s['crossoverAccepted']))
  cinema.terminate();cinema.communicate(timeout=5);cinema=None
  s=wait(c,lambda s:s['inputs']['cinema']['state']=='FAULT');assert not s['armed']
  before=int(s['blocks']);assert c.call('program',program=0)['ok'];wait(c,lambda v:v['selectedProgram']==0 and int(v['blocks'])>before+10)
  print('PASS process/IPC: Music-only, absent Cinema refusal, parameters, transactional profiles, saved Cinema waiting, dual transition and local source loss')
 finally:
  if c:stop(p,c)
  for proc in (music,cinema):
   if proc:proc.terminate();proc.communicate(timeout=5)
if __name__=='__main__':main()

