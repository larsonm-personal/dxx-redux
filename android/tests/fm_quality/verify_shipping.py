"""Compare the real-time default to the approved offline clean audition."""
import argparse
import json
from pathlib import Path
import subprocess
import time

import numpy as np

from experiment import RATE, read_audio, trace


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--reference', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    reference = json.loads((args.reference / 'report.json').read_text())
    seconds = reference['seconds']
    args.output.mkdir(parents=True, exist_ok=True)
    repo = Path(__file__).resolve().parents[3]
    report = {'causal_delay_frames': 48, 'rate': RATE, 'tracks': {}}
    for song in reference['tracks']:
        wav = args.output / f'{song}-shipping.wav'
        registers = args.output / f'{song}-shipping.registers.txt'
        start = time.perf_counter()
        result = subprocess.run([str((args.build / 'fm_quality_shipping.exe').resolve()), '--render',
            str(repo / 'android/app/src/main/assets/gm.sf2'), reference['hog'], f'{song}.hmp',
            str(wav.resolve()), 'ymfm', str(seconds + 1), str(registers.resolve())],
            check=True, capture_output=True, text=True)
        elapsed = time.perf_counter() - start
        (args.output / f'{song}.log').write_text(result.stdout + result.stderr)
        assert 'actual=ymfm' in result.stdout, result.stdout
        assert trace(registers, RATE, seconds) == trace(args.reference / 'raw' / f'{song}-current.registers.txt', RATE, seconds)
        rate, actual = read_audio(wav)
        expected_rate, expected = read_audio(args.reference / 'raw' / f'{song}-sinc.wav')
        assert rate == expected_rate == RATE
        actual = actual[48:48 + len(expected)]
        # Ignore the reference's final boundary; compare without fitting gain or timing
        actual, expected = actual[:-RATE], expected[:-RATE]
        error = actual - expected
        snr = float(10 * np.log10(np.mean(expected ** 2) / np.mean(error ** 2)))
        assert snr > 65, (song, snr)
        assert np.max(np.abs(actual)) < 0.999, song
        integration = subprocess.run([str((args.build / 'fm_quality_shipping.exe').resolve()),
            str(repo / 'android/app/src/main/assets/gm.sf2'), reference['hog'], f'{song}.hmp',
            str((args.output / f'{song}.mid').resolve())], check=True, capture_output=True, text=True)
        (args.output / f'{song}-integration.log').write_text(integration.stdout + integration.stderr)
        assert 'FM seek: exact PCM' in integration.stdout, integration.stdout
        report['tracks'][song] = {'snr_vs_offline_db': snr, 'register_events_equal': True,
                                 'seek_and_repeat_passed': True,
                                 'render_seconds': elapsed, 'audio_seconds': seconds + 1}
        print(f'{song}: {snr:.2f} dB agreement; {seconds + 1}s rendered in {elapsed:.2f}s', flush=True)
    (args.output / 'shipping-report.json').write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
