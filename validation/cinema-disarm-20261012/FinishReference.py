"""Resume delivery after preserving the full oversized local ZIP."""
import hashlib
import importlib.util
import json
import pathlib
import shutil
import subprocess
import sys
import zipfile

p=pathlib.Path(__file__).parent
repo=pathlib.Path('D:/Projects/3Stage-3/Audio-bloc-IPP-Intel')
decoder=pathlib.Path('D:/Projects/3Stage-3/CinemaDecoder-v0.2.21')
out=repo/'release/music-cinema/Cinema/0.2.21'
oversized=out/'Cinema-v0.2.21-Full-Trace-Evidence.zip'
local=p/'Cinema-v0.2.21-Full-Trace-Evidence-local.zip'
if oversized.exists():
    assert not local.exists()
    oversized.rename(local)
spec=importlib.util.spec_from_file_location('compact',p/'CompactTelemetry.py')
codec=importlib.util.module_from_spec(spec);spec.loader.exec_module(codec)
results=[]
bulk=[]
for f in sorted(p.glob('v*-*-telemetry.jsonl')):
    if not f.name.startswith(('v19-','v20-','v21-')):continue
    delta=f.with_suffix(f.suffix+'.delta.gz')
    result=codec.encode(f,delta)
    results.append(result)
    print(json.dumps(result),flush=True)
    bulk.append((delta,delta.name))
for f in p.iterdir():
    if f.is_file() and f.name.startswith(('v19-','v20-','v21-')) and f.suffix in {'.csv','.log'}:
        bulk.append((f,f.name))
bulk.append((p/'CompactTelemetry.py','CompactTelemetry.py'))
(p/'telemetry-lossless-verification.json').write_text(json.dumps(results,indent=2))
bulk.append((p/'telemetry-lossless-verification.json','telemetry-lossless-verification.json'))
archive=out/'Cinema-v0.2.21-Full-Trace-Evidence.zip'
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
    for f,name in sorted(bulk,key=lambda entry:entry[1]):z.write(f,name)
with zipfile.ZipFile(archive) as z:assert z.testzip() is None
assert archive.stat().st_size<100_000_000,archive.stat().st_size
for f in [p/'CompactTelemetry.py',p/'FinishReference.py',p/'telemetry-lossless-verification.json',
          p/'installed-v21-final-hashes.json',p/'final-runtime-processes.json']:
    shutil.copy2(f,repo/'validation/cinema-disarm-20261012'/f.name)
shutil.copy2(p/'CompactTelemetry.py',decoder/'tools/CompactTelemetry.py')
shutil.copy2(p/'CompactTelemetry.py',repo/'Integration/Cinema/Decoder-v0.2.21/tools/CompactTelemetry.py')

def digest(f):
    with f.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()

raw=[dict(file=f.name,bytes=f.stat().st_size,sha256=digest(f),preserved_local=str(f),
          included_in_git_archive=f.name in {'v18-dts-carrier-48000-s16le-stereo.raw','v21-gui-carrier-48000-s16le-stereo.raw'})
     for f in sorted(p.glob('*.raw'))]
(out/'RAW-CAPTURE-INDEX.json').write_text(json.dumps(raw,indent=2))
(out/'TELEMETRY-LOSSLESS-INDEX.json').write_text(json.dumps(results,indent=2))
shutil.copy2(decoder/'README.txt',out/'README.txt')
for folder in [out,repo/'release/music-cinema/Stage2/v0010',repo/'release/music-cinema/Stage3/0.1.9-v2-r3']:
    files=[f for f in folder.iterdir() if f.is_file() and f.name not in {'SHA256SUMS.txt','delivery.json'}]
    (folder/'SHA256SUMS.txt').write_text(''.join(digest(f)+'  '+f.name+'\n' for f in sorted(files)))
    (folder/'delivery.json').write_text(json.dumps([dict(file=f.name,bytes=f.stat().st_size,sha256=digest(f)) for f in sorted(files)],indent=2))
print('DELIVERY VERIFIED',archive.stat().st_size,flush=True)
