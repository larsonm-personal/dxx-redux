"""Numerical integration checks for the offline spectral report."""
import unittest

import numpy as np
from scipy import signal

from compare_music_spectra import RATE, align, compare, spectrum


class SpectralTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        rng = np.random.default_rng(42)
        samples = RATE * 15
        # Irregular musical-like amplitude envelope, with broadband content
        knots = rng.uniform(.03, .3, 151)
        envelope = np.interp(np.arange(samples), np.linspace(0, samples - 1, len(knots)), knots)
        mono = rng.normal(0, .2, samples) * envelope
        cls.pcm = np.column_stack((mono, -mono)).astype(np.float32)

    def test_gain_and_antiphase(self):
        _, delta, anchor, _ = compare(self.pcm * .5, self.pcm)
        np.testing.assert_allclose(delta, 0, atol=1e-5)
        self.assertAlmostEqual(anchor, 6.0206, places=4)
        _, stereo = spectrum(self.pcm)
        _, mono = spectrum(self.pcm[:, :1])
        np.testing.assert_allclose(stereo, mono)
        self.assertGreater(stereo.sum(), 0)

    def test_offset_and_silence(self):
        padded = np.pad(self.pcm * .4, ((RATE, 0), (0, 0)))
        ours, reference, result = align(self.pcm, padded)
        self.assertTrue(result['accepted'], result)
        self.assertAlmostEqual(result['offset_seconds'], 1, places=2)
        _, delta, _, _ = compare(ours, reference)
        np.testing.assert_allclose(delta, 0, atol=1e-4)

    def test_known_treble_cut(self):
        # Independent first-order low-pass: spectrum ratio should follow its response
        sos = signal.butter(1, 4000, fs=RATE, output='sos')
        filtered = signal.sosfilt(sos, self.pcm, axis=0)
        _, _, _, bands = compare(filtered, self.pcm)
        self.assertLess(bands['8000-16000'], -7)
        self.assertAlmostEqual(bands['250-2000'], 0, places=6)
        self.assertGreater(bands['2000-4000'], bands['8000-16000'])

    def test_unrelated_envelope_rejected(self):
        rng = np.random.default_rng(123)
        knots = rng.uniform(.03, .3, 151)
        env = np.interp(np.arange(len(self.pcm)), np.linspace(0, len(self.pcm) - 1, len(knots)), knots)
        other = rng.normal(0, .2, self.pcm.shape) * env[:, None]
        _, _, result = align(self.pcm, other.astype(np.float32))
        self.assertFalse(result['accepted'], result)

    def test_tempo_drift_rejected(self):
        positions = np.arange(len(self.pcm))
        stretched = np.column_stack([np.interp(positions * .96, positions, self.pcm[:, channel])
                                     for channel in range(2)]).astype(np.float32)
        _, _, result = align(self.pcm, stretched)
        self.assertFalse(result['accepted'], result)


if __name__ == '__main__':
    unittest.main()
