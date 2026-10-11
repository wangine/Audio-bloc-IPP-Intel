"""Kernel/runner source integration gates complement executable model tests.

--revision checks the review baseline read-only; it must fail before the fixes.
These checks do not certify a loaded PortCls driver.
"""
import pathlib, subprocess, sys, unittest
ROOT=pathlib.Path(__file__).absolute().parents[1]
REV=sys.argv[sys.argv.index('--revision')+1] if '--revision' in sys.argv else None
def source(name):
    if REV:
        return subprocess.check_output(['git','show',REV+':Drivers/EntropyMusicRenderV2/'+name],cwd=ROOT.parents[1]).decode('utf-8-sig')
    return (ROOT/name).read_text(encoding='utf-8-sig')
class ReviewContracts(unittest.TestCase):
    def test_CommitDoesNotTakePublicationGate(self):
        text=source('kernel/shell/minwavertstream.cpp').split('CMiniportWaveRTStream::SetWritePacket',1)[1].split('CMiniportWaveRTStream::GetPosition',1)[0]
        self.assertNotIn('TryEnter',text)
        self.assertIn('m_Pcm->Submit',text)
        submit=source('kernel/PcmCommit.cpp').split('PcmCommit::Submit',1)[1].split('PcmCommit::Flush',1)[0]
        self.assertNotIn('operation_',submit)
        self.assertNotIn('EnterPassive',submit)
    def test_ProductionRefusesUnprovenPositionDma(self):
        text=source('kernel/shell/minwavertstream.cpp').split('CMiniportWaveRTStream::Allocate(',1)[1].split('AllocatePagesForMdl',1)[0]
        self.assertIn('if(!rawAllocationSupported(notifications))return STATUS_NOT_SUPPORTED',text)
    def test_LossEvidenceSurvivesFree(self):
        text=source('kernel/PcmCommit.cpp')
        self.assertIn('DriverLedger evidence',text)
        self.assertIn('evidence.finish',text)
        info=source('kernel/BridgeDevice.cpp')
        self.assertIn('GetPcmEvidence(0',info)
        self.assertNotIn('t.lifetimeTotals()',info)
        teardown=text.split('void UnregisterPcmOwner',1)[1].split('PcmCommit* AcquirePcmOwner',1)[0]
        self.assertLess(teardown.index('candidate->Stop()'),teardown.index('owner=nullptr'))
    def test_VerifierBindsRequestedMetadata(self):
        text=source('audit/main.cpp')
        self.assertIn('PcmVerifier verifier(format,expected,generation)',text)
        self.assertNotIn('if(!verifier)verifier=std::make_unique',text)
        self.assertIn('--generation',text)
        self.assertIn('p.generation!=generation_',source('audit/PcmVerifier.cpp'))
    def test_QualificationUsesPackageBinaries(self):
        text=source('tools/Run-Live-Acceptance.ps1')
        self.assertNotIn('build/model/EntropyMusicPcmAudit.exe',text)
        self.assertIn('Assert-MusicInstalledDriver',text)
        self.assertIn('Assert-MusicPackage',text)
        self.assertIn('frozenCoreSha256',text)
    def test_CloseReleasesReaderBeforeIdle(self):
        text=source('provider/BridgeClient.cpp')
        lease=text.split('struct ReadLease',1)[1].split('if(cancelled_)',1)[0]
        self.assertIn('reader.unlock()',lease)
        self.assertLess(lease.index('reader.unlock()'),lease.index('--client.active_'))
if __name__=='__main__': unittest.main(argv=[sys.argv[0]],verbosity=2)
