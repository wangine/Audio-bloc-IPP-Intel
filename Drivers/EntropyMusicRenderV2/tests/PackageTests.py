"""Validate pinned references and non-deploying build scripts before packaging."""
import hashlib
import json
import pathlib
import re
import sys
import unittest
import subprocess
import tempfile
import zipfile
import shutil

ROOT = pathlib.Path(__file__).absolute().parents[1]
PIN = '2dc3fd3a0cc84a2933f2194e7ec0871584979071'
PACKAGE = pathlib.Path(sys.argv[sys.argv.index('--package')+1]).absolute() if '--package' in sys.argv else None

def check_artifacts(folder, manifest):
    for artifact in manifest['artifacts']:
        relative = pathlib.PurePosixPath(artifact['path'])
        if relative.is_absolute() or '..' in relative.parts:
            return False
        path = folder.joinpath(*relative.parts)
        if not path.is_file() or path.stat().st_size != artifact['bytes'] or hashlib.sha256(path.read_bytes()).hexdigest().upper() != artifact['sha256']:
            return False
    return bool(manifest['artifacts'])

@unittest.skipIf(PACKAGE is None, 'package-only checks requested with --package')
class DeliveryTests(unittest.TestCase):
    def manifest(self):
        self.assertTrue((PACKAGE/'manifest.json').is_file(), 'reviewable package not implemented')
        return json.loads((PACKAGE/'manifest.json').read_text(encoding='utf-8-sig'))

    def test_PackageHasOneNewStereoIdentity(self):
        data=self.manifest()
        self.assertEqual(data['identity']['hardwareId'], r'ROOT\EntropyMusicRenderV2')
        self.assertEqual(data['identity']['channels'], 2)
        self.assertEqual(data['identity']['service'], 'EntropyMusicRenderV2')
        self.assertEqual(data['acceptance'], 'NOT_RUN')
        text=(PACKAGE/'driver/EntropyMusicRenderV2.inf').read_text(encoding='utf-8-sig')
        self.assertEqual(len(re.findall(r'(?im)^%DeviceName%=Music,ROOT\\EntropyMusicRenderV2\s*$',text)),1)
        self.assertRegex(text,r'(?im)^DriverVer\s*=\s*10/11/2026,0\.3\.0\.1\s*$')
        self.assertTrue((PACKAGE/'driver/EntropyMusicRenderV2.cat').stat().st_size>0)
        self.assertEqual(data['catalogGenerationExit'],0)
        self.assertEqual(data['signatures']['driver/EntropyMusicRenderV2.sys'],'NotSigned')
        self.assertEqual(data['signatures']['driver/EntropyMusicRenderV2.cat'],'NotSigned')

    def test_AllArtifactsMatchManifest(self):
        data=self.manifest();self.assertTrue(check_artifacts(PACKAGE,data))
        changed=json.loads(json.dumps(data));changed['artifacts'][0]['sha256']='0'*64
        self.assertFalse(check_artifacts(PACKAGE,changed),'altered artifact manifest must fail')
        changed['artifacts'][0]['path']='../foreign';self.assertFalse(check_artifacts(PACKAGE,changed))

    def test_ExistingDriverIdsAbsent(self):
        self.manifest()
        text=(PACKAGE/'driver/EntropyMusicRenderV2.inf').read_text(encoding='utf-8-sig')
        self.assertNotRegex(text,r'(?i)ROOT\\(?:EntropyAudioVirtual|EntropyAudioTransport|EntropyMusicTransport)\b')
        self.assertEqual(len(re.findall(r'(?im)^AddService=',text)),1)

    def test_NoInstallFromBuild(self):
        self.manifest()
        for script in ROOT.joinpath('tools').glob('*.ps1'):
            if script.name.startswith(('Build-','Package-','Prepare-','Import-')):
                self.assertNotRegex(script.read_text(encoding='utf-8-sig'),r'(?i)\b(bcdedit|pnputil|devcon|certutil|Import-Certificate|Import-PfxCertificate)\b')
        self.assertTrue((PACKAGE/'DEPLOYMENT.md').is_file())

    def test_SourceArchiveMatchesInventoryAndCheckout(self):
        self.manifest()
        items=json.loads((PACKAGE/'source-files.json').read_text(encoding='utf-8-sig'))
        repo=ROOT.parents[1]
        with zipfile.ZipFile(PACKAGE/'source.zip') as archive:
            self.assertEqual(set(archive.namelist()), {item['path'] for item in items})
            for item in items:
                body=archive.read(item['path'])
                self.assertEqual(len(body),item['bytes'])
                self.assertEqual(hashlib.sha256(body).hexdigest().upper(),item['sha256'])
                self.assertEqual(body,(repo/item['path']).read_bytes(),item['path'])
        for item in self.manifest()['artifacts']:
            if item['path'].startswith(('driver/','user/')) and not item['path'].endswith('.cat'):
                build=ROOT/'build'/('kernel' if item['path'].startswith('driver/') else 'model')/pathlib.PurePosixPath(item['path']).name
                self.assertEqual(build.read_bytes(),(PACKAGE/item['path']).read_bytes(),item['path'])

    def test_QualificationRejectsChangedArtifact(self):
        manifest=self.manifest()
        shell=shutil.which('pwsh')
        self.assertIsNotNone(shell,'PowerShell7 required for the packaged runner check')
        script=PACKAGE/'tools/Run-Live-Acceptance.ps1'
        command=[str(shell),'-NoProfile','-File',str(script),'-ValidatePackageOnly','-Package',str(PACKAGE)]
        self.assertEqual(subprocess.run(command,capture_output=True).returncode,0)
        # Real file alteration against an unchanged manifest, without changing
        # the delivery package, must fail before device discovery or rendering.
        with tempfile.TemporaryDirectory() as temp:
            folder=pathlib.Path(temp)
            shutil.copytree(PACKAGE,folder,dirs_exist_ok=True)
            changed=folder/'user/EntropyMusicRawProviderV2Audit.dll'
            body=bytearray(changed.read_bytes());body[0]^=1;changed.write_bytes(body)
            result=subprocess.run([str(shell),'-NoProfile','-File',str(script),'-ValidatePackageOnly','-Package',str(folder)],capture_output=True)
            self.assertNotEqual(result.returncode,0)
            self.assertIn(b'Package artifact differs',result.stderr)
        text=script.read_text(encoding='utf-8-sig')
        self.assertNotIn('build/model/EntropyMusicPcmAudit.exe',text)
        self.assertIn('Assert-MusicInstalledDriver',text)
        self.assertIn('--generation $rendered.generation',text)

class ReferenceTests(unittest.TestCase):
    def test_frozen_provider_abi(self):
        abi = ROOT.parents[1] / 'Integration/Music/Stage1/ABI/endpoint_provider_v2.h'
        self.assertEqual(hashlib.sha256(abi.read_bytes()).hexdigest().upper(), 'A4F848B9A4E653FD58ED937E4488D1DB81D5BAA1222CDEFB93813D07B40F60CD')

    def test_upstream_manifest_exact_pin(self):
        manifest = ROOT / 'upstream-manifest.json'
        self.assertTrue(manifest.is_file(), 'pinned import manifest not implemented')
        data = json.loads(manifest.read_text(encoding='utf-8-sig'))
        self.assertEqual(data['commit'], PIN)
        self.assertEqual(data['license'], 'MS-PL')
        self.assertTrue(data['files'])
        self.assertTrue((ROOT / 'upstream/LICENSE').is_file())
        for entry in data['files']:
            body = (ROOT / 'upstream' / entry['path']).read_bytes()
            git_blob = b'blob ' + str(len(body)).encode() + b'\0' + body
            self.assertEqual(hashlib.sha1(git_blob).hexdigest(), entry['gitBlobSha'], entry['path'])

    def test_build_has_no_deployment_side_effects(self):
        script = ROOT / 'tools/Build-Kernel.ps1'
        self.assertTrue(script.is_file(), 'non-deploying kernel build not implemented')
        text = script.read_text(encoding='utf-8-sig')
        self.assertIn('MSBuild', text)
        self.assertNotRegex(text, r'(?i)\b(bcdedit|pnputil|devcon|certutil|Import-Certificate|Import-PfxCertificate)\b')
        self.assertNotRegex(text, r'(?i)Start-Service|New-Service|Set-Service|sc\.exe\s+(create|start)')
        self.assertIn('SignMode=Off', text)

if __name__ == '__main__':
    unittest.main(argv=[sys.argv[0]], verbosity=2)
