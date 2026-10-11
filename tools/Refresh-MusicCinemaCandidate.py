"""Refresh only this task's undelivered 0.1.8-v2 candidate after final UI QA.
Earlier archives are retained locally; no original release can be targeted.
"""
import hashlib,json,pathlib,shutil,subprocess,sys,zipfile
root=pathlib.Path(__file__).resolve().parents[1]
release=pathlib.Path('D:/Projects/3Stage-3/Audio-bloc-IPP-Intel/release/0.1.8-v2').resolve()
assert release.name=='0.1.8-v2' and release.parent.name=='release'
v=root/'validation/music-cinema-v2';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest().upper()
report=json.loads((root/'docs/VALIDATION-MUSIC-CINEMA-v2.json').read_text())
for f in json.loads((v/'protected-baseline.json').read_text(encoding='utf-8-sig')):assert sha(root/f['path'])==f['sha256']
for variant in ('32k','64k'):
 bundle=release/f'EntropyAudioCore-0.1.8-v2-{variant}-win-x64';manifest=json.loads((bundle/'manifest.json').read_text(encoding='utf-8-sig'))
 assert manifest['version']=='0.1.8-v2' and sha(bundle/'runtime/EntropyAudioCore.exe')==report['native'][variant]['coreSha256']
 archive=release/(bundle.name+'.zip');backup=v/(bundle.name+'-before-final-ui.zip')
 if not backup.exists():shutil.copy2(archive,backup)
 shutil.copy2(root/'build/qt-music-cinema/EntropyUI.exe',bundle/'runtime/EntropyUI.exe');shutil.copy2(root/'docs/MUSIC-CINEMA-v2.md',bundle/'README.md')
 r=subprocess.run(['powershell.exe','-NoLogo','-NoProfile','-ExecutionPolicy','Bypass','-File',str(bundle/'tools/Start-MusicCinema.ps1'),'-SmokeTest'],capture_output=True,timeout=25,creationflags=subprocess.CREATE_NO_WINDOW)
 (release/f'smoke-{variant}.txt').write_bytes(r.stdout+r.stderr);assert r.returncode==0,(r.stdout,r.stderr)
 validated=dict(report,packagedVariant=variant,packagedSmoke='PASS')
 (bundle/'VALIDATION-MUSIC-CINEMA-v2.json').write_text(json.dumps(validated,indent=2))
 for f in manifest['files']:p=bundle/f['path'];f['bytes']=p.stat().st_size;f['sha256']=sha(p)
 (bundle/'manifest.json').write_text(json.dumps(manifest,indent=2))
 with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
  for relative in [f['path'] for f in manifest['files']]+['manifest.json']:z.write(bundle/relative,bundle.name+'/'+relative)
source=release/'EntropyAudioCore-0.1.8-v2-source.zip'
with zipfile.ZipFile(source) as z:assert json.loads(z.read('source-manifest.json'))['version']=='0.1.8-v2'
backup=v/'source-before-final-qa.zip'
if not backup.exists():shutil.copy2(source,backup)
source.unlink() # Exact verified file, no recursive deletion or computed directory removal.
r=subprocess.run([sys.executable,str(root/'tools/Package-MusicCinemaSource.py'),str(root),str(source),'D:/Projects/3Stage-3/Audio-bloc-IPP-Intel/local/stage04-ref/Stage3/native-reference'],capture_output=True);assert r.returncode==0,(r.stdout,r.stderr)
(release/'SHA256SUMS.txt').write_text(''.join(sha(p)+'  '+p.name+'\n' for p in sorted(release.glob('*.zip'))),encoding='ascii')
print('PASS final UI smoke both variants; refreshed owned candidate archives; earlier archives retained locally')
