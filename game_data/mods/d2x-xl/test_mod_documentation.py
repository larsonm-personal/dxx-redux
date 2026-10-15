"""Release documentation integration checks using small real ZIP fixtures."""

import hashlib
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import zipfile

import mod_documentation as docs
import prepare_texture_release as release


def sha(data):
    return hashlib.sha256(data).hexdigest()


class DocumentationReleaseTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / "sample.dxa"
        self.notes = self.root / "notes"
        self.notes.mkdir()
        self.note = b"# Redux adaptation notes\n\nConverted textures; original credits are unchanged\n"
        (self.notes / "sample.dxa.md").write_bytes(self.note)
        self.original = b"Original credits\r\n\xff\r\n"
        self.entries = {
            "docs/README.TXT": self.original,
            "README.md": b"old generated note",
            "textures/image.bin": bytes(range(256)) * 32,
        }
        self.policy = {
            "original_documents": {"docs/README.TXT": sha(self.original)},
            "generated_documents": {"README.md": [sha(self.entries["README.md"])]},
        }
        self.write_source()

    def write_source(self):
        with zipfile.ZipFile(self.source, "w", compression=zipfile.ZIP_DEFLATED) as archive:
            for name, data in self.entries.items():
                archive.writestr(name, data)

    def prepare(self):
        return docs.prepare_pack(self.source, self.root / "out", self.policy, self.notes)

    def test_originals_payloads_companions_and_repeat_generation(self):
        original_archive = self.source.read_bytes()
        pack, evidence = self.prepare()
        self.assertEqual(evidence["preserved_entries"], 2)
        with zipfile.ZipFile(pack) as archive:
            self.assertEqual(set(archive.namelist()), {"docs/README.TXT", "textures/image.bin", docs.REDUX_README})
            self.assertEqual(archive.read("docs/README.TXT"), self.original)
            self.assertEqual(archive.read("textures/image.bin"), self.entries["textures/image.bin"])
            self.assertEqual(archive.read(docs.REDUX_README), self.note)
        bundle = docs.prepare_bundle(pack, self.root / "out", self.policy)
        bundle_path = self.root / "out" / bundle["filename"]
        with zipfile.ZipFile(bundle_path) as archive:
            self.assertEqual(set(archive.namelist()), {"docs/README.TXT", docs.REDUX_README})
            self.assertEqual(archive.read("docs/README.TXT"), self.original)
        before = [(p.read_bytes(), p.stat().st_mtime_ns) for p in (pack, bundle_path)]
        self.prepare()
        docs.prepare_bundle(pack, self.root / "out", self.policy)
        self.assertEqual(before, [(p.read_bytes(), p.stat().st_mtime_ns) for p in (pack, bundle_path)])
        self.assertEqual(self.source.read_bytes(), original_archive)
        self.assertEqual(bundle["source_pack_sha256"], sha(pack.read_bytes()))
        # Notes may evolve without touching the conversion input or original documents
        (self.notes / "sample.dxa.md").write_bytes(self.note + b"More detail\n")
        self.prepare()
        with zipfile.ZipFile(pack) as archive:
            self.assertEqual(archive.read("docs/README.TXT"), self.original)
            self.assertTrue(archive.read(docs.REDUX_README).endswith(b"More detail\n"))

    def test_original_changes_and_unknown_readmes_fail_without_overwriting_output(self):
        pack, _ = self.prepare()
        before = pack.read_bytes()
        for name in ("docs/README.TXT", "README.md"):
            with self.subTest(name=name):
                old = self.entries[name]
                self.entries[name] = b"unexpected author document"
                self.write_source()
                with self.assertRaises(ValueError):
                    self.prepare()
                self.assertEqual(pack.read_bytes(), before)
                self.entries[name] = old

    def test_untouched_archive_is_not_rewritten(self):
        self.policy["upstream_sha256"] = sha(self.source.read_bytes())
        before = (self.source.read_bytes(), self.source.stat().st_mtime_ns)
        pack, evidence = self.prepare()
        self.assertEqual(pack, self.source)
        self.assertTrue(evidence["unchanged_upstream"])
        self.assertEqual(before, (pack.read_bytes(), pack.stat().st_mtime_ns))
        self.entries["textures/image.bin"] = b"changed"
        self.write_source()
        with self.assertRaisesRegex(ValueError, "Untouched upstream"):
            self.prepare()

    def test_unsafe_and_ambiguous_paths_are_rejected(self):
        for name in ("../README.txt", "docs/../README.txt", "docs\\README.txt", "DOCS/readme.txt"):
            with self.subTest(name=name):
                self.entries[name] = b"bad"
                self.write_source()
                with self.assertRaises(ValueError):
                    self.prepare()
                del self.entries[name]

    def test_bundle_limits_and_binary_documents(self):
        self.entries["docs/manual.PDF"] = b"%PDF-1.0 binary\x00"
        self.entries["README"] = b"extensionless readme"
        self.write_source()
        pack, _ = self.prepare()
        bundle = docs.prepare_bundle(pack, self.root / "out", self.policy)
        self.assertEqual(len(bundle["documents"]), 4)
        self.entries.update({f"docs/{i}.txt": b"x" for i in range(docs.MAX_ENTRIES)})
        self.write_source()
        with self.assertRaisesRegex(ValueError, "inventory"):
            docs.prepare_bundle(self.source, self.root / "out", self.policy)

    def test_publication_rejects_changed_companion_before_github(self):
        pack, _ = self.prepare()
        bundle = docs.prepare_bundle(pack, self.root / "out", self.policy)
        bundle_path = self.root / "out" / bundle["filename"]
        validated = {**docs.identity(pack), "documentation": bundle}
        bundle_path.write_bytes(b"changed after validation")
        with patch.object(release, "gh") as github:
            with self.assertRaisesRegex(RuntimeError, "Documentation changed"):
                release.publish([pack, bundle_path], self.root / "notes.md", [validated])
            github.assert_not_called()


if __name__ == "__main__":
    unittest.main()
