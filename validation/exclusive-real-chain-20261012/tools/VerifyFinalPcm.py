import pathlib,json,numpy as np
out=pathlib.Path(__file__).resolve().parents[1];folder=out/'observed2'
dt=np.dtype([(k,'<u8') for k in ('qpc','device','host','drops','resync','begin','end')]+[(k,'<f8') for k in ('first','last','ppm','fill')]+[(k,'<u4') for k in ('frames','channels','format','audio','offset','fifo','faults','reset','accepted','result')]+[('rendered','<f8',(16,1024)),('pcm','u1',(65536,))])
results={}
for name in ('dac-a','dac-b'):
    firstAudio=True;maxError=np.zeros(8);bad=np.zeros(8,dtype=np.int64);samples=0;prefixes=[];leadingZeroOutsideStartup=0
    for p in sorted(folder.glob(name+'-*.bin'),key=lambda p:int(p.stem.rsplit('-',1)[1])):
        if not p.stat().st_size:continue
        a=np.memmap(p,mode='r',dtype=dt)
        for r in a:
            if not r['audio']:continue
            n=int(r['frames']);ch=int(r['channels']);x=r['rendered'][:ch,:n].T
            raw=np.frombuffer(r['pcm'][:n*ch*4].tobytes(),dtype='<i4').reshape(n,ch)
            nz=np.flatnonzero(np.any(raw!=0,axis=1));prefix=int(nz[0]) if len(nz) else n
            expected=np.clip(x,-1.,8388607/8388608.)
            if firstAudio:
                if prefix==n:raise RuntimeError('Entire first audio buffer zero; startup comparison cannot be qualified')
                # LiveSinkWorkers intentionally zeros the PCM prefix until the
                # common audible onset; its pre-quantizer FP64 view stays intact.
                prefixes.append(dict(deviceFrame=int(r['device']),frames=prefix,preMuteFp64Nonzero=bool(np.any(x[:prefix]!=0))))
                expected=expected.copy();expected[:prefix]=0
            elif prefix and np.any(x[:prefix]!=0):leadingZeroOutsideStartup+=prefix
            firstAudio=False
            err=np.abs(raw.astype(float)/2147483648.-expected)*8388608.
            maxError=np.maximum(maxError,np.max(err,axis=0));bad+=np.sum(err>1.500001,axis=0);samples+=n
    if not samples:raise RuntimeError('No audible samples')
    results[name]=dict(samplesPerChannel=samples,startupPcmMutePrefix=prefixes,startupPrefixVerification='Observed/inferred and exempted from FP64 comparison; callback presentation/onset metadata not recorded',leadingQuantizedZeroSamplesAfterStartup=leadingZeroOutsideStartup,maxErrorLSB=maxError.tolist(),outsideExistingTpdfBound=bad.tolist())
(out/'pcm-verification.json').write_text(json.dumps(results,indent=2));print(json.dumps(results))
