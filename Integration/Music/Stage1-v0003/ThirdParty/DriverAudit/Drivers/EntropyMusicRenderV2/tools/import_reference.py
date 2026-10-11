"""Fetch exact Git blobs for the approved Microsoft reference, never HEAD."""
import concurrent.futures
import hashlib
import json
import pathlib
import sys
import urllib.request

PIN = '2dc3fd3a0cc84a2933f2194e7ec0871584979071'
root = pathlib.Path(__file__).absolute().parents[1]
commit, inventory, destination = sys.argv[1:]
if commit != PIN:
    raise SystemExit('Reference commit differs from approved pin')
target = pathlib.Path(destination).absolute()
entries = json.loads(pathlib.Path(inventory).read_text(encoding='utf-8-sig'))
if isinstance(entries, dict):
    entries = entries['tree']
files = [e for e in entries if e.get('type', 'blob') == 'blob' and
         (e['path'].startswith('audio/simpleaudiosample/') or e['path'] == 'LICENSE')]
assert files and any(e['path'] == 'LICENSE' for e in files)

def fetch(e):
    path = pathlib.PurePosixPath(e['path'])
    if path.is_absolute() or '..' in path.parts:
        raise ValueError('Unsafe reference path')
    url = 'https://raw.githubusercontent.com/microsoft/Windows-driver-samples/' + PIN + '/' + path.as_posix()
    request = urllib.request.Request(url, headers={'User-Agent': 'Entropy-reference-import'})
    with urllib.request.urlopen(request, timeout=60) as response:
        body = response.read()
    blob = hashlib.sha1(b'blob ' + str(len(body)).encode() + b'\0' + body).hexdigest()
    if blob != e['sha']:
        raise ValueError('Git blob mismatch: ' + path.as_posix())
    out = target.joinpath(*path.parts)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(body)
    return {'path': path.as_posix(), 'gitBlobSha': blob, 'sha256': hashlib.sha256(body).hexdigest()}

with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
    imported = sorted(pool.map(fetch, files), key=lambda e: e['path'])
manifest = {'repository': 'https://github.com/microsoft/Windows-driver-samples',
            'commit': PIN, 'license': 'MS-PL', 'files': imported}
(root / 'upstream-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
print('Imported and verified', len(imported), 'pinned Git blobs')
