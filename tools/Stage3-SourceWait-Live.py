"""Authorized local lifecycle audit; no signal generation or PCM modification."""
import argparse,datetime,json,pathlib,subprocess,sys,time,uuid
ROOT=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'Tests'))
from Stage05IpcIntegration import Client
p=argparse.ArgumentParser()
p.add_argument('--endpoint',default='EntropyAudioCore.Control.v1')
p.add_argument('--output',type=pathlib.Path,required=True)
p.add_argument('--steady-seconds',type=int,default=30)
p.add_argument('--reset-arm',action='store_true')
a=p.parse_args()
if a.output.exists():raise RuntimeError('Preserve existing live evidence')
a.output.parent.mkdir(parents=True,exist_ok=True)
stage1=pathlib.Path('D:/Projects/3Stage-3/Stage1-v0003/release/stage1-native-music-v0003-r7/Stage1-Launch.ps1')
stage2=pathlib.Path('D:/Projects/3Stage-3/Stage2/release/stage2-music-v0009')
def run(args):
 # Child audio processes may inherit handles. Real files avoid communicate()
 # waiting forever for a descendant to close a captured stdout/stderr pipe.
 path=a.output.parent/('helper-'+uuid.uuid4().hex+'.txt')
 with path.open('x',encoding='utf-8') as log:r=subprocess.run(args,stdout=log,stderr=subprocess.STDOUT,timeout=20)
 text=path.read_text(encoding='utf-8',errors='replace').strip()
 if r.returncode:raise RuntimeError(text)
 return text
def ps(code):return run(['powershell.exe','-NoLogo','-NoProfile','-Command',code])
def source1(action):
 print(run(['powershell.exe','-NoLogo','-NoProfile','-ExecutionPolicy','Bypass','-File',str(stage1),'-Action',action,'-NoUI']),flush=True)
def source2(action):
 exe=str(stage2/'EntropyDspEngine.exe').replace('/','\\')
 if action=='Stop':
  code="$expected='"+exe+"'; $owners=@(Get-CimInstance Win32_Process -Filter \"name='EntropyDspEngine.exe'\"|Where-Object {$_.ExecutablePath -eq $expected}); if($owners.Count -ne 1){throw 'Stage2 owner ambiguous'}; $owned=Get-Process -Id $owners[0].ProcessId; if($owned.CloseMainWindow()){if(!$owned.WaitForExit(10000)){throw 'Stage2 graceful stop failed'}; Write-Output 'Stage2 stopped gracefully'}else{Stop-Process -Id $owned.Id; if(!$owned.WaitForExit(10000)){throw 'Stage2 termination failed'}; Write-Output 'Stage2 identified owner had no closeable window; abrupt-stop recovery test'}"
 else:
  code="$expected='"+exe+"'; $owners=@(Get-CimInstance Win32_Process -Filter \"name='EntropyDspEngine.exe'\"|Where-Object {$_.ExecutablePath -eq $expected}); if($owners.Count){throw 'Stage2 already running'}; & $expected --verify-only; if($LASTEXITCODE){throw 'Stage2 package verification failed'}; $started=Start-Process -FilePath $expected -ArgumentList @('--runtime-root','\""+str(stage2)+"\"') -WorkingDirectory '"+str(stage2)+"' -WindowStyle Hidden -PassThru; Write-Output ('Stage2 started PID '+$started.Id)"
 result=ps(code);events.append(dict(stage2Action=action,detail=result));print(result,flush=True)
client=Client(a.endpoint)
meta=client.call('describe')['data']
if meta.get('version')!='0.1.9-v2-r2' or not meta.get('authorizedLiveTest'):raise RuntimeError('Expected authorized local source-wait candidate')
events=[];core_pid=None;stage1_stopped=False;stage2_stopped=False
with a.output.open('x',encoding='utf-8') as log:
 def read(event='sample'):
  global core_pid
  reply=client.call('telemetry',viewContractVersion=2)
  if not reply['ok']:raise RuntimeError(reply)
  data=reply['data']
  if core_pid is None:core_pid=data['pid']
  if data['pid']!=core_pid:raise RuntimeError('Stage3 Core changed during audit')
  keep=('pid','version','lifecycleStatus','armed','physicalArmed','inputMuted','liveFault','armPending','blocks','sourceWaitCount','sourceResumeCount','lastSourceWaitCause','inputResumeError','inputState','program','inputs','domains','faults')
  row=dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),event=event,telemetry={k:data.get(k) for k in keep})
  log.write(json.dumps(row,allow_nan=False)+'\n');log.flush()
  return data
 def until(predicate,event,seconds=30):
  deadline=time.monotonic()+seconds
  while True:
   data=read(event)
   if predicate(data):return data
   if time.monotonic()>deadline:raise RuntimeError('Timeout: '+event+'; '+json.dumps({k:data.get(k) for k in ('lifecycleStatus','armed','physicalArmed','inputMuted','liveFault','inputResumeError')}))
   time.sleep(.2)
 def live(data):return data['armed'] and data['physicalArmed'] and data['lifecycleStatus']=='RUNNING' and not data['liveFault']
 def waiting(data):return data['armed'] and data['inputMuted'] and not data['physicalArmed'] and not data['liveFault'] and data['lifecycleStatus']=='WAITING_INPUT'
 def command(action,**kw):
  reply=client.call(action,**kw)
  if not reply['ok']:raise RuntimeError(reply)
  events.append(dict(action=action,reply=reply));return reply
 def hold(event,seconds=5):
  until(waiting,event);time.sleep(.2);base=read(event+'-settled');deadline=time.monotonic()+seconds
  while time.monotonic()<deadline:
   data=read(event+'-held')
   if not waiting(data) or data['blocks']!=base['blocks']:raise RuntimeError('Waiting lost ARM or manufactured Graph blocks')
   for old,new in zip(base['domains'],data['domains']):
    if new['physicalSinkOpen']:raise RuntimeError('DAC owner remains open while waiting')
    for key in ('submittedFrames','callbacks','pcmProcessedFrames','nonzeroPcmBlocks'):
     if new.get(key)!=old.get(key):raise RuntimeError('False PCM/owner activity while waiting: '+key)
   time.sleep(.2)
  events.append(dict(event=event,heldSeconds=seconds,blocks=base['blocks'],corePid=core_pid))
 def steady(event,seconds):
  deadline=time.monotonic()+seconds
  while time.monotonic()<deadline:
   if not live(read(event)):raise RuntimeError('Unexpected output interruption: '+event)
   time.sleep(.2)
 try:
  initial=until(lambda d:d['inputs']['music']['state']=='RECEIVING','initial-valid-input')
  if a.reset_arm and initial['armed']:
   command('disarm');initial=until(lambda d:not d['armed'] and not d['physicalArmed'],'audit-explicit-disarm')
  if initial['armed'] or initial['physicalArmed']:raise RuntimeError('Initial automatic ARM detected')
  command('arm');until(live,'initial-playback');steady('initial-steady',5)
  first_uuid=read()['inputs']['music']['sourceSession']
  source1('Stop');stage1_stopped=True;hold('stage1-stopped')
  source1('Start');stage1_stopped=False;resumed=until(live,'stage1-resumed')
  if resumed['inputs']['music']['sourceSession']==first_uuid:raise RuntimeError('Expected fresh Stage2 publication UUID after Stage1 restart')
  steady('stage1-recovered-steady',5)
  source2('Stop');stage2_stopped=True;hold('stage2-stopped')
  source2('Start');stage2_stopped=False;until(live,'stage2-resumed');steady('stage2-recovered-steady',5)
  source1('Stop');stage1_stopped=True;hold('before-explicit-disarm',2)
  command('disarm');until(lambda d:not d['armed'] and not d['physicalArmed'],'explicit-disarm')
  source1('Start');stage1_stopped=False;until(lambda d:d['inputs']['music']['state']=='RECEIVING','input-after-disarm')
  for _ in range(15):
   data=read('disarm-cancels-resume')
   if data['armed'] or data['physicalArmed'] or data['liveFault']:raise RuntimeError('DISARM failed to cancel resumption')
   time.sleep(.2)
  source1('Stop');stage1_stopped=True
  until(lambda d:d['inputs']['music']['state']!='RECEIVING','input-absent-before-new-arm')
  command('arm');hold('explicit-arm-without-input',3)
  source1('Start');stage1_stopped=False;until(live,'new-arm-resumed')
  cinema=client.call('program',program=1)
  if cinema['ok']:raise RuntimeError('Absent Cinema unexpectedly accepted')
  events.append(dict(event='absent-cinema-refused',reply=cinema))
  steady('final-steady',a.steady_seconds)
  final=read('PASS')
  result=dict(PASS=True,scope='Real local MUSIC Stage1/Stage2 lifecycle; no original PCM identity or ten-hour duration claim',corePid=core_pid,events=events,final=final)
  a.output.with_suffix('.summary.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
  print(json.dumps(dict(PASS=True,corePid=core_pid,waits=final['sourceWaitCount'],resumes=final['sourceResumeCount'],output=str(a.output))),flush=True)
 except Exception as error:
  read('FAILED')
  a.output.with_suffix('.summary.json').write_text(json.dumps(dict(PASS=False,error=str(error),corePid=core_pid,events=events),indent=2),encoding='utf-8')
  raise
 finally:
  if stage1_stopped:source1('Start')
  if stage2_stopped:source2('Start')
  client.close()
