"""Full collector/verifier regression with a real frozen publisher; no audio."""
import ctypes, pathlib, subprocess, sys, tempfile, time, uuid, unittest, struct
AUDIT, PUBLISHER = sys.argv[1:3]
k=ctypes.WinDLL('kernel32',use_last_error=True)
k.CreateFileMappingW.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_uint32,ctypes.c_uint32,ctypes.c_uint32,ctypes.c_wchar_p]
k.CreateFileMappingW.restype=ctypes.c_void_p
k.MapViewOfFile.argtypes=[ctypes.c_void_p,ctypes.c_uint32,ctypes.c_uint32,ctypes.c_uint32,ctypes.c_size_t]
k.MapViewOfFile.restype=ctypes.c_void_p
k.UnmapViewOfFile.argtypes=[ctypes.c_void_p]
k.CloseHandle.argtypes=[ctypes.c_void_p]
class OutputBoundary(unittest.TestCase):
    def exercise(self,kind):
        token=uuid.uuid4().hex
        with tempfile.TemporaryDirectory(prefix='entropy-output-boundary-') as folder:
            root=pathlib.Path(folder);ready=root/'ready';capture=root/'capture.bin';stop=root/'abort'
            mapping=k.CreateFileMappingW(ctypes.c_void_p(-1),None,4,0,32,'Local\\EntropyMusicAudit-'+token)
            self.assertTrue(mapping)
            view=k.MapViewOfFile(mapping,0xf001f,0,0,32);self.assertTrue(view)
            ctypes.memset(view,0,32);publisher=collector=None
            try:
                publisher=subprocess.Popen([PUBLISHER,token,kind,str(ready),str(stop)])
                deadline=time.monotonic()+5
                while not ready.exists() and time.monotonic()<deadline:time.sleep(.01)
                self.assertTrue(ready.exists())
                collector=subprocess.Popen([AUDIT,'--action','collect-output','--session',token,'--endpoint','\\\\.\\pipe\\Entropy.Stage1.Audit.'+token,'--core-pid',str(publisher.pid),'--capture',str(capture),'--stop-file',str(stop)],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
                deadline=time.monotonic()+5
                while ctypes.c_int64.from_address(view+16).value!=1024 and time.monotonic()<deadline and collector.poll() is None:time.sleep(.001)
                if collector.poll() is not None:
                    stdout,stderr=collector.communicate();self.fail('premature collector exit: '+stderr.decode()+stdout.decode())
                self.assertEqual(ctypes.c_int64.from_address(view+16).value,1024)
                self.assertIsNone(collector.poll(),'collector completed before producer completion')
                if kind=='abort':stop.write_text('cancel')
                publisher.wait(timeout=5);stdout,stderr=collector.communicate(timeout=5)
                self.assertEqual(collector.returncode,0 if kind in ('clean','extra') else 2,stderr.decode())
                verified=subprocess.run([AUDIT,'--action','verify-output','--capture',str(capture),'--frames','1024','--bits','16','--generation','7','--core-pid',str(publisher.pid)],capture_output=True,text=True,timeout=5)
                self.assertEqual(verified.returncode,0 if kind=='clean' else 2,verified.stdout+verified.stderr)
                data=capture.read_bytes();cursor=0;blocks=0
                while cursor<len(data):
                    magic,version,record,size,errors,reserved=struct.unpack_from('<IIIIQQ',data,cursor)
                    self.assertEqual(magic,0x464d5245);self.assertEqual(version,1)
                    blocks+=record==2;cursor+=32+size
                self.assertEqual(blocks,2 if kind=='extra' else 1)
            finally:
                for process in (collector,publisher):
                    if process and process.poll() is None:process.kill();process.wait()
                    if process:
                        for stream in (process.stdout,process.stderr):
                            if stream:stream.close()
                k.UnmapViewOfFile(view);k.CloseHandle(mapping)
    def test_clean_producer_exit(self):self.exercise('clean')
    def test_delayed_extra_after_expected_count(self):self.exercise('extra')
    def test_cancel_is_not_completion(self):self.exercise('abort')
    def test_abnormal_producer_exit(self):self.exercise('badexit')
    def test_retired_session_with_later_extra_output(self):self.exercise('retire')
    def test_error_state_erased_by_retirement(self):self.exercise('error-retire')
    def test_ambiguous_normal_stop_is_not_certification(self):self.exercise('clean-retire')
    def test_abort_at_final_queue_drain(self):self.exercise('abortfinal')
if __name__=='__main__':unittest.main(argv=[sys.argv[0]],verbosity=2)
