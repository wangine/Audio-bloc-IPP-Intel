import ctypes,json,pathlib,subprocess,sys,time,uuid,zipfile,hashlib,os
root=pathlib.Path(__file__).resolve().parents[1];release=pathlib.Path(sys.argv[1]).resolve();evidence=root/'validation/music-cinema-v2';sys.path.insert(0,str(root/'Tests'))
from Stage05IpcIntegration import Client
sha=lambda b:hashlib.sha256(b).hexdigest().upper()
results=[]
for variant in ('32k','64k'):
 bundle=release/f'EntropyAudioCore-0.1.9-v2-{variant}-win-x64';manifest=json.loads((bundle/'manifest.json').read_text(encoding='utf-8-sig'))
 with zipfile.ZipFile(release/(bundle.name+'.zip')) as z:
  for f in manifest['files']:
   data=(bundle/f['path']).read_bytes();assert len(data)==f['bytes'] and sha(data)==f['sha256'];assert z.read(bundle.name+'/'+f['path'])==data
  assert not any('/logs/' in p or ('/notices/' not in p and pathlib.Path(p).suffix.lower() in ('.entropy','.lib','.obj','.pdb','.wav','.raw','.pcm')) for p in z.namelist())
 # Both launch and stop must refuse the old version at the default endpoint.
 rejected=[]
 for script in ('Start-MusicCinema.ps1','Stop-MusicCinema.ps1'):
  r=subprocess.run(['powershell.exe','-NoLogo','-NoProfile','-ExecutionPolicy','Bypass','-File',str(bundle/'tools'/script)],capture_output=True,timeout=20,creationflags=subprocess.CREATE_NO_WINDOW)
  assert r.returncode!=0,(script,r.stdout,r.stderr);rejected.append({'script':script,'exit':r.returncode,'output':r.stdout.decode(errors='replace').strip()})
 name='EntropyAudioCore.PackageReopen.'+uuid.uuid4().hex
 core=bundle/'runtime/EntropyAudioCore.exe';process=subprocess.Popen([str(core),'--control-server','--stage2-input','--input-only','--endpoint',name,'--run-seconds','30'],stdout=subprocess.PIPE,stderr=subprocess.PIPE,creationflags=subprocess.CREATE_NO_WINDOW)
 client=None
 try:
  client=Client(name);meta=client.call('describe')['data'];assert meta['version']=='0.1.9-v2' and not meta['physicalArmAllowed'] and meta['inputOnly']
  deadline=time.monotonic()+5
  while True:
   initial=client.call('telemetry',viewContractVersion=2)['data']
   if int(initial['blocks'])>=20:break
   if time.monotonic()>deadline:raise AssertionError(initial['inputState'])
   time.sleep(.02)
  source=initial['inputs']['music']['sourceSession'];ui_results=[]
  client.close();client=None # Core IPC serves one presentation connection at a time; audio stays running.
  env=os.environ.copy();env['PATH']=str(bundle/'runtime')+';'+env.get('PATH','');env['QT_QPA_PLATFORM']='offscreen';env['QT_PLUGIN_PATH']=str(bundle/'runtime');env['QT_QPA_PLATFORM_PLUGIN_PATH']=str(bundle/'runtime/platforms')
  for attempt in (1,2):
   capture=evidence/f'packaged-{variant}-reopen-{attempt}.png'
   r=subprocess.run([str(bundle/'runtime/EntropyUI.exe'),'--endpoint',name,'--smoke-seconds','2','--capture-ui',str(capture)],capture_output=True,env=env,timeout=10,creationflags=subprocess.CREATE_NO_WINDOW)
   assert r.returncode==0,(r.stdout,r.stderr);assert capture.exists();ui_results.append({'attempt':attempt,'exit':r.returncode,'capture':capture.name})
  client=Client(name);final=client.call('telemetry',viewContractVersion=2)['data'];assert int(final['blocks'])>int(initial['blocks']) and not final['armed'] and source==final['inputs']['music']['sourceSession'];assert int(final['inputs']['music']['gaps'])==0 and int(final['inputs']['music']['faults'])==0
  client.close();client=None
  r=subprocess.run(['powershell.exe','-NoLogo','-NoProfile','-ExecutionPolicy','Bypass','-File',str(bundle/'tools/Stop-MusicCinema.ps1'),'-Endpoint',name],capture_output=True,timeout=15,creationflags=subprocess.CREATE_NO_WINDOW);assert r.returncode==0,(r.stdout,r.stderr);process.wait(timeout=8);assert process.returncode==0
  results.append({'variant':variant,'manifestFiles':len(manifest['files']),'manifestAndZip':'PASS','refusedOtherVersion':rejected,'uiReopenSameCore':ui_results,'blocksAdvanced':int(final['blocks'])-int(initial['blocks']),'physicalArmed':False,'safeOwnPackageStop':'PASS'})
 finally:
  if client:client.close()
  if process.poll() is None:process.terminate();process.communicate(timeout=5)
with zipfile.ZipFile(release/'EntropyAudioCore-0.1.9-v2-source.zip') as z:
 source=json.loads(z.read('source-manifest.json'))
 for f in source['files']:data=z.read(f['path']);assert len(data)==f['bytes'] and sha(data)==f['sha256']
 assert any(p.startswith('local/stage04-ref/') for p in z.namelist()) and any(p.startswith('local/thiran-baseline/') for p in z.namelist())
 for p in z.namelist():assert pathlib.Path(p).suffix.lower() not in ('.entropy','.exe','.dll','.lib','.pdb','.obj','.wav','.raw','.pcm')
checksums=(release/'SHA256SUMS.txt').read_text().splitlines()
for line in checksums:
 digest,name=line.split('  ',1);assert sha((release/name).read_bytes())==digest
receipt={'verdict':'PASS','runtime':results,'sourceManifestFiles':len(source['files']),'sourceManifestAndZip':'PASS','checksums':'PASS','physicalOutputOpened':False}
(release/'PACKAGE-VERIFICATION.json').write_text(json.dumps(receipt,indent=2));print(json.dumps({'verdict':'PASS','runtimeFiles':[r['manifestFiles'] for r in results],'sourceFiles':len(source['files']),'uiReopen':'PASS both variants','stopOwnership':'PASS'}))
