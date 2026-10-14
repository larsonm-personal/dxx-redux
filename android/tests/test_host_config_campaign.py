"""Offline controls for the semantic desktop configuration runner."""

import importlib.util
import itertools
from pathlib import Path
import tempfile
import json
import sys
import time
import unittest
from unittest.mock import Mock, patch

spec = importlib.util.spec_from_file_location(
    "campaign", Path(__file__).resolve().parents[1] / "helpers/host_config_campaign.py"
)
campaign = importlib.util.module_from_spec(spec)
spec.loader.exec_module(campaign)


class HostConfigControls(unittest.TestCase):
    def test_pairwise_and_risky_triples_are_complete_and_deterministic(self):
        rows = campaign.covering_cases(42)
        self.assertEqual(rows, campaign.covering_cases(42))
        self.assertLess(len(rows), 100)
        for names in itertools.combinations(campaign.FACTORS, 2):
            observed = {tuple(row[k] for k in names) for row in rows}
            expected = set(itertools.product(*(campaign.FACTORS[k] for k in names)))
            self.assertEqual(observed, expected)
        names = ("MusicType", "TexFilt", "ReverseStereo")
        self.assertEqual(
            {tuple(row[k] for k in names) for row in rows},
            set(itertools.product(*(campaign.FACTORS[k] for k in names))),
        )

    def test_ambiguous_items_are_rejected(self):
        session = Mock()
        session.wait.return_value = {"items": [{"text": "music volume"}, {"text": "music type"}], "selected": 0}
        with self.assertRaisesRegex(campaign.ProbeFailure, "matched"):
            campaign.HostSession.select(session, "music")

    def test_transient_unknown_snapshot_cannot_skip_target_item(self):
        session = campaign.HostSession.__new__(campaign.HostSession)
        items = [{"text": "first", "type": 0}, {"text": "target", "type": 0}]
        initial = {"kind": "menu", "title": "", "subtitle": "Menu", "selected": 0, "items": items}
        selected = dict(initial, selected=1)
        session.wait = Mock(side_effect=[initial, selected])
        session.key = Mock(return_value={"kind": "other", "selected": -1, "items": []})
        self.assertEqual(session.select("target"), selected)
        session.key.assert_called_once_with("down")

    def test_listbox_selection_does_not_assume_wrapping(self):
        session = campaign.HostSession.__new__(campaign.HostSession)
        initial = {
            "kind": "listbox",
            "title": "Mission",
            "subtitle": "",
            "selected": 1,
            "items": [{"text": "target"}, {"text": "current"}],
        }
        selected = dict(initial, selected=0)
        session.wait = Mock(side_effect=[initial, selected])
        session.key = Mock()
        self.assertEqual(session.select("target"), selected)
        session.key.assert_called_once_with("up")

    def test_false_assertion_and_stalled_snapshot_time_out(self):
        with tempfile.TemporaryDirectory() as directory:
            session = campaign.HostSession("unused", directory, directory)
            with patch.object(session, "snapshot", return_value={"frame": 1}):
                with self.assertRaisesRegex(campaign.ProbeFailure, "timeout"):
                    session.wait(lambda state: state["frame"] > 1, "deliberate stalled frame", timeout=0.02)
                with self.assertRaisesRegex(campaign.ProbeFailure, "timeout"):
                    session.wait(lambda state: False, "deliberate wrong expectation", timeout=0.02)

    def test_process_exit_is_never_success(self):
        with tempfile.TemporaryDirectory() as directory:
            session = campaign.HostSession("unused", directory, directory)
            session.process = Mock()
            session.process.poll.return_value = -11
            session.process.returncode = -11
            session.log_path = Path(directory) / "engine.log"
            with self.assertRaisesRegex(campaign.ProbeFailure, "unexpected engine exit"):
                session.health()

    def test_sanitizer_and_deadline_are_failures(self):
        with tempfile.TemporaryDirectory() as directory:
            session = campaign.HostSession("unused", directory, directory)
            session.process = Mock()
            session.process.poll.return_value = None
            session.log_path = Path(directory) / "engine.log"
            session.log_path.write_text("AddressSanitizer: sample control")
            with self.assertRaisesRegex(campaign.ProbeFailure, "fatal engine diagnostic"):
                session.health()
            session.deadline = time.monotonic() - 1
            with self.assertRaisesRegex(campaign.ProbeFailure, "deadline"):
                session.health()

    def test_already_dead_child_is_not_a_clean_quit(self):
        with tempfile.TemporaryDirectory() as directory:
            session = campaign.HostSession("unused", directory, directory)
            session.process = Mock()
            session.process.poll.return_value = -11
            session.process.returncode = -11
            with self.assertRaisesRegex(campaign.ProbeFailure, "unexpected engine exit before quit"):
                session.close()

    def test_teardown_sanitizer_even_with_zero_exit_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            session = campaign.HostSession("unused", directory, directory)
            session.process = Mock()
            session.process.poll.side_effect = [None, 0]
            session.process.returncode = 0
            session.wait = Mock()
            session.select = Mock()
            session.log_path = Path(directory) / "engine.log"
            session.log_path.write_text("runtime error: deliberately injected final-log control")
            with self.assertRaisesRegex(campaign.ProbeFailure, "fatal diagnostic during quit"):
                session.close()

    def test_expired_campaign_reports_incomplete_instead_of_success(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            executable = root / "unused-engine"
            executable.write_text("unused offline executable fixture")
            output = root / "output"
            argv = [
                "campaign",
                "--executable",
                str(executable),
                "--data",
                directory,
                "--game",
                "d1",
                "--output",
                str(output),
                "--hours",
                "0",
                "--limit",
                "1",
            ]
            with (
                patch.object(sys, "argv", argv),
                patch.object(campaign, "freeze_binary", return_value=executable),
                patch.object(campaign, "run_case") as run,
            ):
                self.assertEqual(campaign.main(), 2)
                run.assert_not_called()
            summary = json.loads((output / "summary.json").read_text())
            self.assertEqual((summary["status"], summary["executed"], summary["planned"]), ("incomplete", 0, 1))

    def test_profiles_are_isolated_and_config_written_only_there(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            a = campaign.HostSession("unused", root, root / "a", config={"MusicType": 0})
            b = campaign.HostSession("unused", root, root / "b", config={"MusicType": 1})
            self.assertNotEqual(a.home, b.home)
            self.assertEqual((a.profile / "descent.cfg").read_text(), "MusicType=0\n")
            self.assertEqual((b.profile / "descent.cfg").read_text(), "MusicType=1\n")


if __name__ == "__main__":
    unittest.main()
