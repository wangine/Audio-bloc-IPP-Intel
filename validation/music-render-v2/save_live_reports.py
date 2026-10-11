"""Archive recorded evidence and capture hashes; never promotes failed cases.

Replaying uses a fresh output path. Large PCM captures remain at their preserved
local paths; their bytes and SHA256 are recorded instead of exceeding Git limits.
"""
import hashlib,json,pathlib,sys,datetime
REPO=pathlib.Path(__file__).absolute().parents[2]
LIVE=REPO/'Drivers/EntropyMusicRenderV2/build/live'
FAMILIES=['matrix','stress','pause-r5','period2208-r5','period192-r6','fp64-r5','fp64-r5-repeat','fp64-r6','incomplete-tail-r6']
def read(path):return json.loads(path.read_text(encoding='utf-8-sig'))
def digest(path):
    sha=hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda:stream.read(4*1024*1024),b''):sha.update(chunk)
    return sha.hexdigest().upper()
def save(output):
    output=pathlib.Path(output)
    if output.exists():raise RuntimeError('Previous evidence is not overwritten')
    report={'createdUtc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'driverVersion':'0.3.0.10','status':'PARTIAL; raw Event transport qualified for recorded tests; full chain not accepted','kernelSourceRevision':'d3ba51e','families':[]}
    for family in FAMILIES:
        folder=LIVE/('installed-0.3.0.10-'+family)
        results=read(folder/'results.json');results=results if isinstance(results,list) else [results]
        evidence={'name':family,'localDirectory':str(folder),'provenance':read(folder/'provenance.json'),'cases':[]}
        for result in results:
            case=result['case'];casepath=folder/f"{case['rate']}-{case['bits']}-{case['servicing']}"
            item={'result':result,'metadata':{},'captures':[]}
            for name in ('render.json','comparison.json','core-final.json','core-raw.bin.client.json','output-comparison.json','full-output-diagnosis.json'):
                path=casepath/name
                if path.exists() and path.stat().st_size:
                    try:item['metadata'][name]=read(path)
                    except json.JSONDecodeError:item['metadata'][name]={'unparseableText':path.read_text(encoding='utf-8-sig')}
            for name in ('render-error.log','output-collector-error.log','collector-error.log','core-error.log'):
                path=casepath/name
                if path.exists() and path.stat().st_size:item['metadata'][name]=path.read_text(encoding='utf-8-sig')
            for name in ('core-raw.bin','core-output-fp64.bin'):
                path=casepath/name
                if path.exists():item['captures'].append({'path':str(path),'bytes':path.stat().st_size,'sha256':digest(path)})
            evidence['cases'].append(item)
        report['families'].append(evidence)
    report['limitations']=['Polling refused; no position-only RAW identity','Native192k FP64 has observed inserted interior invalid clock blocks','Frozen publisher terminal0 is ambiguous and rejected by r6','Other-rate post-SRC exact comparison needs independent latency/startup/flush reference','Native OS partial EOS, overload, power/removal and long ULONG wrap remain unqualified','Real TIDAL A/B is pending; existing listening chain untouched']
    output.write_text(json.dumps(report,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
    raw=[]
    for family in report['families']:
        for case in family['cases']:
            comparison=case['metadata'].get('comparison.json',{})
            if comparison.get('pass'):raw.append(comparison)
    print(json.dumps({'families':len(report['families']),'byteExactRawCases':len(raw),'byteExactFrames':sum(c['frames'] for c in raw),'output':str(output)}))
if __name__=='__main__':save(sys.argv[1])
