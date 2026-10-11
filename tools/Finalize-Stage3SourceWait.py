"""Save verified local evidence and payload archives; never commits or uploads."""
import hashlib,json,pathlib,re,shutil,subprocess,sys,zipfile
ROOT=pathlib.Path(__file__).resolve().parents[1]
VERSION='0.1.9-v2-r2'
OUT=ROOT/'release'/VERSION
EVIDENCE=ROOT/'validation/music-cinema-v2'
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest().upper()
def load(path):return json.loads(path.read_text(encoding='utf-8-sig'))
def write(path,data):path.write_text(json.dumps(data,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
report_path=ROOT/'docs/VALIDATION-STAGE3-SOURCE-WAIT-r2.json'
if report_path.exists():raise RuntimeError('Preserve finalized report')
native=[];live=[]
for variant in ('32k','64k'):
 path=EVIDENCE/('source-wait-final-full-'+variant+'.txt')
 text=path.read_text();match=re.search(r'(\d+)% tests passed, (\d+) tests failed out of (\d+)',text)
 if not match or match[2]!='1' or re.findall(r'^\s*\d+ - (\S+) \(Failed\)',text,re.M)!=['EntropyStage04Offline']:raise RuntimeError('Unexpected native failure')
 native.append(dict(variant=variant,passed=int(match[3])-1,total=int(match[3]),inheritedFailure='EntropyStage04Offline',reportSha256=sha(path)))
 path=EVIDENCE/('source-wait-live-'+variant+'-complete.summary.json');audit=load(path)
 if not audit['PASS']:raise RuntimeError('Live audit failed')
 final=audit['final']
 if final['liveFault'] or not final['physicalArmed']:raise RuntimeError('Live final state invalid')
 holds=[event for event in audit['events'] if 'heldSeconds' in event]
 if len(holds)!=4:raise RuntimeError('Waiting evidence incomplete')
 live.append(dict(variant=variant,corePid=audit['corePid'],waitingIntervals=holds,sourceWaitCount=final['sourceWaitCount'],sourceResumeCount=final['sourceResumeCount'],inputFaultHistory=final['inputs']['music']['faults'],musicGaps=final['inputs']['music']['gaps'],pcmClippedBlocks=[d['pcmClippedBlocks'] for d in final['domains']],nonzeroPcmBlocks=[d['nonzeroPcmBlocks'] for d in final['domains']],steadySeconds=60 if variant=='64k' else 30,summarySha256=sha(path)))
ready=load(EVIDENCE/'source-wait-final-64k-ready.json')
if not ready['PASS']:raise RuntimeError('Final listening state not verified')
owner=load(EVIDENCE/'source-wait-reference-owner.json')
original_profile=pathlib.Path('C:/Users/bobi/Documents/entropy.entropy')
if sha(original_profile)!=owner['profileSha256']:raise RuntimeError('Original profile changed')
report=dict(version=VERSION,date='2026-10-11',scope='Local retained-user-ARM/source recovery; no full Stage04/V07 or original TIDAL PCM qualification',sourceWorkingTreeBaseCommit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),sourceModifiedLocal=True,gitUploaded=False,native=native,live=live,finalListeningCorePid=ready['pid'],finalListeningVariant='64k',originalProfileSha256=sha(original_profile),compiler='MSVC 19.51.36257.0 x64',ipp='C:/Program Files (x86)/Intel/oneAPI/ipp/2026.0',ui='Qt 6.11.2 / GNU 13.1.0; separate IPC process',protectedInventory=dict(checked=72,unchanged=70,intentionallyChanged=['Engine/Egress/LiveSinkWorkers.cpp','Engine/Egress/LiveSinkWorkers.h'],originalDspAlgorithmsChanged=False),uiGeneralFixture='EntropyStage05Ui failure also reproduced with accepted Core; retained',limitations=['No ten-hour wall-clock test; wait policy has no timeout','Real Cinema decoder pending','No analog skew/whole-machine-load guarantee','Intentional Stage1 downtime can overflow finite driver ring; raw loss metadata retained','No TIDAL PCM identity or Shared/Exclusive equality claim','V2 actual access metadata remains unverified'])
write(report_path,report)
for variant in ('32k','64k'):
 bundle=OUT/f'EntropyAudioCore-{VERSION}-{variant}-win-x64'
 manifest=load(bundle/'manifest.json')
 if sha(bundle/'runtime/EntropyAudioCore.exe')!=manifest['coreSha256'] or sha(bundle/'runtime/EntropyUI.exe')!=manifest['uiSha256']:raise RuntimeError('Runtime changed after test')
 shutil.copy2(report_path,bundle/report_path.name);shutil.copy2(ROOT/'docs/STAGE3-SOURCE-WAIT-r2.md',bundle/'VALIDATION.md')
 for name in ('Stage3-SourceWait-Live.py','Stage3-SourceWait-Switch.py'):shutil.copy2(ROOT/'tools'/name,bundle/'tools'/name)
 (bundle/'Tests').mkdir(exist_ok=True);shutil.copy2(ROOT/'Tests/Stage05IpcIntegration.py',bundle/'Tests/Stage05IpcIntegration.py')
 evidence=bundle/'evidence';evidence.mkdir()
 for name in ('source-wait-final-full-'+variant+'.txt','source-wait-live-'+variant+'-complete.summary.json','source-wait-live-'+variant+'-complete.jsonl','source-wait-smoke-'+variant+'.txt','source-wait-final-EntropyMusicCinemaUi.txt','source-wait-final-EntropyStage017Ui.txt','source-wait-final-EntropyStage07Ui.txt','source-wait-EntropyStage05Ui-baseline.txt'):
  shutil.copy2(EVIDENCE/name,evidence/name)
 manifest.update(liveValidation='PASS bounded local lifecycle audit; see VALIDATION.md',validationReportSha256=sha(report_path))
 files=[]
 for path in sorted(bundle.rglob('*')):
  if not path.is_file():continue
  relative=path.relative_to(bundle)
  if relative.parts[0] in ('logs','state','profiles') or path.name=='manifest.json':continue
  files.append(dict(path=relative.as_posix(),bytes=path.stat().st_size,sha256=sha(path)))
 manifest['files']=files;write(bundle/'manifest.json',manifest)
 archive=OUT/f'EntropyAudioCore-{VERSION}-{variant}-win-x64.zip'
 if archive.exists():raise RuntimeError('Preserve runtime archive')
 with zipfile.ZipFile(archive,'x',zipfile.ZIP_DEFLATED) as z:
  for name in [item['path'] for item in files]+['manifest.json']:z.write(bundle/name,bundle.name+'/'+name)
 with zipfile.ZipFile(archive) as z:
  for item in files:
   if hashlib.sha256(z.read(bundle.name+'/'+item['path'])).hexdigest().upper()!=item['sha256']:raise RuntimeError('Archive payload mismatch')
 print(json.dumps(dict(archive=str(archive),sha256=sha(archive),files=len(files))))
source=OUT/f'EntropyAudioCore-{VERSION}-source.zip'
subprocess.run([sys.executable,str(ROOT/'tools/Package-MusicCinemaSource.py'),str(ROOT),str(source),str(ROOT/'local/stage04-ref/Stage3/native-reference'),VERSION],check=True)
with zipfile.ZipFile(source) as z:
 source_manifest=json.loads(z.read('source-manifest.json'))
 for item in source_manifest['files']:
  if hashlib.sha256(z.read(item['path'])).hexdigest().upper()!=item['sha256']:raise RuntimeError('Source archive mismatch')
(OUT/'SHA256SUMS.txt').write_text(''.join(sha(path)+'  '+path.name+'\n' for path in sorted(OUT.glob('*.zip'))),encoding='ascii')
print(json.dumps(dict(finalized=True,report=str(report_path),sourceFiles=len(source_manifest['files']))))
