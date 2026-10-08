"""Offline Stage04 deadline gate under real Stage05 IPC traffic. Keeps FAIL visible."""
import sys,pathlib,threading,subprocess,time,json,os
ROOT=pathlib.Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'Tests'))
from Stage05IpcIntegration import Client
out=ROOT/'local/stage05/ipc-load';out.mkdir(parents=True,exist_ok=True)
endpoint='EntropyAudioCore.Stage05.Bench.'+str(os.getpid());count=[0];errors=[];stop=threading.Event()
core=subprocess.Popen([str(ROOT/'build/msvc-stage05/EntropyAudioCore.exe'),'--control-server','--endpoint',endpoint,'--offline-test-signal','--run-seconds','180'],stdout=open(out/'core.txt','w'),stderr=open(out/'core-errors.txt','w'))
def traffic():
 try:
  client=Client(endpoint)
  while not stop.is_set():
   r=client.call('telemetry')
   if not r['ok']:raise RuntimeError(r)
   count[0]+=1
   time.sleep(.01)
  client.close()
 except Exception as error:errors.append(str(error))
thread=threading.Thread(target=traffic);thread.start()
try:
 with open(out/'execution.txt','w') as log:
  result=subprocess.run([str(ROOT/'build/msvc-stage05/EntropyStage04Validation.exe'),str(out),'--mmcss-benchmark'],stdout=log,stderr=subprocess.STDOUT)
 stop.set();thread.join(timeout=10)
 (out/'traffic.json').write_text(json.dumps({'benchmarkExit':result.returncode,'telemetryRequests':count[0],'trafficErrors':errors,'physicalOutput':False},indent=2))
 print('Stage04 realtime gate under IPC:', 'PASS' if result.returncode==0 else 'FAIL', 'exit',result.returncode,'telemetry requests',count[0], 'errors',errors)
finally:
 stop.set();thread.join(timeout=10);core.terminate();core.wait(timeout=10)
sys.exit(result.returncode)
