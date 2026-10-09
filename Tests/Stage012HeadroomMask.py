"""Real Core: configured physical lanes drive the detector even while DISARMED."""
import os,pathlib,json,subprocess,tempfile,time
from Stage05IpcIntegration import Client
exe=os.environ['STAGE05_CORE'];endpoint='EntropyAudioCore.Stage012.'+str(os.getpid())
process=subprocess.Popen([exe,'--control-server','--endpoint',endpoint,'--offline-test-signal'],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
client=None
try:
    client=Client(endpoint)
    with tempfile.TemporaryDirectory(prefix='stage012-') as directory:
        path=pathlib.Path(directory)/'fixture.entropy'
        assert client.call('save-profile',path=str(path))['ok']
        profile=json.loads(path.read_text())
        profile['stage05_devices']={'bindings':[{'logicalId':'DAC_A','endpoint':'offline-mask-fixture','name':'offline','fingerprint':'fixture','approvalRevision':'1','evidence':[],'formats':[]},None],
            'configuration':[{'enabled':True,'logicalId':'DAC_A','endpoint':'offline-mask-fixture','approvalRevision':'1','rate':192000,'channels':2,'format':0,'lanes':[0,2]},{'enabled':False}]}
        path.write_text(json.dumps(profile))
        reply=client.call('load-profile',path=str(path));assert reply['ok'],reply
        t=client.call('telemetry')['data']
        assert t['headroom']['activeChannelMask']==5,'Physical configuration ignored by Headroom detector'
        assert t['headroom']['renderLaneMask']==5
        assert not t['armed']
        assert client.call('disarm')['ok']
        assert client.call('telemetry')['data']['headroom']['activeChannelMask']==5,'DISARM changed Graph detector mask'
        for fields in ({'action':'headroom-groups','value':2},{'action':'headroom-channel-group','channel':0,'group':1}):
            action=fields.pop('action');assert not client.call(action,**fields)['ok'],'Incompatible global group API accepted'
        # Lane 2 loses its last valid matrix contributor at one block boundary.
        assert client.call('matrix-route',source=2,destination=2,enabled=False)['ok']
        end=time.monotonic()+3
        while time.monotonic()<end:
            t=client.call('telemetry')['data']
            if t['headroom']['activeChannelMask']==1:break
            time.sleep(.01)
        assert t['headroom']['activeChannelMask']==1 and t['headroom']['renderLaneMask']==5,'Matrix intersection not adopted'
        assert client.call('clear-binding',slot=0)['ok']
        end=time.monotonic()+3
        while time.monotonic()<end:
            t=client.call('telemetry')['data']
            if t['headroom']['activeChannelMask']==0:break
            time.sleep(.01)
        assert t['headroom']['activeChannelMask']==0 and t['headroom']['renderLaneMask']==0
        assert not t['armed']
    print('PASS physical mask / Matrix intersection / DISARM invariance / binding clear / explicit group rejection; no DAC')
finally:
    if client:client.close()
    process.terminate();process.communicate(timeout=10)
