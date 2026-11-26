import unittest
from pathlib import Path

from xl_reference_campaign import classify, texture_quality_observation


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
        self.assertNotEqual(classify(0, ran, self.descriptor, "secret.rl2", -1, 900), "started_and_ran")
        extended = ran[:-1] + [["frames", "900", "-1", "0"]]
        self.assertEqual(classify(0, extended, self.descriptor, "secret.rl2", -1, 900), "started_and_ran")
        self.assertNotEqual(self.result(0, extended), "started_and_ran")
        ran[-1] = ["frames", "120", "1", "0"]
        self.assertNotEqual(self.result(0, ran), "started_and_ran")

    def test_unobserved_texture_quality_keeps_the_gameplay_observation(self):
        rejected = self.events + [["mission", "0"]]
        result = {"observation": self.result(21, rejected)}
        result.update(texture_quality_observation(rejected, 2))
        self.assertEqual(
            result,
            {"observation": "mission_rejected", "requested_texture_quality": 2, "texture_quality_verified": False},
        )
        measured = [["texture_quality", "2", "3"]]
        self.assertEqual(
            texture_quality_observation(measured, 2),
            {
                "texture_quality": 2,
                "maximum_texture_quality": 3,
                "requested_texture_quality": 2,
                "texture_quality_verified": True,
            },
        )
        self.assertFalse(texture_quality_observation(measured, 1)["texture_quality_verified"])
        self.assertNotIn("texture_quality_verified", texture_quality_observation(measured, None))
