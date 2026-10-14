"""Negative controls for bounded filtering-change/render-capability assertions."""

import sys
from pathlib import Path
from types import SimpleNamespace
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "helpers"))
from host_render_campaign import ERROR_FIELDS, validate_capture, validate_difference, ProbeFailure


def rgb_frame(pixels):
    return SimpleNamespace(size=(640, 480), mode="RGB", tobytes=lambda: pixels)


class RenderOracleTests(unittest.TestCase):
    def setUp(self):
        self.metadata = {"width": 640, "height": 480, "written": True, **dict.fromkeys(ERROR_FIELDS, 0)}
        self.image = rgb_frame(
            bytes(channel for y in range(480) for x in range(640) for channel in (x % 256, y % 256, (x + y) % 256)),
        )

    def test_nontrivial_frame_accepted(self):
        validate_capture(self.metadata, self.image, 640 * 480 * 3)

    def test_blank_negative_control(self):
        with self.assertRaisesRegex(ProbeFailure, "spatial content"):
            validate_capture(self.metadata, rgb_frame(bytes(640 * 480 * 3)), 640 * 480 * 3)

    def test_identical_negative_control(self):
        with self.assertRaisesRegex(ProbeFailure, "meaningful rendered difference"):
            validate_difference(self.image, rgb_frame(self.image.tobytes()))

    def test_known_rgb_difference_statistics(self):
        result = validate_difference(rgb_frame(bytes(640 * 480 * 3)), rgb_frame(bytes([3, 6, 9]) * (640 * 480)))
        self.assertEqual(result, {"changed_pixels": 640 * 480, "mean_absolute_channel_delta": 6.0})

    def test_difference_rejects_incomplete_frame(self):
        with self.assertRaisesRegex(ProbeFailure, "complete 640x480 RGB frames"):
            validate_difference(self.image, rgb_frame(self.image.tobytes()[:-1]))

    def test_gl_error_negative_control(self):
        for field in ERROR_FIELDS:
            with self.subTest(field=field), self.assertRaisesRegex(ProbeFailure, "GL diagnostics"):
                validate_capture({**self.metadata, field: 1282}, self.image, 640 * 480 * 3)

    def test_truncated_bytes_rejected(self):
        with self.assertRaisesRegex(ProbeFailure, "byte count"):
            validate_capture(self.metadata, self.image, 640 * 480 * 3 - 1)

    def test_wrong_dimensions_rejected(self):
        with self.assertRaisesRegex(ProbeFailure, "dimensions"):
            validate_capture({**self.metadata, "width": 320}, self.image, 640 * 480 * 3)


if __name__ == "__main__":
    unittest.main()
