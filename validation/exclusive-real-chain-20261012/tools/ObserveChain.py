import sys,pathlib,json,time,subprocess,os,datetime,uuid
root=pathlib.Path('D:/Projects/3Stage-3');repo=root/'Audio-bloc-IPP-Intel';out=root/'Diagnostics/Exclusive-RealChain-20261012'
sys.path.insert(0,str(repo/'Tests'))
from Stage05IpcIntegration import Client
production=repo/'release/0.1.9-v2-r2/EntropyAudioCore-0.1.9-v2-r2-64k-win-x64/runtime/EntropyAudioCore.exe'
diagnostic=out/'observation-stage3/EntropyAudioCore.exe'
profile=out/'stage3-before.entropy';endpoint='EntropyAudioCore.Control.v1'
args=['--control-server','--endpoint',endpoint,'--stage2-input','--authorized-live-test','--profile',str(profile)]
keys=('armed','physicalArmed','lifecycleStatus','headroom','domains','scheduler','inputs','inputMuted','liveFault','sourceWaitCount','sourceResumeCount','matrix','crossoverPeaks','outputs')
def shutdown():
    c=Client(endpoint)
    try:
        reply=c.call('shutdown');print('Shutdown',json.dumps(reply),flush=True)
        if not reply['ok']:raise RuntimeError(reply)
    finally:c.close()
    time.sleep(1)
def launch(exe,env=None):
    log=(out/('observation-core.log' if exe==diagnostic else 'restored-core.log')).open('w')
    p=subprocess.Popen([str(exe)]+args,cwd=str(exe.parent),env=env,stdout=log,stderr=log,creationflags=subprocess.CREATE_NO_WINDOW)
    return p,log
def arm(c):
    initial=c.call('telemetry',viewContractVersion=2)
    reply=c.call('arm');print('ARM',json.dumps(reply),flush=True)
    if not reply['ok']:raise RuntimeError(reply)
    return initial
tag='observed-'+datetime.datetime.now().strftime('%H%M%S')+'-'+uuid.uuid4().hex[:8]
capture=out/tag;capture.mkdir()
env=os.environ.copy();env['ENTROPY_CHAIN_AUDIT_DIR']=str(capture)
p=None;log=None;shutdownAttempted=False
try:
    shutdownAttempted=True;shutdown()
    p,log=launch(diagnostic,env)
    c=Client(endpoint)
    try:
        initial=arm(c);(out/(tag+'-initial.json')).write_text(json.dumps(initial,indent=2))
        end=time.monotonic()+66
        with (out/(tag+'-telemetry.jsonl')).open('w') as f:
            while time.monotonic()<end:
                reply=c.call('telemetry',viewContractVersion=2)
                if not reply['ok']:raise RuntimeError(reply)
                t=reply['data'];f.write(json.dumps(dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),telemetry={k:t.get(k) for k in keys}))+'\n')
                time.sleep(.1)
        print('Observation complete',flush=True)
        print(json.dumps({k:t.get(k) for k in ('armed','physicalArmed','liveFault','scheduler')}),flush=True)
    finally:c.close()
finally:
    try:
        if p and p.poll() is None:
            try:shutdown()
            except Exception as error:print('Diagnostic shutdown error:',error,flush=True)
            try:p.wait(timeout=90)
            except subprocess.TimeoutExpired:
                # This exact subprocess handle is the diagnostic owner, never
                # another stage or an unrelated production process.
                p.terminate();p.wait(timeout=15)
    finally:
        if log:log.close()
        if shutdownAttempted:
            # A failed initial shutdown may have left production running.
            # Probe that existing owner before attempting another launch.
            existing=None
            try:
                probe=Client(endpoint)
                try:existing=probe.call('telemetry',viewContractVersion=2)['data']
                finally:probe.close()
            except Exception:pass
            if existing and (not p or int(existing.get('pid',0))!=p.pid):
                print('Existing production owner retained; no duplicate launch',flush=True)
            elif existing:
                raise RuntimeError('Diagnostic still owns endpoint; refusing duplicate production launch')
            else:
                restored,restorelog=launch(production)
                c=Client(endpoint)
                try:arm(c)
                finally:c.close();restorelog.close()
                print('Original Stage3 restored with saved profile; PID',restored.pid,flush=True)
