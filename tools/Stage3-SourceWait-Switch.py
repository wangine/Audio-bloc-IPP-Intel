"""Switch only the explicitly identified local Core, using graceful IPC shutdown."""
import argparse,json,pathlib,subprocess,sys,time,uuid
ROOT=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'Tests'))
from Stage05IpcIntegration import Client
p=argparse.ArgumentParser()
p.add_argument('--old-bundle',type=pathlib.Path,required=True)
p.add_argument('--new-bundle',type=pathlib.Path,required=True)
p.add_argument('--profile',type=pathlib.Path,required=True)
p.add_argument('--save-active-profile',action='store_true')
p.add_argument('--record',type=pathlib.Path,required=True)
a=p.parse_args()
if a.record.exists():raise RuntimeError('Preserve switch evidence')
endpoint='EntropyAudioCore.Control.v1'
old=str((a.old_bundle/'runtime/EntropyAudioCore.exe').resolve())
def ps(code):
 path=a.record.parent/('switch-helper-'+uuid.uuid4().hex+'.txt')
 with path.open('x',encoding='utf-8') as log:r=subprocess.run(['powershell.exe','-NoLogo','-NoProfile','-Command',code],stdout=log,stderr=subprocess.STDOUT,timeout=20)
 text=path.read_text(encoding='utf-8',errors='replace').strip()
 if r.returncode:raise RuntimeError(text)
 return text
owners=json.loads(ps("$expected='"+old+"'; $matches=@(Get-CimInstance Win32_Process -Filter \"name='EntropyAudioCore.exe'\"|Where-Object {$_.ExecutablePath -eq $expected -and $_.CommandLine -match '--endpoint EntropyAudioCore.Control.v1( |$)'}); if($matches.Count -ne 1){throw 'Core identity ambiguous'}; $owned=Get-Process -Id $matches[0].ProcessId; [ordered]@{pid=$owned.Id;startTicks=[string]$owned.StartTime.ToUniversalTime().Ticks;path=$owned.Path}|ConvertTo-Json"))
client=Client(endpoint)
before=client.call('telemetry',viewContractVersion=2)['data']
if before['pid']!=owners['pid']:raise RuntimeError('Pipe owner differs from selected Core')
if a.save_active_profile:
 if a.profile.exists():raise RuntimeError('Preserve existing profile')
 a.profile.parent.mkdir(parents=True,exist_ok=True)
 reply=client.call('save-profile',path=str(a.profile.resolve()))
 if not reply['ok']:raise RuntimeError(reply)
if not a.profile.exists():raise RuntimeError('Validated profile required')
record=dict(oldOwner=owners,before=before,profile=str(a.profile.resolve()))
a.record.write_text(json.dumps(record,indent=2),encoding='utf-8')
reply=client.call('shutdown')
if not reply['ok']:raise RuntimeError(reply)
client.close()
ps("$owned=Get-Process -Id "+str(owners['pid'])+" -ErrorAction SilentlyContinue; if($owned){if($owned.Path -ne '"+old+"' -or [string]$owned.StartTime.ToUniversalTime().Ticks -ne '"+owners['startTicks']+"'){throw 'Core identity changed'}; if(!$owned.WaitForExit(10000)){throw 'Graceful Core shutdown timed out'}}; Write-Output 'Old Core stopped'")
bundle=str(a.new_bundle.resolve());exe=str((a.new_bundle/'runtime/EntropyAudioCore.exe').resolve());profile=str(a.profile.resolve())
log=a.record.with_suffix('.core.txt');error=a.record.with_suffix('.core-errors.txt')
print(ps("$started=Start-Process -FilePath '"+exe+"' -ArgumentList @('--control-server','--endpoint','"+endpoint+"','--stage2-input','--authorized-live-test','--profile','\""+profile+"\"') -WorkingDirectory '"+bundle+"' -WindowStyle Hidden -PassThru -RedirectStandardOutput '"+str(log.resolve())+"' -RedirectStandardError '"+str(error.resolve())+"'; Write-Output ('Candidate Core PID '+$started.Id)"),flush=True)
client=Client(endpoint)
after=client.call('telemetry',viewContractVersion=2)['data'];description=client.call('describe')['data'];client.close()
if description['version']!='0.1.9-v2-r2' or after['armed'] or description['profileRequired']:raise RuntimeError('New candidate did not start disarmed with validated profile')
record.update(after=after,description=description)
a.record.write_text(json.dumps(record,indent=2),encoding='utf-8')
print(json.dumps(dict(switched=True,oldPid=owners['pid'],newPid=after['pid'],profileRequired=description['profileRequired'])),flush=True)
