"""Immutable source, stable Stage3, original transport and UI binary checks."""
import pathlib,hashlib,subprocess,csv,json
root=pathlib.Path(__file__).resolve().parents[1];out=root/'validation/stage07';out.mkdir(parents=True,exist_ok=True)
def digest(b):return hashlib.sha256(b).hexdigest().upper()
protected=[]
for p in subprocess.check_output(['git','ls-tree','-r','--name-only','5fa4c636b47b37ede5a18ebbce80e7a35ac2abcc','DSP','Egress','Devices','Shared','Transport'],cwd=root,text=True).splitlines():
 old=subprocess.check_output(['git','show','5fa4c636b47b37ede5a18ebbce80e7a35ac2abcc:'+p],cwd=root);new=(root/p).read_bytes();same=old.replace(b'\r\n',b'\n')==new.replace(b'\r\n',b'\n');protected.append(dict(path=p,baselineGitSHA256=digest(old),workingSHA256=digest(new),identical=same));assert same,p
stable=[]
for r in csv.DictReader((root/'validation/stage06/stable-stage3-sha256.csv').open(encoding='utf-8-sig')):
 current=digest(pathlib.Path(r['path']).read_bytes());stable.append(dict(path=r['path'],baseline=r['currentSha256'],current=current,identical=current==r['currentSha256']));assert current==r['currentSha256'],r['path']
for name,data in [('protected-source-sha256.csv',protected),('stable-stage3-sha256.csv',stable)]:
 with (out/name).open('w',newline='',encoding='utf-8') as f:w=csv.DictWriter(f,data[0].keys());w.writeheader();w.writerows(data)
imports=subprocess.check_output(['C:/Qt/Tools/mingw1310_64/bin/objdump.exe','-p',str(root/'build/qt-stage07/EntropyUI.exe')],text=True);assert not any(x in imports.lower() for x in ['dll name: ipp','dll name: crossover','dll name: headroom','dll name: clocksync','dll name: pcm24','dll name: renderengine'])
exe={p:digest((root/p).read_bytes()) for p in ['build/msvc-stage07/EntropyAudioCore.exe','build/qt-stage07/EntropyUI.exe']};(out/'executables-sha256.json').write_text(json.dumps(exe,indent=2)+'\n')
print('PASS protected source files:',len(protected),'stable Stage3:',len(stable),'UI imports IPC-only');print(json.dumps(exe,indent=2))
