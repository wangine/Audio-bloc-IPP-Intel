import pathlib,json,numpy as np,struct,collections
out=pathlib.Path(__file__).resolve().parents[1];folder=out/'observed2'
gdt=np.dtype([('qpc','<u8'),('frame','<u8'),('token','<u8'),('drops','<u8'),('header','u1',(128,)),('input','<f8',(13,1024)),('cross','<f8',(16,1024)),('matrix','<f8',(16,1024)),('output','<f8',(16,1024))])
sdt=np.dtype([(k,'<u8') for k in ('qpc','device','host','drops','resync','begin','end')]+[(k,'<f8') for k in ('first','last','ppm','fill')]+[(k,'<u4') for k in ('frames','channels','format','audio','offset','fifo','faults','reset','accepted','result')]+[('rendered','<f8',(16,1024)),('pcm','u1',(65536,))])
assert gdt.itemsize==499872 and sdt.itemsize==196736
def parts(name,dt):
    for p in sorted(folder.glob(name+'-*.bin'),key=lambda p:int(p.stem.rsplit('-',1)[1])):
        if p.stat().st_size%dt.itemsize:raise RuntimeError('truncated record '+str(p))
        if p.stat().st_size:yield np.memmap(p,mode='r',dtype=dt)
summaries={};squares={k:np.zeros(13 if k=='input' else 16) for k in ('input','cross','matrix','output')};peaks={k:np.zeros_like(v) for k,v in squares.items()};n=0
summary=json.loads((folder/'graph-summary.json').read_text());reference=np.memmap(out/'graph-output-f64.bin',mode='w+',dtype='<f8',shape=(summary['written']*1024,16))
seqs=[];tokens=[];gaps=0;previous=None
for a in parts('graph',gdt):
    for start in range(0,len(a),64):
        b=a[start:start+64]
        for k in squares:
            x=b[k];squares[k]+=np.sum(x*x,axis=(0,2));peaks[k]=np.maximum(peaks[k],np.max(np.abs(x),axis=(0,2)))
        for r in b:
            if previous is not None and int(r['frame'])!=previous+1024:gaps+=1
            previous=int(r['frame']);h=struct.unpack('<16s9Q10I',r['header'].tobytes());seqs.append(h[2]);tokens.append(int(r['token']))
            reference[n*1024:(n+1)*1024,:]=r['output'].T;n+=1
reference.flush();del reference
summaries['graph']=dict(blocks=n,frameGaps=gaps,firstSequence=seqs[0],lastSequence=seqs[-1],tokenTransitions=sum(x!=y for x,y in zip(tokens,tokens[1:])),rms={k:(v/(n*1024))**.5 for k,v in squares.items()},peak=peaks)
for name in ('dac-a','dac-b'):
    sums=np.zeros(8);maxs=np.zeros(8);errorMax=np.zeros(8);errorSquares=np.zeros(8);overrange=np.zeros(8,dtype=np.int64);outOfBound=np.zeros(8,dtype=np.int64);zero=0;interiorZero=[];previous=None;gaps=[];resets=collections.Counter();resync=collections.Counter();mins=dict(fifo=2**32,fill=float('inf'));maxDeadline=0;late=0;rejected=0;num=0;audioSeen=False
    for a in parts(name,sdt):
        for r in a:
            frames=int(r['frames']);ch=int(r['channels']);assert frames==1024 and ch==8 and r['format']==1
            if previous is not None and int(r['device'])!=previous+1024:gaps.append([previous,int(r['device'])])
            previous=int(r['device']);resets[int(r['reset'])]+=1;resync[int(r['resync'])]+=1
            maxDeadline=max(maxDeadline,(int(r['end'])-int(r['begin']))/10);late+=(int(r['end'])-int(r['begin']))>frames*1e7/192000
            rejected+=int(r['accepted'])!=frames or r['result']!=0
            if not r['audio']:
                zero+=1
                if audioSeen:interiorZero.append(int(r['device']))
                continue
            audioSeen=True;mins['fifo']=min(mins['fifo'],int(r['fifo']));mins['fill']=min(mins['fill'],float(r['fill']))
            x=r['rendered'][:8,:].T;raw=np.frombuffer(r['pcm'][:frames*ch*4].tobytes(),dtype='<i4').reshape(frames,ch)
            # TPDF is the existing final quantizer: noise [-1,1] LSB + rounding +/- .5 LSB.
            pcm=raw.astype(np.float64)/2147483648.;bounded=np.clip(x,-1.,(8388607.)/8388608.);err=pcm-bounded
            errorMax=np.maximum(errorMax,np.max(np.abs(err),axis=0));errorSquares+=np.sum(err*err,axis=0);outOfBound+=np.sum(np.abs(err)>1.500001/8388608.,axis=0)
            overrange+=np.sum(np.abs(x)>1,axis=0);sums+=np.sum(x*x,axis=0);maxs=np.maximum(maxs,np.max(np.abs(x),axis=0));num+=frames
    summaries[name]=dict(samplesPerChannel=num,initialSilentBlocks=zero-len(interiorZero),interiorSilentBlocks=len(interiorZero),interiorSilentPositions=interiorZero,deviceGaps=gaps,resetReasons=dict(resets),resyncCounts=dict(resync),minimum=mins,maximumCallbackUs=maxDeadline,deadlineMisses=int(late),rejectedBlocks=int(rejected),rms=np.sqrt(sums/max(1,num)),peak=maxs,pcmMaximumErrorLSB=errorMax*8388608,pcmRmsErrorLSB=np.sqrt(errorSquares/max(1,num))*8388608,pcmOutsideDitherBound=outOfBound,overrangeSamples=overrange)
def encode(x):
    if isinstance(x,np.ndarray):return x.tolist()
    if isinstance(x,np.generic):return x.item()
    raise TypeError(type(x))
(out/'buffer-measurements.json').write_text(json.dumps(summaries,indent=2,default=encode));print(json.dumps(summaries,default=encode))
