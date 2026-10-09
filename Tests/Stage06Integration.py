"""Real original StageBus/StageLink + Core IPC; no physical audio objects."""
import json, os, pathlib, subprocess, sys, time, uuid
sys.path.insert(0,str(pathlib.Path(__file__).parent))
from Stage05IpcIntegration import Client

def main():
    core_path=pathlib.Path(os.environ['STAGE05_CORE'])
    producer_path=core_path.parent/'EntropyStage06Input.exe'
    token=uuid.uuid4().hex
    endpoint='EntropyAudioCore.Stage06.Test.'+token
    upstream=r'\\.\pipe\Entropy.StagePublish.V2.Stage06Fixture.'+token
    fixture_endpoint=endpoint+'.Profile'
    profile=core_path.parent/('stage06-fixture-'+token+'.entropy')
    creator=subprocess.Popen([str(core_path),'--control-server','--endpoint',fixture_endpoint,'--offline-test-signal','--run-seconds','10'],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    try:
        fixture_client=Client(fixture_endpoint)
        assert fixture_client.call('save-profile',path=str(profile))['ok'];fixture_client.call('shutdown');fixture_client.close();creator.wait(timeout=8)
    finally:
        if creator.poll() is None:creator.terminate();creator.communicate(timeout=8)
    producer=subprocess.Popen([str(producer_path),'--fixture-server',upstream],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    hidden=subprocess.STARTUPINFO();hidden.dwFlags=subprocess.STARTF_USESHOWWINDOW;hidden.wShowWindow=0
    core=subprocess.Popen([str(core_path),'--control-server','--endpoint',endpoint,'--stage2-input','--profile',str(profile),'--stage2-endpoint',upstream,'--run-seconds','25'],stdout=subprocess.PIPE,stderr=subprocess.PIPE,creationflags=subprocess.CREATE_NEW_CONSOLE,startupinfo=hidden)
    client=None
    try:
        client=Client(endpoint)
        assert client.call('describe')['data']['consoleAttached'] is False,'Live Core still depends on launcher console lifetime'
        until=time.monotonic()+8
        while time.monotonic()<until:
            data=client.call('telemetry')['data']
            if int(data['stage2']['consumed'])>3:break
            time.sleep(.03)
        assert int(data['stage2']['consumed'])>3, data['stage2']
        assert data['inputState']=='FP64 SOURCE / 192 kHz / PASSIVE V2' and data['blockSize']==1024,data['inputState']
        assert max(data['musicPeaks'])>1e-8 and max(data['cinemaPeaks'][:8])>1e-8,'All-zero input is FAIL'
        assert not data['armed'] and all(d['state']=='DISARMED' for d in data['domains'])
        assert client.call('parameter',id='cinemaDelayMs1',value=3)['ok']
        assert client.call('matrix-clear')['ok']
        assert client.call('matrix-route',source=0,destination=1,enabled=True)['ok']
        time.sleep(.15)
        before=client.call('telemetry')['data'];blocks=int(before['blocks'])
        client.close();client=None;time.sleep(.2)
        assert core.poll() is None,'Core stopped when presentation client closed'
        client=Client(endpoint);after=client.call('telemetry')['data']
        assert int(after['blocks'])>blocks,'DSP stopped without UI'
        assert after['matrix']['routes'][0]==2 and after['matrix']['routeCount']==1,'Last applied state lost'
        params={p['id']:p['value'] for p in after['parameters']}
        assert abs(params['cinemaDelayMs1']-3.00006103515625)<1e-12
        print('Stage06 StageBus -> graph -> IPC nonzero MUSIC/CINEMA meters PASS')
        print('Stage06 UI detach/reconnect: same Core, advancing blocks, Crossover/Matrix state retained PASS')
        producer.terminate();producer.wait(timeout=5)
        until=time.monotonic()+4
        while time.monotonic()<until:
            data=client.call('telemetry')['data']
            if data['stage2']['producerPid']==0:break
            time.sleep(.03)
        assert data['stage2']['producerPid']==0 and not data['armed'],'Producer loss not reported safely'
        assert client.call('shutdown')['ok'];client.close();client=None
        core.wait(timeout=8);assert core.returncode==0
        print('Stage06 producer loss and explicit independent engine shutdown PASS')
    finally:
        if client:client.close()
        for process in (core,producer):
            if process.poll() is None:process.terminate();process.wait(timeout=8)
        for label,process in [('core',core),('fixture',producer)]:
            stdout,stderr=process.communicate()
            print(label+' exit: '+str(process.returncode))
            if stdout:print(label+' stdout: '+stdout.decode(errors='replace'))
            if stderr:print(label+' stderr: '+stderr.decode(errors='replace'))
if __name__=='__main__':main()
