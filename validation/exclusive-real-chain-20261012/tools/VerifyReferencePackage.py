import pathlib, json, hashlib, zipfile
root = pathlib.Path('D:/Projects/3Stage-3')
repo = root / 'Audio-bloc-IPP-Intel'
dest = repo / 'validation/exclusive-real-chain-20261012'
manifest = json.loads((dest / 'reference-manifest.json').read_text())

def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda: f.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest().upper()

checks = []
for item in manifest['archives']:
    path = repo / item['path']
    assert path.stat().st_size == item['bytes']
    assert sha(path) == item['sha256'], str(path)
    with zipfile.ZipFile(path) as z:
        assert z.testzip() is None, str(path)
    checks.append(dict(kind='archive', path=item['path'], sha256=item['sha256'], crc='PASS'))
for item in manifest['binaries']:
    assert sha(pathlib.Path(item['localPath'])) == item['sha256'], item['component']
    checks.append(dict(kind='unchanged-binary', component=item['component'], sha256=item['sha256']))
kernel = pathlib.Path('C:/WINDOWS/system32/DriverStore/FileRepository/entropymusicrenderv2.inf_amd64_26f4a0766080c004/EntropyMusicRenderV2.sys')
assert sha(kernel) == 'FB78EC3CC6066CDF3A039CCE95CD13C26199F2EC630F188351155F84EBB610AA'
checks.append(dict(kind='unchanged-kernel', version='0.3.0.11', sha256=sha(kernel)))
replay = json.loads((dest / 'offline-replay.json').read_text())
assert replay['blocks'] == 11250 and replay['gaps'] == 0
assert all(c['maxError'] == 0 and c['differentSamples'] == 0 for stage in replay['stages'] for c in stage)
dac = json.loads((dest / 'dac-reference.json').read_text())
assert all(d['checkedSamples'] > 10000000 and d['failedSamples'] == 0 for d in dac['domains'])
assert len(json.loads((dest / 'tool-integrity-checks.json').read_text())) == 9
for folder in ('Stage1-v0003-r9/release/stage1-native-music-v0003-r9/Start-Stage1-Music.cmd', 'Stage2/release/stage2-music-v0009/ColdStart-DSP-Music.cmd', 'Audio-bloc-IPP-Intel/release/0.1.9-v2-r2/EntropyAudioCore-0.1.9-v2-r2-64k-win-x64/Live-Test.cmd'):
    assert (root / folder).is_file(), folder
capture = json.loads((dest / 'capture-sha256.json').read_text())
assert len(capture['files']) == 23
result = dict(status='PASS', checks=checks, qualifiedCaptureFiles=len(capture['files']), qualifiedCaptureBytes=sum(p['bytes'] for p in capture['files']), generalSuitesRun=False, scope='Archive CRC/hashes, unchanged reference binaries, evidence consistency and launcher existence; no live sources opened')
(dest / 'package-verification.json').write_text(json.dumps(result, indent=2))
print(json.dumps(dict(status='PASS', archives=len(manifest['archives']), unchangedBinaries=len(manifest['binaries']), qualifiedCaptureFiles=len(capture['files'])), indent=2))
