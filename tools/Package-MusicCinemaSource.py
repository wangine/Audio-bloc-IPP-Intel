"""Source-only archive with deterministic file ordering and per-file SHA256."""
import hashlib,json,pathlib,sys,zipfile,subprocess,os
root=pathlib.Path(sys.argv[1]).resolve();output=pathlib.Path(sys.argv[2]).resolve();reference=pathlib.Path(sys.argv[3]).resolve()
if output.exists():raise RuntimeError('Preserve existing source archive')
entries={}
forbidden={'.exe','.dll','.lib','.pdb','.obj','.o','.a','.entropy','.wav','.pcm','.raw','.pyc','.zip','.7z'}
def walk_error(error):raise error
def collect(base,prefix='',allow_local=False):
 # Prune excluded directories before descent, including unrelated worktrees
 # and their long Windows paths. Errors in included sources remain fatal.
 excluded={'.git','build','logs','state','release','node_modules','__pycache__','.codex','.agents'}
 for parent,dirs,files in os.walk(base,topdown=True,onerror=walk_error,followlinks=False):
  dirs[:]=[d for d in dirs if d.lower() not in excluded and not d.lower().startswith(('build-','build_')) and not (not allow_local and pathlib.Path(parent)==base and d.lower() in {'local','validation'})]
  for d in dirs:
   if (pathlib.Path(parent)/d).is_symlink():raise RuntimeError('Symlink not packaged: '+str(pathlib.Path(parent)/d))
  for name in files:
   p=pathlib.Path(parent)/name;rel=p.relative_to(base)
   if not prefix and rel.as_posix()=='source-manifest.json':continue
   if p.suffix.lower() in forbidden:continue
   if p.is_symlink():raise RuntimeError('Symlink not packaged: '+str(p))
   entries[prefix+rel.as_posix()]=p
collect(root)
collect(root/'local/thiran-baseline','local/thiran-baseline/',True)
collect(reference/'dependencies/SRC','local/stage04-ref/Stage3/native-reference/dependencies/SRC/',True)
manifest={'version':sys.argv[4],'sourceBaseCommit':'2f3c45aa71cbf1bc352aece848060dc7523b0649','sourceBaseArchiveSha256':'AC20227D613AE12460789419EBA566CE0729070616F10F3BFA1632E9D6AABA81','files':[]}
manifest.update(sourceWorkingTreeBaseCommit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),sourceModifiedLocal=True)
with zipfile.ZipFile(output,'x',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 for name,p in sorted(entries.items()):
  data=p.read_bytes();manifest['files'].append({'path':name,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest().upper()});z.writestr(name,data)
 z.writestr('source-manifest.json',json.dumps(manifest,indent=2))
print(json.dumps({'files':len(entries),'zip':str(output),'sha256':hashlib.sha256(output.read_bytes()).hexdigest().upper()}))
