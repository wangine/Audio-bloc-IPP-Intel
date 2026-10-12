"""Verify the staged scope and previously validated runtime identities."""
import hashlib
import json
import pathlib
import subprocess
import zipfile

diag = pathlib.Path(__file__).parent
repo = diag.parents[1] / 'Audio-bloc-IPP-Intel'
decoder = diag.parents[1] / 'CinemaDecoder-v0.2.21'

def digest(p):
    with p.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()

paths = subprocess.check_output(['git', 'diff', '--cached', '--name-only', '-z'], cwd=repo).decode().split('\0')[:-1]
allowed_files = {'Engine/Control/CoreRuntime.cpp', 'Engine/Egress/LiveSinkWorkers.cpp',
                 'Tests/Stage019Egress.cpp', 'Tests/Stage07LiveWorkers.cpp', 'UI/main.cpp',
                 'docs/LATEST-LOCAL-REFERENCE.md'}
allowed_roots = ('Integration/Cinema/Decoder-v0.2.21/', 'Integration/Music/Stage2-v0010/',
                 'validation/cinema-disarm-20261012/', 'release/music-cinema/Cinema/0.2.21/',
                 'release/music-cinema/Stage2/v0010/', 'release/music-cinema/Stage3/0.1.9-v2-r3/')
assert all(p in allowed_files or p.startswith(allowed_roots) for p in paths)
sizes = [(p, int(subprocess.check_output(['git', 'cat-file', '-s', ':' + p], cwd=repo))) for p in paths]
assert all(n < 100_000_000 for p, n in sizes)
assert not any(p.endswith(('.etl', '.raw', '.jsonl', '.exe', '.dll', '.pdb')) for p in paths)
for folder in ['source', 'ThirdParty', 'tests']:
    for f in (decoder / folder).rglob('*'):
        if f.is_file():
            assert digest(f) == digest(repo / 'Integration/Cinema/Decoder-v0.2.21' / f.relative_to(decoder)), f
for root in ['release/music-cinema/Cinema/0.2.21', 'release/music-cinema/Stage2/v0010',
             'release/music-cinema/Stage3/0.1.9-v2-r3']:
    folder = repo / root
    for row in json.loads((folder / 'delivery.json').read_text()):
        assert digest(folder / row['file']) == row['sha256'], row

with zipfile.ZipFile(repo / 'release/music-cinema/Cinema/0.2.21/CinemaDecoder-v0.2.21-win-x64.zip') as z:
    assert hashlib.sha256(z.read('CinemaDecoder-v0.2.21/EntropySurroundDecoder.exe')).hexdigest() == '820f23daf25d3d093f1a828d8a14731e8412f106c85dc69bfa9855efb2cf6d52'
    assert hashlib.sha256(z.read('CinemaDecoder-v0.2.21/Entropy Surround Decoder.exe')).hexdigest() == 'cd4cdfaaec40ab387298ddfb6435f0ac1b9571fcac4426c90e71c8f8a2b60f2b'
for folder in ['release/music-cinema/Stage1/v0003-r9', 'release/music-cinema/Stage2']:
    assert subprocess.check_output(['git', 'ls-files', folder], cwd=repo).strip()
result = dict(staged_files=len(paths), staged_bytes=sum(n for p, n in sizes),
              largest_files=sorted(sizes, key=lambda row: row[1], reverse=True)[:10],
              all_files_below_github_limit=True, decoder_source_mirror='PASS',
              archive_hashes='PASS', packaged_runtime_identity='PASS')
(diag / 'staged-delivery-verification.json').write_text(json.dumps(result, indent=2))
print(json.dumps(result, indent=2))
