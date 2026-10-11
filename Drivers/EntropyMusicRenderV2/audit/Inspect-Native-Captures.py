"""Offline diagnosis only. Never upgrades failed official acceptance to PASS.

Compare *all* valid FP64 blocks to PCM from the already-verified raw capture,
while separately preserving every inserted clock block and its timeline.
"""
import array,json,pathlib,struct,sys
def inspect(folder):
    folder=pathlib.Path(folder);render=json.loads((folder/'render.json').read_text(encoding='utf-8-sig'))
    assert render['rate']==192000
    pcm=bytearray();source_frames=0;raw_footer=False
    with (folder/'core-raw.bin').open('rb') as file:
        while header:=file.read(32):
            assert len(header)==32
            magic,version,kind,size,overflow,failed=struct.unpack('<IIIIQQ',header)
            assert magic==0x41524d45 and version==1
            if kind==2:
                assert size==overflow==failed==0 and not file.read(1);raw_footer=True;break
            body=file.read(size);assert kind==1 and len(body)==size and size>=160
            generation,sequence,first=struct.unpack_from('<QQQ',body,72)
            count,flags,mode,payload=struct.unpack_from('<IIII',body,120)
            assert generation==render['generation'] and first==source_frames and payload==size-160
            assert payload==count*(render['containerBits']//4)
            pcm.extend(body[160:]);source_frames+=count
    assert raw_footer and source_frames==render['emittedFrames']
    words=array.array('h' if render['containerBits']==16 else 'i');words.frombytes(pcm)
    if render['containerBits']==16:expected=[array.array('d',(v/32768.0 for v in words[c::2])) for c in range(2)]
    else:expected=[array.array('d',((v>>8)/8388608.0 for v in words[c::2])) for c in range(2)]
    valid_frames=blocks=invalid=interior=mismatches=extra=0;runs=[];footer=False;errors=0;last_sequence=0
    with (folder/'core-output-fp64.bin').open('rb') as file:
        header=file.read(32);magic,version,kind,size,e,reserved=struct.unpack('<IIIIQQ',header)
        assert magic==0x464d5245 and version==1 and kind==1 and size==256 and not(e or reserved)
        session=file.read(256);frequency=struct.unpack_from('<Q',session,96)[0]
        while header:=file.read(32):
            magic,version,kind,size,e,reserved=struct.unpack('<IIIIQQ',header)
            assert magic==0x464d5245 and version==1 and not reserved
            if kind==3:
                assert size==0 and not file.read(1);footer=True;errors=e;break
            body=file.read(size);assert kind==2 and size==16512 and len(body)==size and not e
            block=body[:128];seq,first=struct.unpack_from('<QQ',block,24)
            frames,validity,discontinuity=struct.unpack_from('<III',block,88)
            qpc=struct.unpack_from('<Q',block,72)[0]
            assert frames==1024 and first==(seq-1)*1024 and (not last_sequence or seq==last_sequence+1)
            assert validity in (0,1) and block[:16]==session[16:32]
            last_sequence=seq;blocks+=1
            if not validity:
                assert body[128:]==bytes(16384)
                domain='startup' if not valid_frames else 'interior' if valid_frames<source_frames else 'tail'
                invalid+=1024;interior+=1024 if domain=='interior' else 0
                if runs and runs[-1]['lastSequence']==seq-1 and runs[-1]['domain']==domain:
                    runs[-1]['lastSequence']=seq;runs[-1]['clockFrames']+=1024
                else:runs.append(dict(domain=domain,firstSequence=seq,lastSequence=seq,sourceFrameAtGap=valid_frames,clockFrames=1024,qpc=qpc))
            else:
                assert struct.unpack_from('<Q',block,48)[0]==render['generation'] and struct.unpack_from('<I',block,112)[0]==192000
                if valid_frames+1024>source_frames:extra+=1024
                else:
                    for c in range(2):
                        actual=body[128+c*8192:128+(c+1)*8192]
                        wanted=expected[c][valid_frames:valid_frames+1024].tobytes()
                        if actual!=wanted:mismatches+=sum(actual[i:i+8]!=wanted[i:i+8] for i in range(0,8192,8))
                valid_frames+=1024
    result=dict(scope='DIAGNOSTIC_ONLY; concatenating valid blocks does not repair chronology or qualify completion',sourceFrames=source_frames,validFp64Frames=valid_frames,allValidSamplesCompared=valid_frames==source_frames and not extra,mismatchedBinary64Samples=mismatches,extraFrames=extra,publicationBlocks=blocks,lastPublicationSequence=last_sequence,invalidClockFrames=invalid,interiorInvalidFrames=interior,footer=footer,subscriberErrors=errors,qpcFrequency=frequency,invalidRuns=runs)
    (folder/'full-output-diagnosis.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:v for k,v in result.items() if k!='invalidRuns'}))
if __name__=='__main__':
    for argument in sys.argv[1:]:inspect(argument)
