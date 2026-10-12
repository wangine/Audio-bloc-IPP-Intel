"""Preserve the actual v0.2.13 installer code/lifecycle; replace its resources.
No installation is performed here. All writes remain in this candidate directory.
"""
import ctypes as c
from ctypes import wintypes as w
import hashlib,json,pathlib,shutil,struct
root=pathlib.Path(__file__).resolve().parent.parent
installed=pathlib.Path(r'C:\Program Files\Entropy Audio\Surround Decoder')
template=root/'tools'/'InstallerTemplate-v0.2.13.exe'
release=root/'release';release.mkdir(exist_ok=True)
original=root.parent/'Entropy-Surround-Decoder-v0.2.13-Auto-Recovery'/'Entropy Surround Decoder v0.2.13 Setup.exe'
if not template.exists():shutil.copy2(original,template)
k=c.WinDLL('kernel32',use_last_error=True)
k.LoadLibraryExW.argtypes=[w.LPCWSTR,w.HANDLE,w.DWORD];k.LoadLibraryExW.restype=w.HMODULE
k.FindResourceW.argtypes=[w.HMODULE,c.c_void_p,c.c_void_p];k.FindResourceW.restype=c.c_void_p
k.LoadResource.argtypes=[w.HMODULE,c.c_void_p];k.LoadResource.restype=c.c_void_p
k.LockResource.argtypes=[c.c_void_p];k.LockResource.restype=c.c_void_p
k.SizeofResource.argtypes=[w.HMODULE,c.c_void_p];k.SizeofResource.restype=w.DWORD
k.FreeLibrary.argtypes=[w.HMODULE]
k.BeginUpdateResourceW.argtypes=[w.LPCWSTR,w.BOOL];k.BeginUpdateResourceW.restype=w.HANDLE
k.UpdateResourceW.argtypes=[w.HANDLE,c.c_void_p,c.c_void_p,w.WORD,c.c_void_p,w.DWORD];k.UpdateResourceW.restype=w.BOOL
k.EndUpdateResourceW.argtypes=[w.HANDLE,w.BOOL];k.EndUpdateResourceW.restype=w.BOOL
def resource(path,typ,ident):
 h=k.LoadLibraryExW(str(path),None,0x22)
 if not h:raise c.WinError(c.get_last_error())
 try:
  r=k.FindResourceW(h,c.c_void_p(ident),c.c_void_p(typ))
  if not r:raise c.WinError(c.get_last_error())
  data=k.LockResource(k.LoadResource(h,r));size=k.SizeofResource(h,r)
  return c.string_at(data,size)
 finally:k.FreeLibrary(h)
def sha(data):return hashlib.sha256(data).hexdigest()
payload={101:(root/'build'/'EntropySurroundDecoderGUI_artefacts'/'Release'/'Entropy Surround Decoder.exe').read_bytes(),102:(root/'build'/'bin'/'EntropySurroundDecoder.exe').read_bytes(),103:resource(template,10,103),104:resource(template,10,104),105:(root/'THIRD-PARTY-NOTICES.txt').read_bytes(),106:(root/'README.txt').read_bytes()}
assert sha(payload[103])==sha((installed/'ffmpeg.exe').read_bytes()),'FFmpeg must remain the original runtime'
names={101:'Entropy Surround Decoder.exe',102:'EntropySurroundDecoder.exe',103:'ffmpeg.exe',104:'Entropy Audio.ico',105:'Third-Party Notices.txt',106:'README.txt'}
for ident,data in payload.items():(release/names[ident]).write_bytes(data)
old='0.2.13'.encode('utf-16le');new='0.2.21'.encode('utf-16le')
image=template.read_bytes();count=image.count(old);assert count>0
candidate=release/'Entropy Surround Decoder v0.2.21 Setup.exe';candidate.write_bytes(image.replace(old,new))
version=bytearray(resource(template,16,1).replace(old,new));offset=version.find(struct.pack('<I',0xFEEF04BD));assert offset>=0
struct.pack_into('<I',version,offset+12,(21<<16));struct.pack_into('<I',version,offset+20,(21<<16))
handle=k.BeginUpdateResourceW(str(candidate),False)
if not handle:raise c.WinError(c.get_last_error())
try:
 for (typ,ident),data in [*(( (10,i),d) for i,d in payload.items()),((16,1),bytes(version))]:
  buffer=c.create_string_buffer(data)
  if not k.UpdateResourceW(handle,c.c_void_p(typ),c.c_void_p(ident),1033,buffer,len(data)):raise c.WinError(c.get_last_error())
 if not k.EndUpdateResourceW(handle,False):raise c.WinError(c.get_last_error())
except:
 k.EndUpdateResourceW(handle,True);raise
for ident,data in payload.items():assert resource(candidate,10,ident)==data,'Installer payload verification failed'
# Executable machine code must be unchanged; only fixed-width version literals
# and the .rsrc section are allowed to differ.
def sections(data):
 pe=struct.unpack_from('<I',data,0x3c)[0];number=struct.unpack_from('<H',data,pe+6)[0];optional=struct.unpack_from('<H',data,pe+20)[0];base=pe+24+optional
 result={}
 for n in range(number):
  off=base+n*40;name=data[off:off+8].rstrip(b'\0').decode();size,raw=struct.unpack_from('<II',data,off+16);result[name]=data[raw:raw+size]
 return result
oldsections=sections(image);newsections=sections(candidate.read_bytes())
for name,data in oldsections.items():
 if name!='.rsrc':assert newsections[name]==data.replace(old,new),f'Unexpected change to installer section {name}'
evidence={'template_sha256':sha(image),'template_version_literals_replaced':count,'installer_machine_sections_preserved':True,'payload_sha256':{names[i]:sha(data) for i,data in payload.items()},'installer_sha256':sha(candidate.read_bytes())}
(root/'evidence'/'package.json').write_text(json.dumps(evidence,indent=2))
print(json.dumps(evidence,indent=2))


