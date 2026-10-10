"""Actual MSVC Core / Windows named-pipe integration; no hardware."""
import ctypes as C, json, os, pathlib, struct, subprocess, time, unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
K=C.WinDLL('kernel32',use_last_error=True)
K.CreateFileW.argtypes=[C.c_wchar_p,C.c_uint32,C.c_uint32,C.c_void_p,C.c_uint32,C.c_uint32,C.c_void_p];K.CreateFileW.restype=C.c_void_p
K.ReadFile.argtypes=[C.c_void_p,C.c_void_p,C.c_uint32,C.POINTER(C.c_uint32),C.c_void_p]
K.WriteFile.argtypes=K.ReadFile.argtypes;K.CloseHandle.argtypes=[C.c_void_p]
class Client:
 def __init__(self,endpoint):
  self.handle=None;self.id=0;self.session=None
  for _ in range(200):
   h=K.CreateFileW('\\\\.\\pipe\\'+endpoint,0xc0000000,0,None,3,0,None)
   if h != C.c_void_p(-1).value:self.handle=h;break
   time.sleep(.02)
  if not self.handle:raise RuntimeError('pipe unavailable')
 def close(self):
  if self.handle:K.CloseHandle(self.handle);self.handle=None
 def exact(self,n):
  out=b''
  while len(out)<n:
   b=C.create_string_buffer(n-len(out));read=C.c_uint32()
   if not K.ReadFile(self.handle,b,len(b),C.byref(read),None) or not read.value:raise OSError(C.get_last_error())
   out+=b.raw[:read.value]
  return out
 def call(self,action,_admission_only=False,**fields):
  self.id+=1;
  if self.session:fields.setdefault("session",self.session)
  p=json.dumps(dict(action=action,**fields),allow_nan=False).encode();b=struct.pack('<4I',1,16+len(p),2,self.id)+p;n=C.c_uint32()
  if not K.WriteFile(self.handle,b,len(b),C.byref(n),None):raise OSError(C.get_last_error())
  v,size,cmd,rid=struct.unpack('<4I',self.exact(16));assert v==1 and rid==self.id
  reply=json.loads(self.exact(size-16));self.session=reply.get("session")
  if reply.get('ok') and reply.get('data',{}).get('status')=='ACCEPTED' and not _admission_only:
   identity=reply['data']['commandId'];until=time.monotonic()+10
   while True:
    progress=self.call('command-results',ids=[identity])['data']['results'][0]
    if progress['complete']:
     reply=progress['reply'];reply['session']=self.session;break
    if time.monotonic()>until:raise RuntimeError('accepted command completion timeout')
    time.sleep(.01)
  return reply
class CoreProcessContract(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.endpoint='EntropyAudioCore.Stage05.Integration.'+str(os.getpid())
  exe=os.environ.get('STAGE05_CORE',str(ROOT/'build/msvc-stage05/EntropyAudioCore.exe'))
  cls.process=subprocess.Popen([exe,'--control-server','--endpoint',cls.endpoint,'--offline-test-signal','--offline-test-sinks','--run-seconds','90'],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
  cls.client=Client(cls.endpoint)
 @classmethod
 def tearDownClass(cls):
  cls.client.close();cls.process.terminate();cls.process.communicate(timeout=10)
 def test_01_metadata_and_parameter_validation(self):
  meta=self.client.call('describe');self.assertTrue(meta['ok']);self.assertGreater(len(meta['data']['parameters']),400)
  self.assertFalse(self.client.call('parameter',id='cinemaSource',value=2)['ok'])
  self.assertTrue(self.client.call('parameter',id='cinemaSource',value=1)['ok'])
  for _ in range(200):
   s=self.client.call('telemetry')['data']
   if int(s['crossoverApplied']):break
   time.sleep(.01)
  self.assertEqual(s['program'],1);self.assertGreater(int(s['crossoverApplied']),0)
 def test_02_matrix_headroom_boundaries(self):
  self.assertTrue(self.client.call('matrix-route',source=0,destination=1,enabled=True)['ok'])
  self.assertTrue(self.client.call('headroom-policy',mode=2,manualGainDb=-12)['ok'])
  time.sleep(.05);s=self.client.call('telemetry')['data']
  self.assertEqual(s['matrix']['routes'][0],3);self.assertEqual(s['headroom']['manualGainDb'],-12)
 def test_03_hardware_remains_blocked(self):
  r=self.client.call('arm');self.assertFalse(r['ok']);self.assertIn('Stage04',r['error'])
  self.assertTrue(self.client.call('disarm')['ok'])
  self.assertFalse(self.client.call('telemetry')['data']['armed'])
 def test_04_profiles_atomic_and_corruption(self):
  path=ROOT/'local/stage05/test-profile.entropy';bad=ROOT/'local/stage05/test-profile-corrupt.entropy'
  self.assertTrue(self.client.call('save-profile',path=str(path))['ok'])
  self.assertTrue(self.client.call('parameter',id='cinemaSource',value=0)['ok'])
  self.assertTrue(self.client.call('load-profile',path=str(path))['ok'])
  s=self.client.call('telemetry')['data'];self.assertEqual(s['program'],1)
  p=json.loads(path.read_text());p['modules'][0]['state_checksum_fnv1a64']='corrupt';bad.write_text(json.dumps(p))
  self.assertFalse(self.client.call('load-profile',path=str(bad))['ok'])
  self.assertEqual(self.client.call('telemetry')['data']['program'],1)
  p=json.loads(path.read_text());p['stage05_devices']={'bindings':[{'logicalId':'INVALID'},None],'configuration':[]};bad.write_text(json.dumps(p))
  self.assertFalse(self.client.call('load-profile',path=str(bad))['ok'], 'invalid device binding was ignored')
  self.assertEqual(self.client.call('telemetry')['data']['program'],1)
 def test_05_devices_and_disconnect(self):
  r=self.client.call('devices');self.assertTrue(r['ok']);self.assertIsInstance(r['data']['devices'],list)
  self.client.close();time.sleep(.05);type(self).client=Client(self.endpoint)
  self.assertFalse(self.client.call('telemetry')['data']['armed'])
 def test_06_independent_offline_sink_workers(self):
  self.assertTrue(self.client.call('headroom-policy',mode=2,manualGainDb=-20)['ok']);time.sleep(.05)
  self.assertTrue(self.client.call('offline-arm')['ok']);time.sleep(.3)
  s=self.client.call('telemetry')['data'];self.assertFalse(s['armed'])
  for d in s['domains']:
   self.assertTrue(d['offlineFixture']);self.assertFalse(d['physicalSinkOpen']);self.assertGreater(int(d['callbacks']),40);self.assertEqual(d['faults'],0);self.assertEqual(int(d['xruns']),0)
  self.assertTrue(self.client.call('disarm')['ok'])
 def test_07_stale_sessions_rejected(self):
  self.assertFalse(self.client.call('parameter',id='cinemaSource',value=0,session='1')['ok'])
if __name__=='__main__':unittest.main()
