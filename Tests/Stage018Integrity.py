"""Display-only source proof against published v0.1.6; no audio benchmark."""
import hashlib,json,pathlib,subprocess,sys,re
root=pathlib.Path(__file__).resolve().parents[1]
base='ae4b90339e7c60f99d09ba5cac412822af524ada'
def original(path):return subprocess.check_output(['git','-C',str(root),'show',base+':'+path])
# Reuse the historical function-body parser, without executing its release-specific checks.
parser=(root/'Tests/Stage017Integrity.py').read_text(encoding='utf-8')
namespace={"re":re}
exec(parser[parser.index('def methods('):parser.index("path = 'DSP/Crossover/src/PluginProcessor.cpp'")],namespace)
methods=namespace['methods']
path='DSP/Crossover/src/PluginProcessor.cpp'
before=methods(original(path).decode());after=methods((root/path).read_text(encoding='utf-8'))
changed=[name for name,body in before.items() if after.get(name)!=body]
allowed={'prepareToPlay','captureLiveSpectrumBlock','publishControlSnapshot','beginRt','endRt'}
assert set(changed)<=allowed,changed
for name in ['processBlock','readDelayedSample','compileCoefficients','updateFir','buildBassLane','buildComplexPcaLane','setLowPass','setHighPass','setAllPass']:
 assert before[name]==after[name],name
protected=[]
excluded={path,'DSP/Crossover/include/PluginProcessor.h','DSP/Crossover/compat/src/CrossoverModule.cpp'}
for path in subprocess.check_output(['git','-C',str(root),'ls-tree','-r','--name-only',base],text=True).splitlines():
 if (path.startswith(('DSP/','Egress/','Engine/Input/')) and path not in excluded) or path in ['Engine/AudioGraph.cpp','Engine/AudioScheduler.cpp','Engine/Egress/NativeEgress.cpp','UI/Legacy/R10/HeadroomPage.cpp']:
  old=original(path);current=(root/path).read_bytes();assert old.replace(b'\r\n',b'\n')==current.replace(b'\r\n',b'\n'),path
  protected.append({'path':path,'sha256':hashlib.sha256(current).hexdigest().upper()})
result={'PASS':True,'baseline':base,'unchangedCrossoverMethods':len(before)-len(changed),'visualInstrumentationMethods':changed,'protectedFiles':protected}
if len(sys.argv)>1:pathlib.Path(sys.argv[1]).write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps({'PASS':True,'unchangedCrossoverMethods':result['unchangedCrossoverMethods'],'protectedFiles':len(protected),'baseline':base}))

