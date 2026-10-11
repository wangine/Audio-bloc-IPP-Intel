"""Preserve a tested v0003 runtime and complete sources in a fresh delivery."""
import argparse, hashlib, json, pathlib, shutil, subprocess, zipfile

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()

def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False)+'\n', encoding='utf-8')

def inventory(root):
    return [{'path':p.relative_to(root).as_posix(), 'bytes':p.stat().st_size, 'sha256':sha(p)}
            for p in sorted(root.rglob('*')) if p.is_file()
            and p.name not in ('manifest.json','SHA256SUMS.txt')
            and p.relative_to(root).parts[0] != 'state']

def archive(root, output):
    with zipfile.ZipFile(output, 'x', compression=zipfile.ZIP_DEFLATED) as z:
        for p in sorted(root.rglob('*')):
            if p.is_file():
                item=zipfile.ZipInfo((pathlib.Path(root.name)/p.relative_to(root)).as_posix(), (2026,10,11,0,0,0))
                item.compress_type=zipfile.ZIP_DEFLATED
                item.external_attr=0o644<<16
                z.writestr(item,p.read_bytes())

def manifest(root, metadata):
    value=dict(metadata,artifacts=inventory(root))
    write_json(root/'manifest.json', value)
    (root/'SHA256SUMS.txt').write_text(''.join(x['sha256']+'  '+x['path']+'\n' for x in value['artifacts']), encoding='ascii')
    for x in value['artifacts']:
        p=root/x['path']
        if sha(p)!=x['sha256'] or p.stat().st_size!=x['bytes']:
            raise RuntimeError('Manifest verification failed: '+x['path'])

a=argparse.ArgumentParser()
for name in ('source','runtime','driver','repository','output','version'):a.add_argument('--'+name,required=True)
args=a.parse_args()
source,runtime,driver,repo,out=[pathlib.Path(getattr(args,n)).absolute() for n in ('source','runtime','driver','repository','output')]
workspace=repo.parent
if not out.is_relative_to(workspace) or out.exists():raise RuntimeError('Use a fresh output directory inside the workspace')
if not args.version.replace('-','').isalnum():raise RuntimeError('Invalid version')
core='core/EntropyMusicSourceCore.exe'
if sha(runtime/core)!=sha(source/'build/EntropyMusicSourceCore.exe'):raise RuntimeError('Runtime Core differs from tested build')
provider='core/providers/EntropyMusicRawProviderV2.dll'
if sha(runtime/provider)!=sha(driver/'user/EntropyMusicRawProviderV2.dll'):raise RuntimeError('Runtime provider differs from qualified bundle')
driver_manifest=json.loads((driver/'manifest.json').read_text(encoding='utf-8-sig'))
revision=subprocess.check_output(['git','rev-parse','HEAD'],cwd=repo,text=True).strip()
out.mkdir(parents=True)
delivery=out/args.version
shutil.copytree(runtime,delivery,ignore=shutil.ignore_patterns('state','manifest.json','SHA256SUMS.txt'))
(delivery/'state').mkdir()
shutil.copy2(runtime/'state/Stage1Music.ini',delivery/'state/Stage1Music.ini')
shutil.copy2(source/'README.md',delivery/'README.md')
shutil.copytree(source/'docs',delivery/'docs',dirs_exist_ok=True)
paired=out/(args.version+'-source')
paired.mkdir()
for name in ('ABI','Engine','Providers','ThirdParty','UI','tools','tests','reference-runtime','docs','CMakeLists.txt','README.md'):
    p=source/name
    if p.is_dir():shutil.copytree(p,paired/name)
    else:shutil.copy2(p,paired/name)
# Include exact committed driver-side fixture and audit dependencies for standalone builds.
driver_source=paired/'ThirdParty/DriverAudit'
driver_source.mkdir()
with zipfile.ZipFile(driver/'source.zip') as z:
    for entry in z.infolist():
        target=driver_source/entry.filename
        if not target.absolute().is_relative_to(driver_source.absolute()) or '..' in pathlib.PurePosixPath(entry.filename).parts:
            raise RuntimeError('Invalid source archive path')
    z.extractall(driver_source)
# Raw PCM captures stay local. Copy compact successful and failed diagnostic evidence.
for p in sorted((source/'evidence').rglob('*')):
    if not p.is_file() or p.stat().st_size>1048576:continue
    rel=p.relative_to(source/'evidence')
    if any('diagnostic' in part or part.startswith('source-closure') for part in rel.parts[:-1]) or p.name=='msvc-environment.cmd':continue
    if p.suffix not in ('.json','.jsonl','.csv','.txt','.log'):continue
    for root in (delivery,paired):
        dst=root/'evidence'/rel;dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,dst)
metadata={'version':args.version,'date':'2026-10-11','sourceWorkingTreeBaseCommit':revision,'sourceModifiedLocal':True,
 'compiler':'MSVC 19.51.36257 x64 /fp:precise','ui':'Qt 6.11.2 GNU 13.1.0 separate process',
 'coreSha256':sha(runtime/core),'uiSha256':sha(runtime/'ui/EntropyMusicSourceQt.exe'),
 'providerSha256':sha(runtime/provider),'providerBundleManifestSha256':sha(driver/'manifest.json'),
 'kernelVersion':'0.3.0.11','installedKernelSha256':sha(driver/'driver/EntropyMusicRenderV2.sys'),
 'providerAbiSha256':sha(source/'ABI/endpoint_provider_v2.h'),
 'lifecycleExtensionSha256':sha(source/'ABI/endpoint_stream_state_v1.h'),
 'rawProviderCapabilities':5,
 'referenceV0002CoreSha256':driver_manifest['frozenCoreSha256'],
 'output':'stereo FP64/192000 Hz; 1024 frames; MUSIC+AUX unity sum',
 'processing':'No gain, mute, limiter, clipping or normalization; original r8brain SRC',
 'startupBuffering':'ceil(converted packet frames/1024)+1 completed blocks, max 16 blocks; 1920@192k:16ms; 2048@44.1k:53.333ms; SRC settling additional',
 'native192kAcceptance':'See version-specific live report for exact binary64 qualification, frame count and boundaries',
 'srcAcceptance':'Original r8brain differential regressions; raw format qualification and STOP/RUN results are recorded separately',
 'auxAcceptance':'No new AUX driver qualification in this revision; earlier finite results are preserved separately',
 'limitations':['Not a claim of TIDAL Shared/Exclusive PCM equivalence','Kernel mode0 remains unverified WASAPI access','Frozen v0002 must retain its original Provider bundle','Legacy MUSIC adapter is not raw-PCM certified','Polling commit path unsupported','AUX Exclusive non-MMCSS probe failed and is retained','Finite live checks do not guarantee all machine load conditions'],
 'state':'Mutable selectors copied from tested local runtime; process-owner records excluded'}
for root in (delivery,paired):manifest(root,metadata)
archives=[]
for root,kind in ((delivery,'runtime'),(paired,'source')):
    p=out/(args.version+'-'+kind+'.zip');archive(root,p)
    archives.append({'path':p.name,'bytes':p.stat().st_size,'sha256':sha(p)})
write_json(out/'archives.json',archives)
print(json.dumps({'output':str(out),'archives':archives},indent=2))
