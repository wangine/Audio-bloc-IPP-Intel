"""Explicit live-source lifecycle proof. Owns only its DISARMED Core children.

Reads Stage1 mapping FILE_MAP_READ; never sends source commands or advances cursors.
Never stops/restarts Stage1/Stage2 and never opens a DAC.
"""
import argparse, base64, ctypes as C, json, pathlib, struct, subprocess, time, uuid
from Stage05IpcIntegration import Client
K=C.WinDLL('kernel32',use_last_error=True)
K.OpenFileMappingW.argtypes=[C.c_uint32,C.c_int,C.c_wchar_p];K.OpenFileMappingW.restype=C.c_void_p
K.MapViewOfFile.argtypes=[C.c_void_p,C.c_uint32,C.c_uint32,C.c_uint32,C.c_size_t];K.MapViewOfFile.restype=C.c_void_p
K.UnmapViewOfFile.argtypes=[C.c_void_p];K.CloseHandle.argtypes=[C.c_void_p]

def main():
    p=argparse.ArgumentParser();p.add_argument('--core',required=True);p.add_argument('--profile',required=True)
    p.add_argument('--source-discovery',required=True);p.add_argument('--stage2-diagnostics',required=True);p.add_argument('--output',required=True)
    a=p.parse_args();out=pathlib.Path(a.output);out.mkdir(parents=True,exist_ok=True)
    rows=[];baseline=None;children=[];client=None;process=None
    def snapshot(label):
        nonlocal baseline
        discovery=json.loads(pathlib.Path(a.source_discovery).read_text(encoding='utf-8-sig'))
        h=base64.b64decode(discovery['session_header_base64'],validate=True);sid=h[16:32].hex();layout=struct.unpack_from('<I',h,32)[0]
        handle=K.OpenFileMappingW(4,0,'Local\\Entropy.StageBus.V1.'+sid+'.'+str(layout)+'.Mapping')
        assert handle,'Source read-only mapping unavailable'
        view=K.MapViewOfFile(handle,4,0,0,1344)
        try:
            assert view;raw=C.string_at(view,1344)
            assert raw[16:32]==h[16:32]
            counters={name:struct.unpack_from('<Q',raw,offset)[0] for name,offset in [('blocksPublished',256),('blocksConsumed',320),('producerHeartbeatQpc',384),('requestWrite',512),('fault',1088)]}
        finally:
            if view:K.UnmapViewOfFile(view)
            K.CloseHandle(handle)
        d=json.loads(pathlib.Path(a.stage2_diagnostics).read_text(encoding='utf-8-sig'))
        if baseline is None:baseline=sid
        assert sid==baseline==d['source_session_id'],'Source UUID changed'
        assert counters['fault']==0 and d['faults']==0 and d['state']==2,(counters,d)
        row={'phase':label,'monotonicSeconds':time.monotonic(),'sourceSession':sid,'stage1':counters,
             'stage2':{k:d[k] for k in ('blocks_in','blocks_out','frames_out','publish_drops','snapshot_tick_ms','faults')}}
        if client:
            t=client.call('telemetry')['data'];assert not t['armed']
            row['core']={'blocks':t['blocks'],'inputState':t['inputState'],'input':t['stage2'],'musicPeaks':t['musicPeaks']}
        if rows:
            assert counters['blocksPublished']>rows[-1]['stage1']['blocksPublished'],'Stage1 delivery stalled'
            assert d['blocks_out']>rows[-1]['stage2']['blocks_out'],'Stage2 processing stalled'
        rows.append(row);print(label,'Stage1',counters['blocksPublished'],'Stage2',d['blocks_out'],'UUID',sid,flush=True)
    def launch():
        nonlocal process,client
        endpoint='EntropyAudioCore.Stage010.LiveProof.'+uuid.uuid4().hex
        process=subprocess.Popen([str(pathlib.Path(a.core).resolve()),'--control-server','--endpoint',endpoint,'--stage2-input',
                                  '--profile',str(pathlib.Path(a.profile).resolve())],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        children.append(process);client=Client(endpoint);deadline=time.monotonic()+8
        while time.monotonic()<deadline:
            t=client.call('telemetry')['data']
            if int(t['blocks'])>10 and t['stage2']['metersFresh']:break
            time.sleep(.03)
        assert int(t['blocks'])>10 and t['stage2']['metersFresh'],t['stage2']
        assert t['stage2']['sourceSession']==baseline and max(t['musicPeaks'])>1e-9,'No genuine source signal'
        assert not t['armed'] and all(x['state']=='DISARMED' for x in t['domains'])
    try:
        snapshot('source-running-without-Core-before');time.sleep(2);snapshot('source-without-Core-2s')
        launch();time.sleep(1);snapshot('Core-first-start-DISARMED');assert client.call('disarm')['ok']
        assert client.call('shutdown')['ok'];client.close();client=None;process.wait(timeout=8);assert process.returncode==0
        time.sleep(2);snapshot('Core-clean-stop-absent-2s')
        launch();time.sleep(1);snapshot('Core-restart-same-source')
        process.kill();process.wait(timeout=8);client.close();client=None
        time.sleep(2);snapshot('Core-forced-kill-absent-2s')
        launch();time.sleep(1);snapshot('Core-after-kill-reconnect-same-source')
        assert client.call('shutdown')['ok'];client.close();client=None;process.wait(timeout=8);assert process.returncode==0
        time.sleep(1);snapshot('final-source-running-no-cleanup')
        print('PASS live-source isolation: clean stop/restart/forced kill; same source UUID, both upstream counters advance; no DAC')
        result={'status':'PASS','sourceSession':baseline,'hardwareOpened':False,'snapshots':rows}
    except Exception as e:
        result={'status':'FAIL','error':str(e),'snapshots':rows};raise
    finally:
        if client:client.close()
        for child in children:
            if child.poll() is None:child.kill();child.wait(timeout=8)
        (out/'live-isolation.json').write_text(json.dumps(result,indent=2),encoding='utf-8')

if __name__=='__main__':main()
