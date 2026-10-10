"""Targeted command admission regression; independent offline endpoint only."""
import json, os, subprocess, time, uuid
from Stage05IpcIntegration import Client

core = os.environ['STAGE05_CORE']
endpoint = 'EntropyAudioCore.Stage017.' + uuid.uuid4().hex
process = subprocess.Popen([core, '--control-server', '--endpoint', endpoint,
                            '--offline-test-signal'], stdout=subprocess.DEVNULL)
try:
    client = Client(endpoint)
    start = time.perf_counter()
    reply = client.call('program', _admission_only=True, program=1)
    admission_ms = (time.perf_counter() - start) * 1000
    assert reply['ok'], reply
    assert reply['data'].get('status') == 'ACCEPTED', ('command reader still applies synchronously', reply)
    identity = reply['data']['commandId']
    deadline = time.monotonic() + 5
    while True:
        result = client.call('command-results', ids=[identity])['data']['results'][0]
        if result.get('complete'):
            assert result['reply']['ok'], result
            break
        assert time.monotonic() < deadline, 'preparation did not complete'
        time.sleep(.01)
    telemetry = client.call('telemetry')['data']
    assert telemetry['benchmarkCalls'] == '0', 'normal control invoked qualification'
    rejected = client.call('preflight-live')
    assert not rejected['ok'], 'qualification remains accessible in live server'
    missing = client.call('load-profile', path='stage017-nonexistent-fixture.entropy')
    assert not missing['ok'], missing
    assert client.call('telemetry')['data']['lifecycleStatus'] == 'DISARMED', 'failed load left STOPPING'
    disarm = client.call('disarm', _admission_only=True)
    assert disarm['ok'] and disarm['data']['status'] == 'ACCEPTED', disarm
    print(json.dumps({'PASS': True, 'programAdmissionMs': admission_ms, 'benchmarkCalls': 0}))
    client.call('shutdown'); client.close()
finally:
    try: process.wait(timeout=7)
    except subprocess.TimeoutExpired: process.kill(); process.wait()
