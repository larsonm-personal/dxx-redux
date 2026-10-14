#!/usr/bin/env python3
"""Negative-oracle checks; these do not substitute for actual headed runs."""

import copy
import importlib.util
from pathlib import Path
import unittest

SPEC = importlib.util.spec_from_file_location("music", Path(__file__).parents[1] / "helpers" / "host_music_campaign.py")
music = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(music)


class MusicOracleTests(unittest.TestCase):
    def setUp(self):
        self.before = {
            "config": {"DigiVolume": 0, "MusicVolume": 8},
            "music": {
                "audio_open": 1,
                "volume": 128,
                "playing": 1,
                "song": 0,
                "paused": 0,
                "play_result": 0,
                "last_track": "descent.hmp",
                "pcm_samples": 100,
                "pcm_nonzero": 100,
                "pcm_absolute": 200,
            },
        }
        self.after = copy.deepcopy(self.before)
        self.after["music"].update(pcm_samples=200, pcm_nonzero=200, pcm_absolute=400)

    def test_actual_pcm_required(self):
        music.check_music_window(self.before, self.after, audible=True)
        self.after["music"].update(pcm_nonzero=100, pcm_absolute=200)
        with self.assertRaisesRegex(AssertionError, "only silence"):
            music.check_music_window(self.before, self.after, audible=True)

    def test_measured_dither_floor_and_loud_negative_control(self):
        self.after["music"].update(pcm_nonzero=125, pcm_absolute=225)
        music.check_music_window(self.before, self.after, audible=False)
        self.after["music"].update(pcm_nonzero=200, pcm_absolute=400)
        with self.assertRaisesRegex(AssertionError, "dither floor"):
            music.check_music_window(self.before, self.after, audible=False)

    def test_jukebox_frequency_negative_control(self):
        observation = {"after": {"volume": 128}, "pcm": {"positive_crossing_hz": 440, "mean_absolute": 5093}}
        music.check_jukebox_waveform(observation)
        observation["pcm"]["positive_crossing_hz"] = 880
        with self.assertRaisesRegex(AssertionError, "not 440 Hz"):
            music.check_jukebox_waveform(observation)

    def test_jukebox_gain_negative_control(self):
        observation = {"after": {"volume": 16}, "pcm": {"positive_crossing_hz": 440, "mean_absolute": 636}}
        music.check_jukebox_waveform(observation)
        observation["pcm"]["mean_absolute"] = 5093
        with self.assertRaisesRegex(AssertionError, "gain mismatch"):
            music.check_jukebox_waveform(observation)

    def test_wrong_builtin_payload_rejected(self):
        native = {"midi_bytes": 58378, "midi_fnv1a64": "f364d03c4f51172c"}
        music.check_midi_source(native, native)
        with self.assertRaisesRegex(AssertionError, "Wrong builtin MIDI"):
            music.check_midi_source({"midi_bytes": 28508, "midi_fnv1a64": "c7eef94ee5d81281"}, native)

    def test_unavailable_not_pass(self):
        self.after["music"]["playing"] = 0
        with self.assertRaises(music.MusicUnavailable):
            music.check_music_window(self.before, self.after, audible=True)

    def test_stalled_callback_not_silence_pass(self):
        with self.assertRaises(music.MusicUnavailable):
            music.check_music_window(self.before, self.before, audible=False)

    def test_stop_leak_detected(self):
        self.after["music"].update(playing=0, song=-1)
        with self.assertRaisesRegex(AssertionError, "leaked"):
            music.check_music_window(self.before, self.after, audible=False, stopped=True)

    def test_sfx_contamination_rejected(self):
        self.after["config"]["DigiVolume"] = 8
        with self.assertRaisesRegex(AssertionError, "muted sound effects"):
            music.check_music_window(self.before, self.after, audible=True)

    def test_volume_mismatch_rejected(self):
        self.after["music"]["volume"] = 0
        with self.assertRaisesRegex(AssertionError, "volume mismatch"):
            music.check_music_window(self.before, self.after, audible=True)


if __name__ == "__main__":
    unittest.main()
