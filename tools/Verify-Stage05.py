"""Protected source/stable UI, generated adapter and executable integrity."""
import csv,hashlib,json,pathlib,subprocess,re
ROOT=pathlib.Path(__file__).resolve().parents[1];OUT=ROOT/'validation/stage05';OUT.mkdir(parents=True,exist_ok=True)
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest().upper()
records=[]
for row in csv.DictReader((ROOT/'validation/stage04-blockers/source-integrity.csv').open(encoding='utf-8-sig')):
 p=ROOT/row['Path'];expected=row['Stage04SHA256'];exists=p.exists();same=exists and digest(p)==expected
 if not exists:
  p=ROOT/'local/stage04-ref/Stage3/native-reference'/row['Path'];exists=p.exists();same=exists and digest(p)==expected
 records.append(dict(path=row['Path'],baseline=expected,current=digest(p) if exists else 'MISSING',identical=same,authorizedIntegration=row['Path'].replace('\\','/')=='DSP/Crossover/include/PluginProcessor.h'))
header=ROOT/'DSP/Crossover/include/PluginProcessor.h';old=subprocess.check_output(['git','show','1fb726bc103cbb630ea55ca27647e85d897c637d:DSP/Crossover/include/PluginProcessor.h'],cwd=ROOT).decode().replace('\r\n','\n');current=header.read_text()
# Remove only the documented optional owner view seam; require exact remainder.
current=re.sub(r'    // Native-host integration only:[\s\S]*?    bool outputRoleActive[^\n]*\n','',current)
current=re.sub(r'    inline static thread_local const EntropyCrossoverAudioProcessor\* hostBlockParameterOwner[^\n]*\n','',current)
current=re.sub(r'    inline static thread_local const float\* hostBlockParameterView[^\n]*\n','',current)
current=current.replace('if(hostBlockParameterOwner==this&&hostBlockParameterView)return hostBlockParameterView[index];','')
assert current==old,'Unexpected protected Crossover header change'
assert all(r['identical'] or r['authorizedIntegration'] for r in records),[r for r in records if not r['identical'] and not r['authorizedIntegration']]
with (OUT/'protected-source-sha256.csv').open('w',newline='') as f:w=csv.DictWriter(f,records[0].keys());w.writeheader();w.writerows(records)
stable=[]
for file,column in [('local/stage05-ui-baseline.csv','Hash'),('validation/stage04-blockers/stable-stage3-integrity.csv','BaselineSHA256')]:
 for row in csv.DictReader((ROOT/file).open(encoding='utf-8-sig')):
  p=pathlib.Path(row['Path']);same=digest(p)==row[column];stable.append(dict(path=str(p),baseline=row[column],current=digest(p),identical=same));assert same,str(p)
with (OUT/'stable-stage3-sha256.csv').open('w',newline='') as f:w=csv.DictWriter(f,stable[0].keys());w.writeheader();w.writerows(stable)
original=(ROOT/'DSP/Crossover/compat/src/CrossoverModule.cpp').read_text();generated=(ROOT/'build/msvc-stage05/OriginalCrossoverControl.inc').read_text();assert generated==original.replace('std::unique_ptr<Engine> engine;','std::shared_ptr<Engine> engine;').replace('entropy_module_get_api','entropy_storage_control_get_api'),'Adapter changed original implementation'
exe=[]
for file in ['build/msvc-stage05/EntropyAudioCore.exe','build/qt-stage05/EntropyUI.exe','build/qt-stage05/EntropyStage05Ui.exe']:
 p=ROOT/file;exe.append(dict(path=file,sha256=digest(p)))
with (OUT/'executables-sha256.csv').open('w',newline='') as f:w=csv.DictWriter(f,exe[0].keys());w.writeheader();w.writerows(exe)
imports=subprocess.check_output(['C:/Qt/Tools/mingw1310_64/bin/objdump.exe','-p',str(ROOT/'build/qt-stage05/EntropyUI.exe')],text=True);dlls=re.findall(r'DLL Name: (.+)',imports);assert not any(any(x in dll.lower() for x in ['ipp','crossover','clocksync','pcm24','renderengine','headroom']) for dll in dlls)
(OUT/'ui-imports.json').write_text(json.dumps(dlls,indent=2))
print('PASS protected files',len(records),'documented header seam only; stable Stage3 files',len(stable),'unchanged; original adapter body exact; UI imports clean')
print(json.dumps(exe,indent=2))
