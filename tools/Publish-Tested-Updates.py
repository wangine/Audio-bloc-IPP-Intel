"""Import immutable, locally qualified Stage1 r7 / Stage3 r2 deliveries.

Copies verified bytes only; never modifies live runtimes or existing archives.
Git staging, commits and push remain separate explicit operations.
"""
import hashlib
import json
import pathlib
import shutil
import sys
import zipfile

repo = pathlib.Path(sys.argv[1]).absolute()
delivery = pathlib.Path(sys.argv[2]).absolute()
stage3 = repo / 'release/0.1.9-v2-r2'
stage1 = repo / 'Integration/Music/Stage1-v0003'
release1 = stage1 / 'releases/stage1-native-music-v0003-r7'
release3 = repo / 'release/music-cinema/Stage3/0.1.9-v2-r2'
records = []

def digest(data):
    return hashlib.sha256(data).hexdigest().upper()

def copy_verified(src, dst):
    data = src.read_bytes()
    dst.parent.mkdir(parents=True, exist_ok=True)
    if dst.exists() and dst.read_bytes() == data:
        return
    if dst.exists() and (dst.suffix == '.zip' or 'releases' in dst.parts):
        raise RuntimeError('Preserve existing delivery: ' + str(dst))
    shutil.copyfile(src, dst)
    assert dst.read_bytes() == data

def audit_archive(src, manifest_name, prefix, entries_key):
    with zipfile.ZipFile(src) as archive:
        manifest = json.loads(archive.read(prefix + manifest_name))
        for item in manifest[entries_key]:
            data = archive.read(prefix + item['path'])
            assert digest(data) == item['sha256'].upper(), item['path']
            assert len(data) == item['bytes'], item['path']
    return manifest

canonical = delivery / 'stage1-native-music-v0003-r7-source'
manifest1 = json.loads((canonical / 'manifest.json').read_text())
for item in manifest1['artifacts']:
    data = (canonical / item['path']).read_bytes()
    assert digest(data) == item['sha256'].upper(), item['path']
    assert len(data) == item['bytes'], item['path']
for name in ('ABI', 'Engine', 'Providers', 'ThirdParty', 'UI', 'tools',
             'tests', 'reference-runtime', 'docs', 'evidence',
             'CMakeLists.txt', 'README.md'):
    src = canonical / name
    files = src.rglob('*') if src.is_dir() else [src]
    for file in files:
        if file.is_file():
            copy_verified(file, stage1 / file.relative_to(canonical))

for item in json.loads((delivery / 'archives.json').read_text()):
    src = delivery / item['path']
    assert digest(src.read_bytes()) == item['sha256'].upper()
    assert src.stat().st_size == item['bytes']
    stem = 'stage1-native-music-v0003-r7'
    folder = stem + ('-source/' if '-source.zip' in src.name else '/')
    audit_archive(src, 'manifest.json', folder, 'artifacts')
    dst = release1 / src.name
    copy_verified(src, dst)
    records.append(dict(stage='Stage1', path=dst.relative_to(repo).as_posix(),
                        bytes=item['bytes'], sha256=item['sha256']))
copy_verified(delivery / 'archives.json', release1 / 'archives.json')
copy_verified(canonical / 'manifest.json', release1 / 'source-manifest.json')
copy_verified(delivery / 'stage1-native-music-v0003-r7/manifest.json',
              release1 / 'runtime-manifest.json')

for name in ('EntropyAudioCore-0.1.9-v2-r2-32k-win-x64.zip',
             'EntropyAudioCore-0.1.9-v2-r2-64k-win-x64.zip',
             'EntropyAudioCore-0.1.9-v2-r2-source.zip'):
    src = stage3 / name
    source = '-source.zip' in name
    manifest = audit_archive(src, 'source-manifest.json' if source else 'manifest.json',
                             '' if source else name[:-4] + '/',
                             'files' if source else 'files')
    dst = release3 / name
    copy_verified(src, dst)
    records.append(dict(stage='Stage3', path=dst.relative_to(repo).as_posix(),
                        bytes=src.stat().st_size, sha256=digest(src.read_bytes())))
copy_verified(stage3 / 'SHA256SUMS.txt', release3 / 'SHA256SUMS.txt')
publication = repo / 'docs/architecture/publication-r7-r2-artifacts.json'
publication.write_text(json.dumps(records, indent=2) + '\n', encoding='utf-8')
print(json.dumps(dict(status='PASS', archives=len(records),
                     importedSourceArtifacts=len(manifest1['artifacts'])), indent=2))
