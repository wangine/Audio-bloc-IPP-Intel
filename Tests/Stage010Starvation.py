"""Accepted host controls advance without audio or invented DSP samples."""
import os, pathlib, subprocess, tempfile, time, uuid
from Stage05IpcIntegration import Client

def main():
    core = pathlib.Path(os.environ['STAGE05_CORE']).resolve()
    name = 'EntropyAudioCore.Stage010.Starved.' + uuid.uuid4().hex
    # The absent endpoint is deliberately private; this never touches a live publisher.
    pipe_prefix = os.environ.get('STAGE010_PIPE_PREFIX', r'\\.\pipe\Entropy.StagePublish.V2.')
    absent = pipe_prefix + uuid.uuid4().hex
    processes = []; connections = []
    with tempfile.TemporaryDirectory(prefix='entropy-stage010-') as scratch:
        try:
            fixture = subprocess.Popen([str(core), '--control-server', '--endpoint', name+'.Fixture',
                                        '--offline-test-signal', '--run-seconds', '20'],
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            processes.append(fixture); creator = Client(name+'.Fixture'); connections.append(creator)
            profile = pathlib.Path(scratch) / 'starved.entropy'
            assert creator.call('save-profile', path=str(profile))['ok']
            creator.call('shutdown'); creator.close(); fixture.wait(timeout=8)
            target = subprocess.Popen([str(core), '--control-server', '--endpoint', name,
                                       '--stage2-input','--input-only', '--stage2-endpoint', absent,
                                       '--profile', str(profile), '--run-seconds', '20'],
                                      stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            processes.append(target); client = Client(name); connections.append(client)
            assert not client.call('describe')['data']['profileRequired']
            assert client.call('matrix-clear')['ok']
            assert client.call('matrix-route', source=1, destination=2, enabled=True)['ok']
            assert client.call('headroom-policy', mode=2, manualGainDb=-18)['ok']
            assert client.call('parameter', id='cinemaDelayMs1', value=3.5)['ok']
            deadline = time.monotonic()+2
            while True:
                state = client.call('telemetry')['data']
                if state['matrix']['routes'][1] == 4 and state['headroom']['manualGainDb'] == -18:
                    break
                if time.monotonic() >= deadline:
                    raise AssertionError('Accepted Matrix/Headroom controls stalled without input')
                time.sleep(.02)
            assert int(state['blocks']) == 0 and int(state['headroom']['processedFrames']) == 0
            assert int(state['inputs']['music']['received']) == 0 and not state['armed']
            print('PASS starved Core: accepted host controls applied; zero DSP frames; DACs closed')
            client.call('shutdown'); client.close(); target.wait(timeout=8)
            assert target.returncode == 0
        finally:
            for connection in connections: connection.close()
            for process in processes:
                if process.poll() is None: process.terminate()
                process.communicate(timeout=8)

if __name__ == '__main__': main()
