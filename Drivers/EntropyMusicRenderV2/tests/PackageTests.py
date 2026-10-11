"""Validate pinned references and non-deploying build scripts before packaging."""
import hashlib
import json
import pathlib
import re
import sys
import unittest

ROOT = pathlib.Path(__file__).absolute().parents[1]
PIN = '2dc3fd3a0cc84a2933f2194e7ec0871584979071'

class ReferenceTests(unittest.TestCase):
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
