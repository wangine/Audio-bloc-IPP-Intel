import pathlib,shutil,json,hashlib
repo=pathlib.Path('D:/Projects/3Stage-3/Audio-bloc-IPP-Intel')
old=repo/'release/0.1.9-v2-r2/EntropyAudioCore-0.1.9-v2-r2-64k-win-x64'
new=repo/'release/0.1.9-v2-r3/EntropyAudioCore-0.1.9-v2-r3-64k-win-x64'
new.mkdir(parents=True,exist_ok=True)
shutil.copytree(old/'runtime',new/'runtime',dirs_exist_ok=True)
shutil.copytree(old/'tools',new/'tools',dirs_exist_ok=True)
for n in ('Cold-Start.cmd','Live-Test.cmd','Stop-AudioCore.cmd'):shutil.copy2(old/n,new/n)
shutil.copy2(repo/'build/msvc-music-cinema-65536/EntropyAudioCore.exe',new/'runtime/EntropyAudioCore.exe')
shutil.copy2(repo/'build/qt-music-cinema/EntropyUI.exe',new/'runtime/EntropyUI.exe')
(new/'Profiles').mkdir(exist_ok=True)
shutil.copy2(pathlib.Path(__file__).parent/'observer3/stage3-before.entropy',new/'Profiles/Cinema-20261012.entropy')
manifest=dict(version='0.1.9-v2-r3',variant='64k',fifoCapacityFrames=65536,status='CANDIDATE awaiting local live result',changedAudioMechanism='Startup source reference uses committed PCM; hardware presentation lead retained',originalDspAlgorithmsChanged=False,initialAutomaticArm=False,sourceWaitPolicyUnchanged=True)
manifest['files']=[dict(path=str(p.relative_to(new)).replace('\\','/'),sha256=hashlib.sha256(p.read_bytes()).hexdigest(),bytes=p.stat().st_size) for p in sorted(new.rglob('*')) if p.is_file() and p.name!='manifest.json']
(new/'manifest.json').write_text(json.dumps(manifest,indent=2))
print(new)
