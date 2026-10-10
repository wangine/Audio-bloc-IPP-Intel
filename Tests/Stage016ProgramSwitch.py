"""Validate a saved scene on a real offline Core without changing its live state."""
import os, pathlib, subprocess, sys, uuid
sys.path.insert(0, str(pathlib.Path(__file__).parent))
from Stage05IpcIntegration import Client
from DiagnosticQualification import qualify

endpoint = 'EntropyAudioCore.Stage016.' + uuid.uuid4().hex
core = subprocess.Popen([os.environ['STAGE05_CORE'], '--control-server', '--endpoint', endpoint,
                         '--offline-test-signal', '--run-seconds', '90'],
                        stdout=subprocess.PIPE, stderr=subprocess.PIPE)
c = None
try:
    c = Client(endpoint)
    assert c.call('headroom-policy', mode=1, manualGainDb=0)['ok']
    change = {'action': 'parameter', 'id': 'cinemaSource', 'value': 1}
    before = c.call('telemetry')['data']
    result = qualify(c, change)
    assert result['ok'], result
    assert result['data'].get('program') == 1, ('target CINEMA was not validated', result)
    assert result['data'].get('sourceWarmupBlocks', 0) >= 940, ('source scene was not made active before transition', result)
    assert result['data'].get('transitionBlocks', 0) >= 20 and result['data']['transitionDeadlineMisses'] == 0, ('native source-to-target crossfade was not validated', result)
    assert result['data']['deadlineMisses'] == 0 and result['data']['maximumOutputPeak'] > 1e-12
    after = c.call('telemetry')['data']
    assert before['program'] == after['program'] == 0, 'preview changed live scene'
    assert before['headroom']['mode'] == after['headroom']['mode'] == 1
    assert before['headroom']['manualGainDb'] == after['headroom']['manualGainDb'] == 0
    for request in [{'action': 'program', 'program': 1},
                    {'action': 'parameters', 'values': [{'id': 'cinemaSource', 'value': 1}]}]:
        result = qualify(c, request)
        assert result['ok'] and result['data']['program'] == 1, result
    assert c.call('program', program=1)['ok']
    values = [{'id': prefix + str(channel), 'value': value}
              for channel in range(1, 17) for prefix, value in [('firLength', 2), ('processMode', 1)]]
    assert c.call('parameters', values=values)['ok']
    assert c.call('program', program=0)['ok']
    assert c.call('telemetry')['data']['program'] == 0
    for request in [change, {'action': 'program', 'program': 1},
                    {'action': 'parameters', 'values': [{'id': 'cinemaSource', 'value': 1}]}]:
        rejected = qualify(c, request)
        assert not rejected['ok'] and 'FIR8193 x16' in rejected['error'], rejected
        assert c.call('telemetry')['data']['program'] == 0
    invalid = qualify(c, {'action': 'parameter', 'id': 'cinemaSource', 'value': 2})
    assert not invalid['ok'], invalid
    assert not c.call('telemetry')['data']['armed']
    print('PASS target-scene preview, command aliases, explicit diagnostic, unchanged live scene/Headroom, saved FIR8193 x16 rejection; no hardware')
    assert c.call('shutdown')['ok']
    c.close(); c = None
    core.wait(timeout=8)
    assert core.returncode == 0
finally:
    if c: c.close()
    if core.poll() is None: core.terminate()
    out, err = core.communicate(timeout=8)
    if err: print(err.decode(errors='replace'))
