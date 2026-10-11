"""Full collector/verifier regression with the candidate publisher; no audio."""
import ctypes, pathlib, subprocess, sys, tempfile, time, uuid, unittest, struct
AUDIT, PUBLISHER = sys.argv[1:3]
k=ctypes.WinDLL('kernel32',use_last_error=True)
k.CreateFileMappingW.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_uint32,ctypes.c_uint32,ctypes.c_uint32,ctypes.c_wchar_p]
k.CreateFileMappingW.restype=ctypes.c_void_p
k.MapViewOfFile.argtypes=[ctypes.c_void_p,ctypes.c_uint32,ctypes.c_uint32,ctypes.c_uint32,ctypes.c_size_t]
k.MapViewOfFile.restype=ctypes.c_void_p
k.UnmapViewOfFile.argtypes=[ctypes.c_void_p]
k.CloseHandle.argtypes=[ctypes.c_void_p]
class ThreadEntry(ctypes.Structure):
    _fields_=[('size',ctypes.c_uint32),('usage',ctypes.c_uint32),('thread',ctypes.c_uint32),('pid',ctypes.c_uint32),('base',ctypes.c_int32),('delta',ctypes.c_int32),('flags',ctypes.c_uint32)]
k.CreateToolhelp32Snapshot.argtypes=[ctypes.c_uint32,ctypes.c_uint32];k.CreateToolhelp32Snapshot.restype=ctypes.c_void_p
k.Thread32First.argtypes=[ctypes.c_void_p,ctypes.POINTER(ThreadEntry)];k.Thread32Next.argtypes=k.Thread32First.argtypes
k.OpenThread.argtypes=[ctypes.c_uint32,ctypes.c_int,ctypes.c_uint32];k.OpenThread.restype=ctypes.c_void_p
k.SuspendThread.argtypes=[ctypes.c_void_p];k.SuspendThread.restype=ctypes.c_uint32
k.ResumeThread.argtypes=[ctypes.c_void_p];k.ResumeThread.restype=ctypes.c_uint32
def pause_own_collector(pid):
    handles=[];snapshot=k.CreateToolhelp32Snapshot(4,0)
    try:
        entry=ThreadEntry();entry.size=ctypes.sizeof(entry);found=k.Thread32First(snapshot,ctypes.byref(entry))
        while found:
            if entry.pid==pid:
                handle=k.OpenThread(2,0,entry.thread)
                if not handle or k.SuspendThread(handle)==0xffffffff:
                    if handle:k.CloseHandle(handle)
                    raise RuntimeError('Cannot suspend own isolated collector thread')
                handles.append(handle)
            found=k.Thread32Next(snapshot,ctypes.byref(entry))
        if not handles:raise RuntimeError('Own collector threads absent')
        return handles
    except:
        resume_own_collector(handles);raise
    finally:k.CloseHandle(snapshot)
def resume_own_collector(handles):
    for handle in handles:k.ResumeThread(handle);k.CloseHandle(handle)
    handles.clear()
class OutputBoundary(unittest.TestCase):
    def exercise(self,kind):
        token=uuid.uuid4().hex
        with tempfile.TemporaryDirectory(prefix='entropy-output-boundary-') as folder:
            root=pathlib.Path(folder);ready=root/'ready';capture=root/'capture.bin';stop=root/'abort'
            mapping=k.CreateFileMappingW(ctypes.c_void_p(-1),None,4,0,32,'Local\\EntropyMusicAudit-'+token)
            self.assertTrue(mapping)
            view=k.MapViewOfFile(mapping,0xf001f,0,0,32);self.assertTrue(view)
            ctypes.memset(view,0,32);publisher=collector=None;suspended=[]
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
                if kind=='terminal-queued':suspended=pause_own_collector(collector.pid)
                if kind=='abort':stop.write_text('cancel')
                publisher.wait(timeout=5)
                if kind=='terminal-queued':
                    self.assertEqual(ctypes.c_int64.from_address(view+16).value,1024,'collector consumed final queued blocks before terminal publication')
                    resume_own_collector(suspended)
                stdout,stderr=collector.communicate(timeout=5)
                self.assertEqual(collector.returncode,0 if kind in ('clean','extra','clean-retire','terminal-queued') else 2,stderr.decode())
                verified=subprocess.run([AUDIT,'--action','verify-output','--capture',str(capture),'--frames','3072' if kind=='terminal-queued' else '1024','--bits','16','--generation','7','--core-pid',str(publisher.pid)],capture_output=True,text=True,timeout=5)
                self.assertEqual(verified.returncode,0 if kind in ('clean','clean-retire','terminal-queued') else 2,verified.stdout+verified.stderr)
                data=capture.read_bytes();cursor=0;blocks=0
                while cursor<len(data):
                    magic,version,record,size,errors,reserved=struct.unpack_from('<IIIIQQ',data,cursor)
                    self.assertEqual(magic,0x464d5245);self.assertEqual(version,1)
                    blocks+=record==2;cursor+=32+size
                self.assertEqual(blocks,3 if kind=='terminal-queued' else 2 if kind=='extra' else 1)
            finally:
                resume_own_collector(suspended)
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
    def test_durable_normal_stop_and_final_drain(self):self.exercise('clean-retire')
    def test_terminal_with_two_unread_final_blocks(self):self.exercise('terminal-queued')
    def test_abort_at_final_queue_drain(self):self.exercise('abortfinal')
if __name__=='__main__':unittest.main(argv=[sys.argv[0]],verbosity=2)
