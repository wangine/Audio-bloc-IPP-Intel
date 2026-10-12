"""Package already verified sources/binaries/evidence; never start an audio process."""
import hashlib
import json
import pathlib
import shutil
import subprocess
import sys
import zipfile

workspace = pathlib.Path('D:/Projects/3Stage-3')
repo = workspace/'Audio-bloc-IPP-Intel'
diag = pathlib.Path(__file__).parent
decoder = workspace/'CinemaDecoder-v0.2.21'
stage2 = workspace/'Stage2'
stage3 = repo/'release/0.1.9-v2-r3/EntropyAudioCore-0.1.9-v2-r3-64k-win-x64'

def digest(p):
    with p.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()

def copytree(a, b):
    shutil.copytree(a, b, dirs_exist_ok=True, ignore=shutil.ignore_patterns('__pycache__','*.pyc'))

def archive(out, files):
    out.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        for src, name in sorted(files, key=lambda entry: entry[1]):
            z.write(src, name)
    with zipfile.ZipFile(out) as z:
        assert z.testzip() is None, out
    assert out.stat().st_size < 100_000_000, ('Git file size limit', out, out.stat().st_size)
    return dict(file=out.name, bytes=out.stat().st_size, sha256=digest(out))

def payload(folder, prefix=''):
    return [(f, prefix+f.relative_to(folder).as_posix()) for f in folder.rglob('*')
            if f.is_file() and not any(part in {'logs','__pycache__'} for part in f.relative_to(folder).parts)]

protected = json.loads((repo/'validation/music-cinema-v2/protected-baseline.json').read_text(encoding='utf-8-sig'))
changes = [x['path'] for x in protected if digest(repo/x['path']) != x['sha256'].lower()]
assert set(changes) == {'Engine\\Egress\\LiveSinkWorkers.cpp','Engine\\Egress\\LiveSinkWorkers.h'}, changes
assert not subprocess.check_output(['git','diff','--name-only','--','Engine/Egress/LiveSinkWorkers.h'],cwd=repo).strip()
(diag/'protected-source-verification.json').write_text(json.dumps(dict(
    total=len(protected), unchanged=len(protected)-len(changes),
    demonstrated_startup_planner_change=['Engine/Egress/LiveSinkWorkers.cpp'],
    previously_committed_header_change=['Engine/Egress/LiveSinkWorkers.h'],
    dsp_algorithms_unchanged=True), indent=2))

# Stage 2 is an unchanged prior feature, copied as a separate indexed reference.
manifest = json.loads((stage2/'release/stage2-music-v0010/package-manifest.json').read_text(encoding='utf-8-sig'))
for row in manifest['files']:
    assert digest(stage2/'release/stage2-music-v0010'/row['path']) == row['sha256'].lower(), row['path']
stage2out = repo/'release/music-cinema/Stage2/v0010'
stage2out.mkdir(parents=True, exist_ok=True)
shutil.copy2(stage2/'release/stage2-music-v0010-source.zip', stage2out)
stage2zip = stage2out/'stage2-music-v0010-win-x64.zip'
stage2files = payload(stage2/'release/stage2-music-v0010', 'stage2-music-v0010/')
stage2files += payload(stage2/'Profiles', 'Profiles/')
archives2 = [archive(stage2zip, stage2files)]
source2 = repo/'Integration/Music/Stage2-v0010'
for name in ['source','contract','transport','tools']:
    copytree(stage2/name, source2/name)
for name in ['README.md','baseline-v0008-manifest.json']:
    shutil.copy2(stage2/name, source2/name)
copytree(stage2/'evidence/profile-load-v0010-20261012', source2/'evidence/profile-load-v0010-20261012')
shutil.copy2(stage2/'release/stage2-music-v0010/README.md', stage2out/'README.md')

# Preserve old packages; complete the already tested 64k r3 runtime.
old = repo/'release/0.1.9-v2-r2/EntropyAudioCore-0.1.9-v2-r2-64k-win-x64'
copytree(old/'notices', stage3/'notices')
shutil.copy2(diag/'CINEMA-DISARM-VERDICT.md', stage3/'VALIDATION-CINEMA.md')
shutil.copy2(repo/'docs/LATEST-LOCAL-REFERENCE.md', stage3/'README.md')
m3 = json.loads((stage3/'manifest.json').read_text())
m3.update(status='LOCAL DTS reference; build, targeted tests, real DAC trials and user listening accepted',
          targetFrames=4096, live_profile_sha256=digest(stage3/'Profiles/Cinema-20261012.entropy'),
          analogAlignmentMeasured=False, universalCodecQualification=False)
m3['files']=[dict(path=f.relative_to(stage3).as_posix(),bytes=f.stat().st_size,sha256=digest(f))
             for f in sorted(stage3.rglob('*')) if f.is_file() and f.name!='manifest.json' and 'logs' not in f.relative_to(stage3).parts]
(stage3/'manifest.json').write_text(json.dumps(m3,indent=2))
out3=repo/'release/music-cinema/Stage3/0.1.9-v2-r3'
out3.mkdir(parents=True,exist_ok=True)
archives3=[archive(out3/(stage3.name+'.zip'),payload(stage3,stage3.name+'/'))]

# Reports and tools are readable in Git; complete bulk traces are separate ZIPs.
validation=repo/'validation/cinema-disarm-20261012'
validation.mkdir(parents=True,exist_ok=True)
report_names=['CINEMA-DISARM-VERDICT.md','startup-red.log','startup-green.log','ctest-targeted.log',
 'capture-startup-red.log','capture-startup-green.log','v18-dts-boundary-analysis.txt',
 'dts-recovery-offline-control.json','continuous-dts-recovery-red.log',
 'v20-owner-analysis.txt','v20-publication-check.txt','v20-publication-frontier.txt',
 'v20-live-summary.json','v20-carrier-analysis.txt','v20-carrier-offline-decode.log',
 'v21-owner-analysis.txt','v21-publication-check.txt','v21-active-live-summary.json',
 'v21-live-summary.json','v21-trial-timeline.json','v21-carrier-analysis.txt',
 'v21-carrier-offline-decode.log','v21-installed-live-summary.json','v21-installed-publication-check.txt',
 'v21-gui-owner-analysis.txt','v21-gui-live-summary.json','v21-gui-publication-check.txt',
 'v21-gui-carrier-analysis.txt','v21-gui-carrier-offline-decode.log',
 'installed-v21-hashes.json','installed-v21-registry.json','installed-v21-processes.json',
 'autostart-before-v21.json','autostart-after-v21.json','protected-source-verification.json',
 'final-live-state.json']
for name in report_names:
    shutil.copy2(diag/name,validation/name)
for f in diag.glob('*.py'):
    shutil.copy2(f,validation/f.name)
shutil.copy2(diag/'CinemaTiming.cpp',validation)

shutil.copy2(diag/'CINEMA-DISARM-VERDICT.md',decoder/'docs/CINEMA-DISARM-VERDICT.md')
for name in report_names:
    shutil.copy2(diag/name,decoder/'evidence'/name)
source=repo/'Integration/Cinema/Decoder-v0.2.21'
for name in ['source','ThirdParty','tools','tests','docs','evidence']:
    copytree(decoder/name,source/name)
for name in ['CMakeLists.txt','README.txt','THIRD-PARTY-NOTICES.txt','START-CINEMA-LIVE.cmd','ROLLBACK-ORIGINAL.cmd']:
    shutil.copy2(decoder/name,source/name)

out=repo/'release/music-cinema/Cinema/0.2.21'
out.mkdir(parents=True,exist_ok=True)
subprocess.run([sys.executable,str(decoder/'tools/SourceBundle.py')],check=True)
sourcezip=decoder/'CinemaDecoder-v0.2.21-Source-and-Evidence.zip'
shutil.copy2(sourcezip,out)
archives=[archive(out/'CinemaDecoder-v0.2.21-win-x64.zip',payload(decoder/'release','CinemaDecoder-v0.2.21/'))]
for name in ['v18-dts-carrier-48000-s16le-stereo.raw','v21-gui-carrier-48000-s16le-stereo.raw']:
    archives.append(archive(out/(name+'.zip'),[(diag/name,name)]))
bulk=[(f,f.name) for f in diag.iterdir() if f.is_file() and
      (f.name.startswith(('v19-','v20-','v21-')) and f.suffix in {'.csv','.jsonl','.log'})]
archives.append(archive(out/'Cinema-v0.2.21-Full-Trace-Evidence.zip',bulk))
raw_manifest=[dict(file=f.name,bytes=f.stat().st_size,sha256=digest(f),preserved_local=str(f),
                   included_in_git_archive=f.name in {'v18-dts-carrier-48000-s16le-stereo.raw','v21-gui-carrier-48000-s16le-stereo.raw'})
              for f in sorted(diag.glob('*.raw'))]
(out/'RAW-CAPTURE-INDEX.json').write_text(json.dumps(raw_manifest,indent=2))

for folder in [out,stage2out,out3]:
    files=[f for f in folder.iterdir() if f.is_file() and f.name not in {'SHA256SUMS.txt','delivery.json'}]
    (folder/'SHA256SUMS.txt').write_text(''.join(digest(f)+'  '+f.name+'\n' for f in sorted(files)))
    (folder/'delivery.json').write_text(json.dumps([dict(file=f.name,bytes=f.stat().st_size,sha256=digest(f)) for f in sorted(files)],indent=2))
print(json.dumps(dict(cinema=archives,stage2=archives2,stage3=archives3,
                     protected_sources=json.loads((diag/'protected-source-verification.json').read_text())),indent=2))
