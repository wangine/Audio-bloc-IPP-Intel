import pathlib,json,base64,re,struct,collections,numpy as np
out=pathlib.Path(__file__).resolve().parents[1]
profile=json.loads((out/'stage3-before.entropy').read_text());xml=base64.b64decode(profile['modules'][0]['serialized_state'])[8:].decode(errors='replace')
params={k:float(v) for k,v in re.findall(r'id="([^"]+)" value="([^"]+)"',xml)}
(out/'crossover-parameters.json').write_text(json.dumps(params,indent=2))
t=json.loads((out/'stage3-before.json').read_text())['data'];p={k:t.get(k) for k in ('outputs','matrix','headroom')}
p['channels']=[{k:v for k,v in params.items() if re.search(r'(?<!\d)'+str(ch)+r'$',k)} for ch in range(1,17)]
(out/'profile-summary.json').write_text(json.dumps(p,indent=2))
def passive(name):
    dt=np.dtype([('qpc','<u8'),('header','u1',(128,)),('samples','<f8',(3,1024))]);size=(out/name).stat().st_size
    if (size-256)%dt.itemsize:raise RuntimeError('record size')
    return np.memmap(out/name,mode='r',offset=256,dtype=dt)
def header(h):
    v=struct.unpack('<16s9Q10I',h.tobytes());return dict(epoch=v[1],seq=v[2],first=v[3],gen=v[5:8],revision=v[9],valid=v[11])
def audit_headers(a):
    h=[header(x) for x in a['header']];changes=[];gaps=[]
    for old,new in zip(h,h[1:]):
        if old['seq']+1!=new['seq']:gaps.append([old['seq'],new['seq']])
        if old['gen']!=new['gen'] or old['revision']!=new['revision']:changes.append([old,new])
    return dict(blocks=len(h),gaps=gaps,resetTransitions=changes,validities=dict(collections.Counter(x['valid'] for x in h)),generations=dict(collections.Counter(str(x['gen']) for x in h)))
src=passive('observed2-stage2-source.bin');dsp=passive('observed2-stage2-dsp.bin');summary={'source':audit_headers(src),'dsp':audit_headers(dsp)}
firstSrc=header(src[0]['header'])['seq'];firstDsp=header(dsp[0]['header'])['seq'];offset=firstDsp-firstSrc
startSrc=max(0,offset);startDsp=max(0,-offset);n=min(len(src)-startSrc,len(dsp)-startDsp)
summary['commonBlocks']=n;summary['lr']=[]
for ch in (0,1):
    x=np.array(src['samples'][startSrc:startSrc+n,ch,:]).reshape(-1);y=np.array(dsp['samples'][startDsp:startDsp+n,ch,:]).reshape(-1)
    # Delay estimation only; the subsequent comparison retains every overlapping sample.
    a=x[192000:384000];b=y[192000:384000];size=1<<int(np.ceil(np.log2(len(a)*2)))
    corr=np.fft.irfft(np.fft.rfft(b,size)*np.conj(np.fft.rfft(a,size)),size)
    choices=np.concatenate((np.arange(32768),np.arange(size-32768,size)));ix=int(choices[np.argmax(corr[choices])]);delay=ix if ix<32768 else ix-size
    xx=x[:len(x)-delay] if delay>0 else x[-delay:];yy=y[delay:] if delay>0 else y[:len(y)+delay]
    d=xx-yy;summary['lr'].append(dict(channel=ch+1,delaySamples=delay,samples=len(d),differentSamples=int(np.count_nonzero(d)),differentBinary64Words=int(np.count_nonzero(xx.view('<u8')!=yy.view('<u8'))),maxError=float(np.max(np.abs(d))),rmsError=float(np.sqrt(np.mean(d*d))),rmsIn=float(np.sqrt(np.mean(xx*xx))),rmsOut=float(np.sqrt(np.mean(yy*yy))),peakIn=float(np.max(np.abs(xx))),peakOut=float(np.max(np.abs(yy)))))
    del x,y,xx,yy,d
(out/'stage2-comparison.json').write_text(json.dumps(summary,indent=2));print(json.dumps({k:v for k,v in summary.items() if k not in ('source','dsp')},indent=2))
print('Headers',summary['source']['blocks'],summary['dsp']['blocks'],'gaps',len(summary['source']['gaps']),len(summary['dsp']['gaps']),'resets',len(summary['source']['resetTransitions']),len(summary['dsp']['resetTransitions']))
