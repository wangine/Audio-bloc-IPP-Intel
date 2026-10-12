import pathlib,wave,struct,subprocess,json,numpy as np
out=pathlib.Path(__file__).resolve().parents[1];previous=out.parent/'TIDAL-Final-AB-20261012';ref=out/'reference24';ref.mkdir(exist_ok=True)
with wave.open(str(previous/'reference/Entropy_TIDAL_Diagnostic_24bit_96000Hz.wav'),'rb') as w:
    assert (w.getnchannels(),w.getsampwidth(),w.getframerate(),w.getnframes())==(2,3,96000,5184000)
    b=np.frombuffer(w.readframes(w.getnframes()),dtype='u1').reshape(-1,2,3);pcm=b[:,:,0].astype(np.int32)|(b[:,:,1].astype(np.int32)<<8)|(b[:,:,2].astype(np.int32)<<16);pcm=((pcm^8388608)-8388608)<<8
raw=ref/'SYNTHETIC-master24-input.bin'
with raw.open('wb') as f:
    padded=pcm.tobytes()+bytes(96000*8*2);first=0;seq=0
    for pos in range(0,len(padded),2048*8):
        data=padded[pos:pos+2048*8];frames=len(data)//8;seq+=1;h=bytearray(160)
        struct.pack_into('<II',h,0,0x20000,160);struct.pack_into('<10I',h,8,64,96000,2,32,24,8,768000,3,1,1)
        h[48:64]=bytes.fromhex('0100000000001000800000aa00389b71');struct.pack_into('<6Q',h,72,1,seq,first,first+1,96000,0)
        struct.pack_into('<4I',h,120,frames,5 if seq==1 else 4,0,len(data));struct.pack_into('<2I',h,136,32,24)
        f.write(struct.pack('<IIIIQQ',0x41524d45,1,1,160+len(data),0,0));f.write(h);f.write(data);first+=frames
    f.write(struct.pack('<IIIIQQ',0x41524d45,1,2,0,0,0))
subprocess.run([str(previous/'tools/ReplayReference.exe'),str(raw),str(ref/'master24-src.bin'),'192000'],check=True)
chunks=[]
with (ref/'master24-src.bin').open('rb') as f:
    while h:=f.read(16):
        gen,n,flags=struct.unpack('<QII',h);chunks.append(np.frombuffer(f.read(n*16),dtype='<f8').reshape(2,n).T.copy())
master=np.concatenate(chunks)[:54*192000];np.save(ref/'master24-192000.npy',master);del chunks
dt=np.dtype([('qpc','<u8'),('header','u1',(128,)),('samples','<f8',(3,1024))]);p=out/'observed2-stage2-source.bin';a=np.memmap(p,mode='r',dtype=dt,offset=256)
x=np.empty((len(a)*1024,2));x[:,0]=a['samples'][:,0,:].reshape(-1);x[:,1]=a['samples'][:,1,:].reshape(-1)
offsets=[]
for ch,start,end in ((0,.25,.65),(1,.95,1.35)):
    template=master[int(start*192000):int(end*192000),ch];size=1<<(len(x)+len(template)-2).bit_length()
    corr=np.fft.irfft(np.fft.rfft(x[:,ch],size)*np.fft.rfft(template[::-1],size),size)[len(template)-1:len(x)]
    peak=int(np.argmax(corr));offset=peak-int(start*192000);offsets.append(offset)
    del corr
result=dict(rate=192000,masterPeriodFrames=len(master),captureFrames=len(x),markerOffsets=offsets,comparisons=[])
for ch in (0,1):
    reference=master[(np.arange(len(x))-offsets[ch])%len(master),ch];d=x[:,ch]-reference
    result['comparisons'].append(dict(channel=ch+1,samples=len(d),maxError=float(np.max(np.abs(d))),rmsError=float(np.sqrt(np.mean(d*d))),gain=float(np.dot(x[:,ch],reference)/np.dot(reference,reference)),differentSamples=int(np.count_nonzero(d))))
(out/'known-source24-comparison.json').write_text(json.dumps(result,indent=2));print(json.dumps(result))
