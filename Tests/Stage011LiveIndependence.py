"""Authorized real source lifecycle check. No source commands and no physical ARM.

Launch a new Stage1 release once, then stop/restart/kill only owned Stage2/Core
children. Source snapshots are read-only files, not producer ring cursors.
On success leave Stage1 visible and the final Stage2/Core DISARMED for handover.
"""
import argparse, json, os, pathlib, subprocess, time, uuid
from Stage05IpcIntegration import Client


def main():
    parser = argparse.ArgumentParser()
    for name in ('source', 'stage2', 'core', 'profile', 'output'):
        parser.add_argument('--'+name, required=True)
    args = parser.parse_args()
    output = pathlib.Path(args.output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    source_status = output/'source.json'
    source_exe, stage2_exe, core_exe = [pathlib.Path(x).resolve() for x in (args.source, args.stage2, args.core)]
    env = dict(os.environ, QT_QPA_PLATFORM='windows')
    rows, children, handles = [], [], []
    source = stage2 = core = client = None
    stage2_diagnostic = stage2_stop = None
    source_id = None
    core_endpoint = None
    success = False

    def launch(executable, arguments, label):
        stdout = (output/(label+'.stdout.txt')).open('w', encoding='utf-8')
        stderr = (output/(label+'.stderr.txt')).open('w', encoding='utf-8')
        handles.extend((stdout, stderr))
        child = subprocess.Popen([str(executable)]+arguments, cwd=str(executable.parent), env=env,
                                 stdout=stdout, stderr=stderr)
        children.append(child)
        return child

    def read_json(path):
        try:
            return json.loads(path.read_text(encoding='utf-8-sig'))
        except (FileNotFoundError, json.JSONDecodeError):
            return None

    def await_condition(check, label, seconds=12):
        deadline = time.monotonic()+seconds
        while time.monotonic()<deadline:
            value = check()
            if value:
                return value
            time.sleep(.04)
        raise AssertionError(label)

    def source_snapshot(label, advance=True):
        nonlocal source_id
        assert source.poll() is None, 'Stage1 process exited'
        state = await_condition(lambda: read_json(source_status), 'Stage1 status unavailable')
        assert state['faults']==0, state
        identity = state['session']
        if source_id is None:
            source_id = identity
        assert identity==source_id, 'Stage2 lifecycle rotated source UUID'
        if rows and advance:
            old = rows[-1]['source']['blocks']
            state = await_condition(lambda: (s if (s:=read_json(source_status)) and s['blocks']>old+20 else None),
                                    'Stage1 processing stalled')
            assert state['session']==source_id and state['faults']==0, state
        row = {'phase':label, 'monotonicSeconds':time.monotonic(), 'source':state}
        if stage2 and stage2.poll() is None:
            diagnostic = read_json(stage2_diagnostic)
            assert diagnostic and diagnostic['source_session_id']==source_id, diagnostic
            assert diagnostic['state']==2 and diagnostic['faults']==0, diagnostic
            row['stage2'] = diagnostic
        if client:
            telemetry = client.call('telemetry')['data']
            assert not telemetry['armed'], 'Test must stay DISARMED'
            assert all(domain['state']=='DISARMED' for domain in telemetry['domains'])
            row['core'] = {key:telemetry[key] for key in ('blocks','armed','stage2','musicPeaks')}
        rows.append(row)
        print(label, 'Stage1 blocks', state['blocks'], 'UUID', source_id, flush=True)

    def launch_stage2(label):
        nonlocal stage2, stage2_diagnostic, stage2_stop
        stage2_diagnostic = output/(label+'.json')
        stage2_stop = output/(label+'.stop')
        assert not stage2_stop.exists(), 'Use a fresh evidence folder'
        stage2 = launch(stage2_exe, ['--headless','--automatic','--runtime-root',str(stage2_exe.parent),
                                    '--diagnostic-file',str(stage2_diagnostic),'--stop-file',str(stage2_stop)],label)
        await_condition(lambda: (d if (d:=read_json(stage2_diagnostic)) and d['state']==2 and d['blocks_out']>25 else None),
                        'Stage2 did not passively receive source')
        assert stage2.poll() is None

    def launch_core(label):
        nonlocal core, client, core_endpoint
        endpoint = 'EntropyAudioCore.Stage011.'+uuid.uuid4().hex
        core_endpoint = endpoint
        core = launch(core_exe, ['--control-server','--endpoint',endpoint,'--stage2-input','--profile',
                                 str(pathlib.Path(args.profile).resolve())], label)
        client = Client(endpoint)
        telemetry = await_condition(lambda: (t if int((t:=client.call('telemetry')['data'])['blocks'])>20
                                    and t['stage2']['metersFresh'] else None), 'Core genuine source input unavailable')
        assert telemetry['stage2']['sourceSession']==source_id, telemetry['stage2']
        assert max(telemetry['musicPeaks'])>1e-8, 'A silent buffer cannot pass'
        assert not telemetry['armed']

    try:
        assert not source_status.exists(), 'Use a fresh evidence folder'
        source = launch(source_exe, ['--runtime-root',str(source_exe.parent),'--status-file',str(source_status)], 'source')
        await_condition(lambda: (s if (s:=read_json(source_status)) and s['blocks']>40 else None), 'Stage1 standalone stalled')
        source_snapshot('Stage1-alone-start')
        time.sleep(.4); source_snapshot('Stage1-alone-continues')
        launch_stage2('Stage2-first')
        source_snapshot('Stage2-passively-connected')
        stage2_stop.write_text('stop', encoding='ascii')
        stage2.wait(timeout=10)
        assert stage2.returncode==0, 'Stage2 clean stop failed'
        time.sleep(.4); source_snapshot('Stage2-clean-stop-source-continues')
        launch_stage2('Stage2-restarted')
        source_snapshot('Stage2-reconnected-without-Source-Retry')
        stage2.kill(); stage2.wait(timeout=10)
        time.sleep(.4); source_snapshot('Stage2-forced-kill-source-continues')
        launch_stage2('Stage2-after-kill')
        source_snapshot('Stage2-after-kill-same-source')
        launch_core('Core-first')
        source_snapshot('Stage3-connected-DISARMED')
        assert client.call('shutdown')['ok']; client.close(); client=None
        core.wait(timeout=10); assert core.returncode==0
        time.sleep(.4); source_snapshot('Stage3-closed-upstream-continues')
        launch_core('Core-reconnected')
        source_snapshot('Stage3-reconnected-same-source')
        result = {'status':'PASS', 'hardwareOpened':False, 'sourceCommandsSent':0,
                  'sourceSession':source_id, 'snapshots':rows, 'coreEndpoint':core_endpoint,
                  'runningPids':{'source':source.pid, 'stage2':stage2.pid, 'core':core.pid}}
        success=True
        print('PASS complete lifecycle independence, actual live FP64 input, no DAC', flush=True)
    except Exception as error:
        result = {'status':'FAIL', 'error':str(error), 'snapshots':rows}
        raise
    finally:
        if client:
            client.close()
        if not success:
            # Cleanup only this test's children. Do not manipulate any external source.
            for child in reversed(children):
                if child.poll() is None:
                    child.kill(); child.wait(timeout=10)
        for handle in handles:
            handle.close()
        (output/'live-independence.json').write_text(json.dumps(result,indent=2), encoding='utf-8')


if __name__=='__main__':
    main()
