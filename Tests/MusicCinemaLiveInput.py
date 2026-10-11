"""Real Stage2 input, isolated candidate Core, no DAC and no upstream controls."""
import json, os, pathlib, subprocess, sys, time, uuid
from Stage05IpcIntegration import Client
core=pathlib.Path(sys.argv[1]);out=pathlib.Path(sys.argv[2]);seconds=int(sys.argv[3]);out.mkdir(parents=True,exist_ok=True)
endpoint='EntropyAudioCore.MusicCinema.LiveInput.'+uuid.uuid4().hex
p=subprocess.Popen([str(core),'--control-server','--input-only','--endpoint',endpoint,'--run-seconds',str(seconds+20)],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
c=None;probe=None
try:
 c=Client(endpoint);deadline=time.monotonic()+5
 while True:
  initial=c.call('telemetry',viewContractVersion=2)['data']
  if initial['inputs']['music']['state']=='RECEIVING':break
  if time.monotonic()>deadline:raise AssertionError('Real Stage2 Music publisher unavailable: '+initial['inputState'])
  time.sleep(.02)
 music_endpoint=initial['inputs']['music']['endpoint'];probe=subprocess.Popen([str(core.parent/'EntropyMusicCinemaLiveInput.exe'),music_endpoint,str(seconds)],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
 samples=[];until=time.monotonic()+seconds
 while time.monotonic()<until:
  s=c.call('telemetry',viewContractVersion=2)['data'];samples.append({k:s[k] for k in ('blocks','armed','inputState','inputs','scheduler')});assert not s['armed'];time.sleep(.05)
 stdout,stderr=probe.communicate(timeout=5);native=json.loads(stdout);final=samples[-1]
 music=final['inputs']['music'];start=initial['inputs']['music'];delta=int(final['blocks'])-int(initial['blocks'])
 assert probe.returncode==0,(native,stderr.decode(errors='replace'))
 assert delta>=1000 and int(music['gaps'])==0 and int(music['faults'])==0,(delta,music)
 assert music['channels']==3 and music['rate']==192000 and music['layout']==4
 assert start['sourceSession']==music['sourceSession']==native['sourceSession']
 assert not c.call('arm')['ok']
 c.call('shutdown');c.close();c=None;p.wait(timeout=6);assert p.returncode==0
 receipt={'verdict':'PASS','seconds':seconds,'graphBlocks':delta,'initial':initial['inputs'],'final':final['inputs'],'nativeSubscriber':native,'physicalArmed':False,'upstreamCommands':0,'audioActive':max(native['peakL'],native['peakR'],native['peakC'])>0,'cinemaReal':'PENDING'}
 (out/'live-input.json').write_text(json.dumps(receipt,indent=2));(out/'live-input-snapshots.json').write_text(json.dumps(samples,indent=2));print(json.dumps({'verdict':'PASS','graphBlocks':delta,'nativeBlocks':native['blocks'],'peaks':[native['peakL'],native['peakR'],native['peakC']],'audioActive':receipt['audioActive']}))
finally:
 if c:c.close()
 for proc in (p,probe):
  if proc and proc.poll() is None:proc.terminate();proc.communicate(timeout=5)
