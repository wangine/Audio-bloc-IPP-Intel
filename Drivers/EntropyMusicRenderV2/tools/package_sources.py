"""Deterministic source archive and SHA256 inventory, including exact ABI."""
import hashlib, json, pathlib, subprocess, sys, zipfile
repo=pathlib.Path(sys.argv[1]); output=pathlib.Path(sys.argv[2])
paths=['Drivers/EntropyMusicRenderV2','Integration/Music/Stage1/ABI/endpoint_provider_v2.h','Integration/Music/Stage1/ThirdParty/StageTransport/StagePublishV2','Integration/Music/Stage1/ThirdParty/StageTransport/StageBusSDK/include','Integration/Music/Stage1/ThirdParty/StageTransport/StageBusSDK/src/StageBusValidation.cpp','Integration/Music/Stage1/ThirdParty/StageTransport/StageLink/StageLinkProtocol.h','Integration/Music/Stage1/ThirdParty/StageTransport/StageLink/StageLinkSecurity.h','Integration/Music/Stage1/ThirdParty/StageTransport/StageLink/StageLinkSecurity.cpp','docs/architecture/DRIVER-V2-HANDOFF.md','docs/superpowers/plans/2026-10-11-music-render-v2-driver.md']
files=subprocess.check_output(['git','ls-files','--cached','--others','--exclude-standard','-z','--',*paths],cwd=repo).decode('utf-8').split('\0')
inventory=[]
with zipfile.ZipFile(output/'source.zip','x',compression=zipfile.ZIP_DEFLATED) as archive:
    for name in sorted(set(filter(None,files))):
        body=(repo/name).read_bytes()
        item=zipfile.ZipInfo(name,(2026,10,11,0,0,0));item.compress_type=zipfile.ZIP_DEFLATED;item.external_attr=0o644<<16
        archive.writestr(item,body)
        inventory.append({'path':name,'bytes':len(body),'sha256':hashlib.sha256(body).hexdigest().upper()})
(output/'source-files.json').write_text(json.dumps(inventory,indent=2)+'\n',encoding='utf-8')
