"""Offline causal control: same retained bytes, continuous child versus forced reset."""
import hashlib
import json
import pathlib
import subprocess
import numpy as np

p = pathlib.Path(__file__).parent
raw_path = p/'v18-dts-carrier-48000-s16le-stereo.raw'
trace = np.genfromtxt(p/'v18-dts-decoder-trace.csv', delimiter=',', names=True, dtype=np.uint64)
packet = trace[(trace['kind']==6) & ((trace['c']&1)!=0)][0]
cut = int(packet['a'])
raw = raw_path.read_bytes()
parts = [p/'dts-before-warning.raw', p/'dts-after-warning.raw']
parts[0].write_bytes(raw[:cut])
parts[1].write_bytes(raw[cut:])
assert parts[0].read_bytes()+parts[1].read_bytes()==raw

def decode(path, label):
    command = [r'C:\Program Files\Entropy Audio\Surround Decoder\ffmpeg.exe',
        '-nostdin','-hide_banner','-loglevel','error','-probesize','32',
        '-analyzeduration','0','-fflags','nobuffer','-flags','low_delay',
        '-err_detect','explode','-xerror','-f','spdif','-i',str(path),
        '-map','0:a:0','-ar','48000','-ac','6','-c:a','pcm_f32le','-f','f32le','pipe:1']
    count=0
    digest=hashlib.sha256()
    with (p/(label+'-offline-errors.log')).open('wb') as errors:
        child=subprocess.Popen(command,stdout=subprocess.PIPE,stderr=errors)
        while block:=child.stdout.read(1024*1024):
            count+=len(block)
            digest.update(block)
        code=child.wait()
    assert count % 24 == 0
    return dict(bytes=count,frames=count//24,sha256=digest.hexdigest(),exit=code)

full=decode(raw_path,'dts-continuous')
before=decode(parts[0],'dts-before-warning')
after=decode(parts[1],'dts-after-warning')
result=dict(carrier_sha256=hashlib.sha256(raw).hexdigest(),reset_byte_offset=cut,
    continuous=full,before_reset=before,after_reset=after,
    reset_decoded_frame_difference=full['frames']-before['frames']-after['frames'])
(p/'dts-recovery-offline-control.json').write_text(json.dumps(result,indent=2))
print(json.dumps(result,indent=2))
