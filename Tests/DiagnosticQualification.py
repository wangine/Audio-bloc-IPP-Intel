"""Explicit offline subprocess only; never qualification through live IPC."""
import json, os, pathlib, subprocess, tempfile

def qualify(client, change=None):
    with tempfile.TemporaryDirectory(prefix='entropy-diagnostic-') as directory:
        profile = pathlib.Path(directory) / 'offline-fixture.entropy'
        saved = client.call('save-profile', path=str(profile))
        assert saved['ok'], saved
        args = [os.environ['STAGE05_QUALIFICATION'], str(profile)]
        if change is not None:
            args += ['--change-json', json.dumps(change)]
        result = subprocess.run(args, capture_output=True, text=True, timeout=30)
        if result.returncode:
            return {'ok': False, 'error': result.stderr.strip()}
        return {'ok': True, 'data': json.loads(result.stdout)}
