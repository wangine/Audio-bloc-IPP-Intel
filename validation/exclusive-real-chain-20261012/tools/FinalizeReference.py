import pathlib, json, hashlib, shutil, zipfile, sys
root = pathlib.Path('D:/Projects/3Stage-3')
repo = root / 'Audio-bloc-IPP-Intel'
diag = root / 'Diagnostics/Exclusive-RealChain-20261012'
dest = repo / 'validation/exclusive-real-chain-20261012'

def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda: f.read(4 * 1024 * 1024), b''):
            h.update(block)
    return h.hexdigest().upper()

captures = sorted((diag / 'observed2').glob('*.bin'))
captures += [diag / 'observed2-stage2-source.bin', diag / 'observed2-stage2-dsp.bin']
if '--archive-only' not in sys.argv:
    items = []
    for path in captures:
        items.append(dict(path=path.relative_to(diag).as_posix(), bytes=path.stat().st_size, sha256=sha(path)))
        print('Capture hashed', path.name, flush=True)
    capture_manifest = dict(baseDirectory=str(diag), files=items, policy='Preserved locally; no samples removed; large PCM captures not uploaded to Git')
    (diag / 'capture-sha256.json').write_text(json.dumps(capture_manifest, indent=2))
else:
    # Documentation-only repackaging reuses the already computed capture hashes.
    assert (diag / 'capture-sha256.json').is_file()
shutil.copy2(diag / 'capture-sha256.json', dest / 'capture-sha256.json')

delivery = repo / 'release/music-cinema/Diagnostics'
delivery.mkdir(parents=True, exist_ok=True)
archive = delivery / 'exclusive-real-chain-20261012-tools.zip'
build = repo / 'build/msvc-music-cinema-65536'
binary_paths = [
    (build / 'EntropyRealChainReplay.exe', 'tools/EntropyRealChainReplay.exe'),
    (build / 'EntropyRealChainDacReference.exe', 'tools/EntropyRealChainDacReference.exe'),
    (diag / 'tools/PassiveCapture.exe', 'tools/PassiveCapture.exe'),
    (diag / 'observation-stage3/EntropyAudioCore.exe', 'qualified-observation-core/EntropyAudioCore.exe'),
]
with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as z:
    for path, name in binary_paths:
        z.write(path, name)
    for path in (diag / 'tools').iterdir():
        if path.suffix in ('.py', '.cpp', '.ps1'):
            z.write(path, 'tools/' + path.name)
    for name in ('stage2-before.Edsp', 'stage3-before.entropy', 'stage1-before.ini', 'REAL-CHAIN-VERDICT.md', 'final-restored-live-status.json'):
        z.write(diag / name, name)
    z.write(dest / 'README.md', 'README.md')
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None
assert archive.stat().st_size < 99 * 1024 * 1024
manifest = json.loads((dest / 'reference-manifest.json').read_text())
relative_archive = archive.relative_to(repo).as_posix()
manifest['archives'] = [a for a in manifest['archives'] if a['path'] != relative_archive]
manifest['archives'].append(dict(path=relative_archive, bytes=archive.stat().st_size, sha256=sha(archive)))
manifest['auditTools'] = [dict(path=name, sha256=sha(path)) for path, name in binary_paths]
manifest['observerProvenance'] = 'Qualified observation Core is the live-tested build. Final source adds flush/close checks and refusal to reuse a nonempty audit directory; compiled but not separately live-qualified. Production Core remains unchanged.'
(dest / 'reference-manifest.json').write_text(json.dumps(manifest, indent=2))
stage1 = repo / 'release/music-cinema/Stage1/v0003-r9'
(stage1 / 'SHA256SUMS.txt').write_text(''.join(a['sha256'] + '  ' + pathlib.Path(a['path']).name + '\n' for a in manifest['archives'] if a['path'].startswith('release/music-cinema/Stage1/')))
(delivery / 'SHA256SUMS.txt').write_text(sha(archive) + '  ' + archive.name + '\n')
for path in (diag / 'tools').iterdir():
    if path.suffix in ('.py', '.cpp', '.ps1'):
        shutil.copy2(path, dest / 'tools' / path.name)
shutil.copy2(root / 'docs/superpowers/plans/2026-10-12-exclusive-real-chain.md', dest / 'plan.md')
print('Final archive verified', archive.name, archive.stat().st_size, sha(archive), flush=True)
