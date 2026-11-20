import unittest
from pathlib import Path

from xl_reference_campaign import classify


class ReferenceCampaignTests(unittest.TestCase):
    def setUp(self):
        self.descriptor = Path("danwentz/dffusion.mn2").resolve()
        self.events = [["initialized"], ["listed", "1"], ["descriptor", str(self.descriptor)]]

    def result(self, status, events):
        return classify(status, events, self.descriptor, "secret.rl2", -1)

    def test_mission_lookup_preserves_directory_and_negative_result(self):
        rejected = self.events + [["mission", "0"]]
        self.assertEqual(self.result(21, rejected), "mission_rejected")
        rejected[2] = ["descriptor", str(Path("darkhorse/dffusion.mn2").resolve())]
        self.assertEqual(self.result(21, rejected), "descriptor_identity_unverified")

    def test_source_lookup_and_simulation_are_independent(self):
        opened = self.events + [["mission", "1"], ["member", "-1", "secret.rl2", "0"], ["starting", "-1", "secret.rl2"]]
        self.assertEqual(self.result(None, opened), "advertised_member_lookup_empty")
        self.assertEqual(self.result(None, []), "reference_startup_unverified")
        ran = opened + [["started", "1", "-1"], ["frames", "120", "-1", "0"]]
        self.assertEqual(self.result(0, ran), "started_and_ran")
        ran[-1] = ["frames", "120", "1", "0"]
        self.assertNotEqual(self.result(0, ran), "started_and_ran")
