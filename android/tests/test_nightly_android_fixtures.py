import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]


def module(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / 'android/helpers' / (name + '.py'))
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


corpus = module('setup_demo_import_corpus')
audio = module('generate_preview_test_audio')


class KvmWorkflowContractTests(unittest.TestCase):
    def test_grant_is_only_existing_runner_user_on_real_kvm(self):
        source = (ROOT / '.github/workflows/android-nightly-tooling.yml').read_text()
        grant = source.split('- name: Require accelerated emulator access', 1)[1].split('- name: Restore pinned', 1)[0]
        self.assertIn('[[ ! -c /dev/kvm ]]', grant)
        self.assertIn('[[ ! -r /dev/kvm || ! -w /dev/kvm ]]', grant)
        self.assertIn('sudo setfacl -m "u:$(id -u):rw" /dev/kvm', grant)
        self.assertLess(grant.index('getfacl -p -n /dev/kvm >'), grant.index('sudo setfacl -m'))
        self.assertIn("grep -Eq '^(mask::|user:[^:]+:|group:[^:]+:)'", grant)
        self.assertLess(grant.index('grep -Eq'), grant.index('touch'))
        self.assertLess(grant.index('touch "$RUNNER_TEMP/dxx-kvm-acl-changed"'), grant.index('sudo setfacl -m'))
        for forbidden in ('chmod', '0666', 'usermod', 'useradd', 'groupadd'):
            self.assertNotIn(forbidden, grant)

    def test_cleanup_is_unconditional_and_restores_prior_acl(self):
        source = (ROOT / '.github/workflows/android-nightly-tooling.yml').read_text()
        cleanup = source.split('- name: Restore original KVM access', 1)[1].split('- name: Record lane lifecycle', 1)[0]
        self.assertIn("if: always() && inputs.lane != 'jvm'", cleanup)
        self.assertIn('timeout-minutes: 1', cleanup)
        self.assertIn('[[ -f "$RUNNER_TEMP/dxx-kvm-acl-changed" ]]', cleanup)
        self.assertIn('sudo setfacl --restore="$RUNNER_TEMP/dxx-kvm-before.acl"', cleanup)
        self.assertIn('cmp "$RUNNER_TEMP/dxx-kvm-before.acl" results/setup/kvm-restored.acl', cleanup)
        self.assertLess(source.index('- name: Collect emulator diagnostics'), source.index('- name: Restore original KVM access'))
        self.assertLess(source.index('- name: Restore original KVM access'), source.index('- name: Upload execution evidence'))


class FixtureTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.data = b'public demo fixture witness'
        self.entry = {'archive': 'public demo.zip', 'sha256': hashlib.sha256(self.data).hexdigest()}
        self.manifest = {'path': 'game_data/demo installers/public demo.zip',
                         'sha256': self.entry['sha256'], 'size': len(self.data)}
        mapping = patch.dict(corpus.PUBLIC_ASSETS, {'public demo.zip': 'public demo.zip'})
        mapping.start()
        self.addCleanup(mapping.stop)
        self.write_manifests()
        self.output = self.root / 'corpus'

    def write_manifests(self):
        for path, value in [('android/tests/fixtures/demo_import_oracles.json', {'archives': [self.entry]}),
                            ('game_data/test_data_manifest.json', {'files': [self.manifest]})]:
            target = self.root / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(json.dumps(value))

    def test_identity_and_encoded_fixed_release(self):
        entry, = corpus.packages(self.root)
        self.assertEqual(corpus.RELEASE + 'public%20demo.zip', entry['url'])
        self.assertEqual(len(self.data), entry['bytes'])

    def test_unpublished_original_is_not_substituted(self):
        with patch.dict(corpus.PUBLIC_ASSETS, {}, clear=True):
            self.assertIsNone(corpus.packages(self.root)[0]['url'])
            with self.assertRaisesRegex(ValueError, 'no reviewed public URL'):
                corpus.stage(self.root, self.output, True)
        self.assertFalse((self.output / self.entry['archive']).exists())

    def test_oracle_manifest_disagreement_rejected(self):
        self.manifest['sha256'] = '0' * 64
        self.write_manifests()
        with self.assertRaises(ValueError): corpus.packages(self.root)

    def test_archive_traversal_rejected(self):
        self.entry['archive'] = '../retail.zip'
        self.write_manifests()
        with self.assertRaises(ValueError): corpus.packages(self.root)

    def test_package_budget_rejected(self):
        self.manifest['size'] = 33 * 1024 * 1024
        self.write_manifests()
        with self.assertRaises(ValueError): corpus.packages(self.root)

    def test_missing_requires_explicit_download(self):
        with self.assertRaises(ValueError): corpus.stage(self.root, self.output)

    def test_valid_cache_never_downloads(self):
        self.output.mkdir()
        (self.output / self.entry['archive']).write_bytes(self.data)
        with patch.object(corpus.subprocess, 'run', side_effect=AssertionError('network forbidden')):
            result = corpus.stage(self.root, self.output)
        self.assertEqual(1, result['archive_count'])
        self.assertTrue((self.output / 'nightly-demo-corpus-provenance.json').is_file())

    def test_invalid_existing_package_preserved(self):
        self.output.mkdir()
        target = self.output / self.entry['archive']
        target.write_bytes(b'existing user file')
        with self.assertRaises(ValueError): corpus.stage(self.root, self.output, True)
        self.assertEqual(b'existing user file', target.read_bytes())

    def test_bounded_download_verified_before_publish(self):
        def download(argv, **kwargs):
            self.assertEqual(130, kwargs['timeout'])
            self.assertEqual('120', argv[argv.index('--max-time') + 1])
            self.assertEqual(str(len(self.data)), argv[argv.index('--max-filesize') + 1])
            self.assertEqual('=https', argv[argv.index('--proto-redir') + 1])
            Path(argv[argv.index('--output') + 1]).write_bytes(self.data)
        with patch.object(corpus.subprocess, 'run', side_effect=download):
            corpus.stage(self.root, self.output, True)
        self.assertEqual(self.data, (self.output / self.entry['archive']).read_bytes())

    def test_bad_download_not_published(self):
        def download(argv, **kwargs):
            Path(argv[argv.index('--output') + 1]).write_bytes(b'corrupt')
        with patch.object(corpus.subprocess, 'run', side_effect=download), self.assertRaises(ValueError):
            corpus.stage(self.root, self.output, True)
        self.assertFalse((self.output / self.entry['archive']).exists())

    @unittest.skipUnless(shutil.which('ffmpeg') and shutil.which('ffprobe'), 'FFmpeg tools required')
    def test_real_generated_audio_repeats_and_refuses_overwrite(self):
        first = audio.generate(self.root / 'first')
        second = audio.generate(self.root / 'second')
        self.assertEqual(first['sha256'], second['sha256'])
        self.assertGreater(float(first['probe']['format']['duration']), 20)
        with self.assertRaises(ValueError): audio.generate(self.root / 'first')


if __name__ == '__main__':
    unittest.main()
