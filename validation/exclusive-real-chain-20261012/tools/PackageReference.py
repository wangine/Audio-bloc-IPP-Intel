import pathlib,zipfile,shutil,json,hashlib,os
root=pathlib.Path('D:/Projects/3Stage-3');repo=root/'Audio-bloc-IPP-Intel';diag=root/'Diagnostics/Exclusive-RealChain-20261012'
stage1=root/'Stage1-v0003-r9';snapshot=repo/'Integration/Music/Stage1-v0003-r9';delivery=repo/'release/music-cinema/Stage1/v0003-r9';delivery.mkdir(parents=True,exist_ok=True)
skipdirs={'.git','__pycache__','release','evidence'}
skipSuffix={'.obj','.o','.pdb','.ilk','.exp','.log','.tlog','.pch','.idb','.lib','.a'}
def sourcefiles(base):
    for directory,dirs,files in os.walk(base):
        dirs[:]=[d for d in dirs if d.lower() not in skipdirs and not d.lower().startswith('build')]
        for name in files:
            p=pathlib.Path(directory)/name
            if p.suffix.lower() in skipSuffix:continue
            # Binary bridge dependencies in reference-runtime belong to the
            # source closure; outputs elsewhere belong to runtime archives.
            if p.suffix.lower() in ('.exe','.dll','.sys','.cat','.zip') and 'reference-runtime' not in p.relative_to(base).parts:continue
            yield p
files=list(sourcefiles(stage1));count=0
for p in files:
    target=snapshot/p.relative_to(stage1);target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,target);count+=1
print('Stage1 source closure files',count,flush=True)
def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
    return h.hexdigest().upper()
def archive(paths,base,dest,prefix):
    with zipfile.ZipFile(dest,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=6,allowZip64=True) as z:
        for p in paths:z.write(p,(pathlib.Path(prefix)/p.relative_to(base)).as_posix())
    with zipfile.ZipFile(dest) as z:
        bad=z.testzip()
        if bad:raise RuntimeError('CRC '+bad)
    if dest.stat().st_size>=99*1024*1024:raise RuntimeError('GitHub individual file limit '+str(dest))
    return dict(path=dest.relative_to(repo).as_posix(),bytes=dest.stat().st_size,sha256=sha(dest))
archives=[]
archives.append(archive(files,stage1,delivery/'stage1-native-music-v0003-r9-source.zip','Stage1-v0003-r9'))
runtime=stage1/'release/stage1-native-music-v0003-r9'
def runtimefiles(base):
    for p in base.rglob('*'):
        if not p.is_file():continue
        if p.suffix.lower() in ('.lock','.log','.pdb','.obj'):continue
        if p.name in ('core-owner.json','ui-owner.json','diagnostic.json','core.stop','core-stdout.txt','core-stderr.txt','live-owner.json'):continue
        yield p
archives.append(archive(list(runtimefiles(runtime)),runtime,delivery/'stage1-native-music-v0003-r9-runtime.zip','stage1-native-music-v0003-r9'))
validation=repo/'validation/exclusive-real-chain-20261012';validation.mkdir(parents=True,exist_ok=True)
names=['REAL-CHAIN-VERDICT.md','stage2-before.Edsp','stage1-before.ini','stage3-before.entropy','baseline-header-analysis.json','stage2-comparison.json','graph-generation-transitions.json','known-source24-comparison.json','offline-replay.json','dac-reference.json','pcm-verification.json','buffer-measurements.json','observation2-initial.json','stage3-before.json','crossover-parameters.json']
names+=['tool-integrity-checks.json']
for name in names:shutil.copy2(diag/name,validation/name)
# Only the last telemetry snapshot is needed in Git; the full time series stays local.
lines=(diag/'observation2-telemetry.jsonl').read_text().splitlines();(validation/'last-live-telemetry.json').write_text(json.dumps(json.loads(lines[-1]),indent=2))
for folder in ('observed','observed2'):
    target=validation/folder;target.mkdir(exist_ok=True)
    for p in (diag/folder).glob('*-summary.json'):shutil.copy2(p,target/p.name)
target=validation/'tools';target.mkdir(exist_ok=True)
for p in (diag/'tools').iterdir():
    if p.suffix in ('.cpp','.py','.ps1'):shutil.copy2(p,target/p.name)
shutil.copy2(root/'docs/superpowers/plans/2026-10-12-exclusive-real-chain.md',validation/'plan.md')
# Snapshot current Stage2 source if any existing source mirror differs.
stage2=root/'Stage2';stage2target=repo/'Integration/Music/Stage2';changes=[]
for top in ('source','transport'):
    for p in sourcefiles(stage2/top):
        target=stage2target/top/p.relative_to(stage2/top)
        if not target.exists() or sha(p)!=sha(target):
            target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,target);changes.append(target.relative_to(repo).as_posix())
(validation/'stage2-source-snapshot-changes.json').write_text(json.dumps(changes,indent=2));print('Stage2 snapshot differences',len(changes),flush=True)
refs={
 'stage1':runtime/'core/EntropyMusicSourceCore.exe',
 'provider':runtime/'core/providers/EntropyMusicRawProviderV2.dll',
 'stage2':stage2/'release/stage2-music-v0009/EntropyDspEngine.exe',
 'stage3':repo/'release/0.1.9-v2-r2/EntropyAudioCore-0.1.9-v2-r2-64k-win-x64/runtime/EntropyAudioCore.exe',
 'observationStage3':diag/'observation-stage3/EntropyAudioCore.exe'}
expected={'stage1':'EAE8B72517E11F157241FA189BF0DBAEEC3F5C58CEA6F1D898FC4E8D0A00981F','provider':'3329D657956A42A6E83C28850EE457D1A3A9BF9AC327955825621732B6C4B0DE','stage2':'A9E8F777A870185152F3DF92DE410C02FF54486363AFFCE7F3877CC83572D3F3','stage3':'E64D7F193FF7A462EBF54CAF7848A3F83F8A55323A75233BD15BCD1529A27AFD'}
binary=[]
for k,p in refs.items():
    digest=sha(p)
    if k in expected and digest!=expected[k]:raise RuntimeError('Reference binary changed '+k)
    binary.append(dict(component=k,localPath=str(p),sha256=digest))
for p in (delivery/'stage1-native-music-v0003-r9-source.zip',delivery/'stage1-native-music-v0003-r9-runtime.zip'):
    print(p.name,p.stat().st_size,sha(p),flush=True)
for relative,component in (
 ('release/music-cinema/Stage2/stage2-music-v0009-source.zip',None),
 ('release/music-cinema/Stage2/stage2-music-v0009-win-x64.zip','stage2'),
 ('release/music-cinema/Stage3/0.1.9-v2-r2/EntropyAudioCore-0.1.9-v2-r2-source.zip',None),
 ('release/music-cinema/Stage3/0.1.9-v2-r2/EntropyAudioCore-0.1.9-v2-r2-32k-win-x64.zip',None),
 ('release/music-cinema/Stage3/0.1.9-v2-r2/EntropyAudioCore-0.1.9-v2-r2-64k-win-x64.zip','stage3')):
    p=repo/relative
    with zipfile.ZipFile(p) as z:
        bad=z.testzip()
        if bad:raise RuntimeError('CRC '+relative+':'+bad)
        if component:
            exeName=refs[component].name
            members=[n for n in z.namelist() if pathlib.PurePosixPath(n).name==exeName]
            if len(members)!=1:raise RuntimeError('Executable membership '+relative)
            if hashlib.sha256(z.read(members[0])).hexdigest().upper()!=expected[component]:raise RuntimeError('Archived executable differs from live reference '+relative)
    archives.append(dict(path=relative,bytes=p.stat().st_size,sha256=sha(p),alreadyPublished=True))
manifest=dict(date='2026-10-12',sourceFiles=count,archives=archives,binaries=binary,localCaptures=str(diag/'observed2'),productionStage3SourceCommit='fd735c5420d81f4ec89920d56c9a4039ce8809e7',status='Verified known diagnostic signal; additional music listening remains open; no audio algorithm changes')
(validation/'reference-manifest.json').write_text(json.dumps(manifest,indent=2));(delivery/'SHA256SUMS.txt').write_text('\n'.join(a['sha256']+'  '+pathlib.Path(a['path']).name for a in archives)+'\n')
shutil.copy2(diag/'REAL-CHAIN-VERDICT.md',repo/'docs/EXCLUSIVE-REAL-CHAIN-20261012.md')
print('Reference packaging completed',flush=True)
