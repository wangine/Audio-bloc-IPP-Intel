"""Archive the candidate sources and evidence without build intermediates."""
from pathlib import Path
import hashlib,json,zipfile
root=Path(__file__).resolve().parent.parent
archive=root/'CinemaDecoder-v0.2.21-Source-and-Evidence.zip'
directories=['source','ThirdParty','tools','tests','docs','evidence']
files=[f for d in directories for f in (root/d).rglob('*')
       if f.is_file() and f.name!='source-archive.json']
files += [root/n for n in ['CMakeLists.txt','README.txt','THIRD-PARTY-NOTICES.txt',
                          'START-CINEMA-LIVE.cmd','ROLLBACK-ORIGINAL.cmd']]
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as output:
    for file in sorted(files):output.write(file,str(file.relative_to(root)))
with zipfile.ZipFile(archive) as check:
    assert check.testzip() is None
result={'file':str(archive),'files':len(files),'sha256':hashlib.sha256(archive.read_bytes()).hexdigest()}
(root/'evidence'/'source-archive.json').write_text(json.dumps(result,indent=2))
print(json.dumps(result,indent=2))


