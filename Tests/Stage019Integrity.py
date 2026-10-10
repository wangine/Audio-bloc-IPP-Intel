"""Stage019 integration-only proof; no audio benchmark or stable-source writes."""
import hashlib,json,pathlib,subprocess,re
root=pathlib.Path(__file__).resolve().parents[1]
base='41f55c4a556ae21e35ee8f711d853f6ceaa2b12c'
def original(path):return subprocess.check_output(['git','-C',str(root),'show',base+':'+path]).replace(b'\r\n',b'\n')
protected=[]
for path in subprocess.check_output(['git','-C',str(root),'ls-tree','-r','--name-only',base],text=True).splitlines():
 if path.startswith(('DSP/','UI/','Egress/PCM24/','Egress/Guardian/','Engine/Input/','Transport/')) or path in ['Engine/AudioGraph.cpp','Egress/ClockSync/src/EgressClockEstimator.cpp','Egress/ClockSync/src/EgressRatioController.cpp','Egress/ClockSync/include/EntropyBandlimitedAsrc.h','Egress/WasapiSink/src/DacSinkCore.cpp']:
  before=original(path);current=(root/path).read_bytes().replace(b'\r\n',b'\n');assert before==current,path
  protected.append({'path':path,'sha256':hashlib.sha256((root/path).read_bytes()).hexdigest().upper()})
def body(text,name):
 start=text.index(name);brace=text.index('{',start);depth=1;end=brace+1
 while depth:
  depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[start:end]
path='Egress/ClockSync/src/EgressClockEngine.cpp';before=original(path).decode();after=(root/path).read_text(encoding='utf-8-sig')
for name in ['EgressClockEngine::interpolate','EgressClockEngine::push','EgressClockEngine::writeNominal','EgressClockEngine::configure']:
 assert body(before,name)==body(after,name),name
render=body(after,'EgressClockEngine::render')
render=re.sub(r'\n        lastResetReason_=[1-4];','',render).replace('lastEstimate_=estimate;','')
assert render==body(before,'EgressClockEngine::render'),'original render math modified beyond telemetry copies'
result={'PASS':True,'baseline':base,'protectedFileCount':len(protected),'protectedFiles':protected,'originalClockRenderMathIdentical':True,'sincEstimatorControllerIdentical':True}
(root/'validation/stage019/source-integrity.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps({k:v for k,v in result.items() if k!='protectedFiles'}))
