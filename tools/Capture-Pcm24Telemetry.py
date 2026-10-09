"""Two short read-only IPC sessions; close Qt first. Never ARM/DISARM or edit policy."""
import argparse,hashlib,json,pathlib,re,sys,time

ROOT=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'Tests'))
from Stage05IpcIntegration import Client

FIELDS=('pcmDitherMode','pcmSeedMode','pcmFaults','pcmOverrangeSamples','pcmClippedBlocks',
        'pcmRailClampSamples','pcmNonfiniteSamples','pcmProcessedFrames','submittedFrames',
        'physicalSinkOpen','state','channels','format','faults','xruns')
COUNTERS=('pcmOverrangeSamples','pcmClippedBlocks','pcmRailClampSamples',
          'pcmNonfiniteSamples','pcmProcessedFrames','submittedFrames','xruns')

def snapshot(endpoint):
    client=Client(endpoint)
    try:
        reply=client.call('telemetry')
        if not reply['ok']:raise RuntimeError(reply.get('error','telemetry unavailable'))
        return reply['data']
    finally:client.close()

def capture(endpoint,seconds,output,runtime_version):
    output.mkdir(parents=True,exist_ok=True)
    header=ROOT/'Shared/include/entropy/pcm24_abi.h'
    text=header.read_text()
    enums={name:int(value) for name,value in re.findall(r'(ENTROPY_PCM24_\w+)\s*=\s*(\d+)\s*[,\n]',text)}
    start=time.monotonic();before=snapshot(endpoint)
    time.sleep(seconds)
    after=snapshot(endpoint);elapsed=time.monotonic()-start
    if before['pid']!=after['pid']:raise RuntimeError('Core restarted; counter interval is invalid')
    if before['headroom']!=after['headroom']:
        # Meter/counter differences are expected; user controls must stay fixed.
        for key in ('mode','manualGainDb','attack','release','revision'):
            if before['headroom'][key]!=after['headroom'][key]:raise RuntimeError('Headroom policy changed during interval')
    if len(before['domains'])!=len(after['domains']):raise RuntimeError('DAC topology changed')
    rows=[];faults=False;overload=False
    for a,b in zip(before['domains'],after['domains']):
        if a['id']!=b['id']:raise RuntimeError('DAC domain changed')
        delta={k:int(b[k])-int(a[k]) for k in COUNTERS}
        if any(v<0 for v in delta.values()):raise RuntimeError('Counters reset; interval is invalid')
        for key in ('pcmDitherMode','pcmSeedMode','format','channels'):
            if a[key]!=b[key]:raise RuntimeError('PCM configuration changed during interval')
        configuration=(b['pcmDitherMode']==enums['ENTROPY_PCM24_DITHER_TPDF_FLAT'] and
                       b['pcmSeedMode']==enums['ENTROPY_PCM24_SEED_PRODUCTION_INDEPENDENT'])
        active=a['physicalSinkOpen'] and b['physicalSinkOpen'] and delta['pcmProcessedFrames']>0
        faults|=not configuration or not active or bool(a['pcmFaults'] or b['pcmFaults'] or b['faults'] or delta['pcmNonfiniteSamples'])
        overload|=bool(delta['pcmOverrangeSamples'] or delta['pcmClippedBlocks'])
        rows.append({'id':a['id'],'before':{k:a[k] for k in FIELDS},'after':{k:b[k] for k in FIELDS},'delta':delta,
                     'configurationPass':configuration,'activeOutputObserved':active})
    result={'status':'FAIL PCM OVERLOAD' if overload else 'FAIL/BLOCKED CONFIGURATION OR ACTIVITY' if faults else 'PASS MEASURED INTERVAL',
            'secondsApproximate':elapsed,'runtimeVersionSupplied':runtime_version,'pidBefore':before['pid'],'pidAfter':after['pid'],
            'armedBefore':before['armed'],'armedAfter':after['armed'],'domains':rows,
            'headerSha256':hashlib.sha256(header.read_bytes()).hexdigest(),
            'enumValues':{k:v for k,v in enums.items() if k in ('ENTROPY_PCM24_DITHER_TPDF_FLAT','ENTROPY_PCM24_SEED_PRODUCTION_INDEPENDENT','ENTROPY_PCM24_OVERRANGE_CLIP_AND_REPORT','ENTROPY_PCM24_IN_32_LE_LEFT_ALIGNED')},
            'audioCommandsSent':False,'profileChanged':False}
    (output/'live-deltas.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result));return 2 if overload or faults else 0

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--endpoint',default='EntropyAudioCore.Control.v1')
    p.add_argument('--seconds',type=float,default=16)
    p.add_argument('--output',type=pathlib.Path,required=True)
    p.add_argument('--runtime-version',required=True)
    args=p.parse_args()
    if not 1<=args.seconds<=30:p.error('Use a bounded 1-30 second fragment')
    sys.exit(capture(args.endpoint,args.seconds,args.output.resolve(),args.runtime_version))
