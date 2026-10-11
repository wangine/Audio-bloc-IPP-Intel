"""Missing/invalid mother profile keeps Core waiting for a validated IPC selection."""
import os, pathlib, subprocess, tempfile, time, uuid
from Stage05IpcIntegration import Client

def main():
    core_path=pathlib.Path(os.environ['STAGE05_CORE'])
    endpoint='EntropyAudioCore.Stage06.Profile.'+uuid.uuid4().hex
    missing=core_path.parent/'stage06-no-such-profile.entropy'
    absent=r'\\.\pipe\Entropy.StagePublish.V2.Stage06.Absent.'+uuid.uuid4().hex
    core=subprocess.Popen([str(core_path),'--control-server','--endpoint',endpoint,'--stage2-input','--input-only','--stage2-endpoint',absent,'--profile',str(missing),'--run-seconds','20'],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    client=None;creator=None;fixture_client=None
    try:
        client=Client(endpoint)
        descriptor=client.call('describe')['data']
        assert descriptor['profileRequired'] and descriptor['profileError']
        telemetry=client.call('telemetry')['data']
        assert int(telemetry['inputs']['music']['received'])==0 and not telemetry['armed']
        assert not client.call('parameter',id='cinemaSource',value=1)['ok']
        assert not client.call('load-profile',path=str(missing))['ok']
        fixture_endpoint=endpoint+'.Fixture'
        creator=subprocess.Popen([str(core_path),'--control-server','--endpoint',fixture_endpoint,'--offline-test-signal','--run-seconds','15'],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        fixture_client=Client(fixture_endpoint)
        profile=core_path.parent/'stage06-test-profile.entropy'
        assert fixture_client.call('save-profile',path=str(profile))['ok']
        fixture_client.call('shutdown');fixture_client.close();fixture_client=None
        creator.wait(timeout=8)
        assert client.call('load-profile',path=str(profile))['ok']
        assert not client.call('describe')['data']['profileRequired']
        assert client.call('parameter',id='cinemaDelayMs1',value=3.5)['ok']
        assert client.call('matrix-clear')['ok']
        assert client.call('matrix-route',source=1,destination=2,enabled=True)['ok']
        assert client.call('headroom-policy',mode=2,manualGainDb=-18)['ok']
        saved=core_path.parent/'stage06-starved-profile.entropy'
        assert client.call('save-profile',path=str(saved))['ok'],'Save after edits without producer failed'
        before=client.call('telemetry')['data'];assert int(before['blocks'])==0,'Invented samples advanced DSP'
        assert before['matrix']['routes'][1]==4 and before['headroom']['manualGainDb']==-18
        expected={p['id']:p['value'] for p in before['parameters']}['cinemaDelayMs1']
        assert client.call('matrix-clear')['ok']
        assert client.call('parameter',id='cinemaDelayMs1',value=7)['ok']
        assert client.call('load-profile',path=str(saved))['ok'],'Load after edits without producer failed'
        after=client.call('telemetry')['data']
        assert after['matrix']['routes'][1]==4 and after['headroom']['manualGainDb']==-18
        assert {p['id']:p['value'] for p in after['parameters']}['cinemaDelayMs1']==expected
        assert int(after['blocks'])==0
        print('Stage06 absent producer -> Crossover/Matrix/Headroom edits -> save/load without advancing DSP histories PASS')
        client.call('shutdown');client.close();client=None;core.wait(timeout=8)
        assert core.returncode==0
        print('Stage06 missing profile -> Core waits -> native validation -> IPC profile load PASS')
        core=subprocess.Popen([str(core_path),'--control-server','--endpoint',endpoint,'--stage2-input','--input-only','--stage2-endpoint',absent,'--profile',str(missing),'--run-seconds','20'],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        client=Client(endpoint);new_profile=core_path.parent/'stage06-new-profile.entropy'
        assert client.call('new-profile',path=str(new_profile))['ok']
        telemetry=client.call('telemetry')['data']
        assert not telemetry['profileRequired'] and telemetry['headroom']['mode']==2 and telemetry['headroom']['manualGainDb']==-6
        assert new_profile.is_file() and not telemetry['armed']
        client.call('shutdown');client.close();client=None;core.wait(timeout=8);assert core.returncode==0
        print('Stage06 new PC -> native new profile, explicit Manual Safe -6 dB, all DACs DISARMED PASS')
    finally:
        for connection in (client,fixture_client):
            if connection:connection.close()
        for process in (core,creator):
            if process and process.poll() is None:process.terminate();process.communicate(timeout=8)
if __name__=='__main__':main()
