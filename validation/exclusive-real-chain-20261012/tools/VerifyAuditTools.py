"""Focused checks for audit reader integrity and restoration control flow.
No audio clients, sources, kernel readers or physical sinks are opened.
"""
import pathlib,json,subprocess,struct,sys,datetime,uuid,os,io,contextlib
out=pathlib.Path(__file__).resolve().parents[1];repo=out.parents[1]/'Audio-bloc-IPP-Intel';build=repo/'build/msvc-music-cinema-65536';tests=out/'tool-integrity';tests.mkdir(exist_ok=True)
results=[]
def summary(folder,name,size,count=1):
    (folder/(name+'-summary.json')).write_text(json.dumps(dict(recordBytes=size,written=count,drops=0,failed=False)))
def run(name,exe,args,folder,record=None,partname=None):
    folder.mkdir(exist_ok=True)
    if record is not None:(folder/partname).write_bytes(record)
    p=subprocess.run([str(build/exe)]+[str(a) for a in args]+[str(folder)],capture_output=True,text=True)
    expected={'missing-graph':'missing graph records','truncated-graph':'truncated graph record','nonfinite-graph':'nonfinite captured/reference output','missing-dac':'missing DAC records','truncated-dac':'truncated DAC record','nonfinite-dac':'unsupported/invalid sink record'}[name]
    if p.returncode!=1 or expected not in p.stderr:raise RuntimeError(name+' must reject for intended reason: '+str(p.returncode)+' '+p.stdout+' '+p.stderr)
    results.append(dict(name=name,exitCode=p.returncode,error=p.stderr.strip()))
for name,kind in (('missing-graph','missing'),('truncated-graph','short'),('nonfinite-graph','nan')):
    folder=tests/name;folder.mkdir(exist_ok=True);summary(folder,'graph',499872)
    record=None
    if kind=='short':record=b'X'*17
    if kind=='nan':
        record=bytearray((out/'observed2/graph-0.bin').open('rb').read(499872));struct.pack_into('<d',record,160+13*1024*8,float('nan'))
    run(name,'EntropyRealChainReplay.exe',[out/'stage3-before.entropy'],folder,record,'graph-0.bin')
for name,kind in (('missing-dac','missing'),('truncated-dac','short'),('nonfinite-dac','nan')):
    folder=tests/name;folder.mkdir(exist_ok=True);summary(folder,'dac-a',196736);summary(folder,'dac-b',196736)
    record=None
    if kind=='short':record=b'X'*17
    if kind=='nan':
        record=bytearray((out/'observed2/dac-a-0.bin').open('rb').read(196736));struct.pack_into('<d',record,56,float('nan'))
    run(name,'EntropyRealChainDacReference.exe',[out/'graph-output-f64.bin'],folder,record,'dac-a-0.bin')
# Execute the actual runner's try/finally body with process/IPC stubs. The stubs
# fail before monitoring starts, so no sleep, process launch or IPC occurs.
body=(out/'tools/ObserveChain.py').read_text();body=body[body.index('tag='):]
for scenario in ('initial-shutdown-failure','candidate-launch-failure','cleanup-shutdown-and-wait-failure'):
    calls=[]
    class FakeProcess:
        pid=1234
        def poll(self):return None
        def wait(self,timeout):
            if scenario=='cleanup-shutdown-and-wait-failure' and 'terminated' not in calls:raise subprocess.TimeoutExpired('fake',timeout)
        def terminate(self):calls.append('terminated')
    def shutdown():
        calls.append('shutdown')
        if scenario=='initial-shutdown-failure' or (scenario=='cleanup-shutdown-and-wait-failure' and calls.count('shutdown')>1):raise RuntimeError('injected shutdown failure')
    def launch(exe,env=None):
        calls.append('production-launch' if exe=='production' else 'candidate-launch')
        if exe=='candidate' and scenario=='candidate-launch-failure':raise RuntimeError('injected launch failure')
        return FakeProcess(),io.StringIO()
    def Client(endpoint):raise RuntimeError('injected unavailable IPC')
    ns=dict(out=tests,datetime=datetime,uuid=uuid,os=os,subprocess=subprocess,diagnostic='candidate',production='production',shutdown=shutdown,launch=launch,Client=Client,endpoint='fake',json=json,arm=lambda c:None)
    with contextlib.redirect_stdout(io.StringIO()):
        try:exec(compile(body,'ObserveChain.py cleanup body','exec'),ns)
        except RuntimeError:pass
    if calls.count('production-launch')!=1:raise RuntimeError('Restoration attempt missing '+scenario+str(calls))
    results.append(dict(name=scenario,restorationAttempts=1,calls=calls))
(out/'tool-integrity-checks.json').write_text(json.dumps(results,indent=2));print(json.dumps(dict(checks=len(results),passed=len(results),audioDevicesOpened=0)))
