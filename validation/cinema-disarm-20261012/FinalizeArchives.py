"""Refresh final source snapshots and verify deliveries without touching audio."""
import hashlib
import json
import pathlib
import shutil
import subprocess
import sys
import zipfile

diag = pathlib.Path(__file__).parent
repo = diag.parents[1] / 'Audio-bloc-IPP-Intel'
decoder = diag.parents[1] / 'CinemaDecoder-v0.2.21'
out = repo / 'release/music-cinema/Cinema/0.2.21'
integration = repo / 'Integration/Cinema/Decoder-v0.2.21'
validation = repo / 'validation/cinema-disarm-20261012'
stage3 = repo / 'release/0.1.9-v2-r3/EntropyAudioCore-0.1.9-v2-r3-64k-win-x64'
out3 = repo / 'release/music-cinema/Stage3/0.1.9-v2-r3'

def digest(p):
    with p.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()

for name in ['installed-v21-final-hashes.json', 'telemetry-lossless-verification.json',
             'final-runtime-processes.json']:
    shutil.copy2(diag / name, decoder / 'evidence' / name)
    shutil.copy2(diag / name, integration / 'evidence' / name)
shutil.copy2(diag / 'FinalizeArchives.py', validation / 'FinalizeArchives.py')
subprocess.run([sys.executable, str(decoder / 'tools/SourceBundle.py')], check=True)
shutil.copy2(decoder / 'CinemaDecoder-v0.2.21-Source-and-Evidence.zip', out)
shutil.copy2(decoder / 'evidence/source-archive.json', integration / 'evidence/source-archive.json')

shutil.copy2(repo / 'docs/LATEST-LOCAL-REFERENCE.md', stage3 / 'README.md')
manifest = json.loads((stage3 / 'manifest.json').read_text())
manifest['files'] = [dict(path=f.relative_to(stage3).as_posix(), bytes=f.stat().st_size,
                          sha256=digest(f)) for f in sorted(stage3.rglob('*'))
                     if f.is_file() and f.name != 'manifest.json'
                     and 'logs' not in f.relative_to(stage3).parts]
(stage3 / 'manifest.json').write_text(json.dumps(manifest, indent=2))
with zipfile.ZipFile(out3 / (stage3.name + '.zip'), 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as z:
    for f in sorted(stage3.rglob('*')):
        if f.is_file() and 'logs' not in f.relative_to(stage3).parts:
            z.write(f, stage3.name + '/' + f.relative_to(stage3).as_posix())

source3 = out3 / 'EntropyAudioCore-0.1.9-v2-r3-source.zip'
if source3.exists():
    prior = diag / ('EntropyAudioCore-0.1.9-v2-r3-source-prior-' + digest(source3)[:12] + '.zip')
    assert not prior.exists(), prior
    source3.rename(prior)
subprocess.run([sys.executable, str(repo / 'tools/Package-MusicCinemaSource.py'), str(repo),
                str(source3), str(repo / 'local/stage04-ref/Stage3/native-reference'),
                '0.1.9-v2-r3'], check=True)

verified = []
for folder in [out, repo / 'release/music-cinema/Stage2/v0010', out3]:
    files = sorted(f for f in folder.iterdir() if f.is_file()
                   and f.name not in {'SHA256SUMS.txt', 'delivery.json'})
    for f in files:
        assert f.stat().st_size < 100_000_000, f
        if f.suffix == '.zip':
            with zipfile.ZipFile(f) as z:
                assert z.testzip() is None, f
                if 'source-manifest.json' in z.namelist():
                    source_manifest = json.loads(z.read('source-manifest.json'))
                    for row in source_manifest['files']:
                        data = z.read(row['path'])
                        assert len(data) == row['bytes'], row['path']
                        assert hashlib.sha256(data).hexdigest() == row['sha256'].lower(), row['path']
    records = [dict(file=f.name, bytes=f.stat().st_size, sha256=digest(f)) for f in files]
    (folder / 'SHA256SUMS.txt').write_text(''.join(r['sha256'] + '  ' + r['file'] + '\n'
                                               for r in records))
    (folder / 'delivery.json').write_text(json.dumps(records, indent=2))
    verified.extend(dict(directory=str(folder), **r) for r in records)

installed = pathlib.Path('C:/Program Files/Entropy Audio/Surround Decoder')
assert digest(installed / 'EntropySurroundDecoder.exe') == '820f23daf25d3d093f1a828d8a14731e8412f106c85dc69bfa9855efb2cf6d52'
assert digest(installed / 'Entropy Surround Decoder.exe') == 'cd4cdfaaec40ab387298ddfb6435f0ac1b9571fcac4426c90e71c8f8a2b60f2b'
assert digest(stage3 / 'runtime/EntropyAudioCore.exe') == '90b7fc3ac482b661c7960f7a024bbb415e4f41cb5f75884e79021deab251699c'
(diag / 'final-archive-verification.json').write_text(json.dumps(verified, indent=2))
shutil.copy2(diag / 'final-archive-verification.json', validation)
print(json.dumps(dict(delivery_files=len(verified), zip_crc='PASS', source_manifest='PASS',
                     installed_audio_binaries='UNCHANGED'), indent=2), flush=True)
