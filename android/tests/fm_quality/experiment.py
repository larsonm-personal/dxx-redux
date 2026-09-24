"""Offline FM output audition; all shipped files remain untouched."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import shutil
import subprocess

import numpy as np
import scipy
from scipy import signal
from scipy.io import wavfile

RATE = 48000
NATIVE_RATE = 49715
VARIANTS = {
    'current': ('Current', 'Pre-upgrade synth output, including its integer rounding and simple rate conversion.'),
    'float': ('More precision', 'The same rate-conversion algorithm, with fractional precision retained through gain and mixing.'),
    'sinc': ('Clean resampling', 'Native-rate ymfm output, converted with a windowed-sinc filter and higher-precision processing.'),
    'warm': ('Warm: two poles', 'Original warm version: clean resampling plus 5 Hz DC removal and a two-pole 8 kHz low-pass.'),
    'warm1': ('Warm: one pole', 'Same 8 kHz cutoff and DC removal, with a gentler roll-off above the cutoff. A tonal option, not measured sound-card emulation.'),
}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_audio(path):
    rate, data = wavfile.read(path)
    assert data.ndim == 2 and data.shape[1] == 2, path
    if data.dtype == np.int16:
        data = data.astype(np.float64) / 32768
    else:
        assert data.dtype == np.float32, (path, data.dtype)
        data = data.astype(np.float64)
    assert np.isfinite(data).all(), path
    return rate, data


def loudness(data):
    # BS.1770 K weighting at 48 kHz, stereo weights 1, 400 ms blocks / 100 ms hop
    weighted = signal.lfilter(
        [1.53512485958697, -2.69169618940638, 1.19839281085285],
        [1, -1.69065929318241, 0.73248077421585], data, axis=0)
    weighted = signal.lfilter([1, -2, 1], [1, -1.99004745483398, 0.99007225036621], weighted, axis=0)
    power = np.sum(weighted * weighted, axis=1)
    integral = np.r_[0, np.cumsum(power)]
    starts = np.arange(0, len(data) - 19200 + 1, 4800)
    energies = (integral[starts + 19200] - integral[starts]) / 19200
    levels = -0.691 + 10 * np.log10(np.maximum(energies, 1e-30))
    absolute = energies[levels > -70]
    assert len(absolute), 'No audible program'
    relative = -0.691 + 10 * np.log10(absolute.mean()) - 10
    gated = energies[(levels > -70) & (levels > relative)]
    return float(-0.691 + 10 * np.log10(gated.mean()))


def measure(data):
    # Fourfold oversampled peak estimate; leave generous headroom on top
    peak = float(np.max(np.abs(signal.resample_poly(data, 4, 1, axis=0))))
    return {
        'loudness_lufs': loudness(data),
        'peak_dbfs': float(20 * np.log10(max(np.max(np.abs(data)), 1e-30))),
        'true_peak_estimate_dbtp': float(20 * np.log10(max(peak, 1e-30))),
        'rms_dbfs': float(10 * np.log10(max(np.mean(data * data), 1e-30))),
    }


def trace(path, rate, seconds):
    result = []
    for line in path.read_text().splitlines():
        frame, register, value = map(int, line.split())
        if frame < seconds * rate:
            result.append((round(frame * 1000 / rate), register, value))
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--hog', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--seconds', type=int, default=60, choices=range(10, 120))
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[3]
    output = args.output.resolve()
    raw = output / 'raw'
    raw.mkdir(parents=True, exist_ok=True)
    sf = repo / 'android/app/src/main/assets/gm.sf2'
    # SF2 is loaded by the shipping fallback API, but every render must select ymfm
    report = {
        'seconds': args.seconds, 'output_rate': RATE, 'native_rate': NATIVE_RATE,
        'hog': str(args.hog.resolve()), 'hog_sha256': sha(args.hog),
        'numpy': np.__version__, 'scipy': scipy.__version__,
        'dither': 'Deterministic TPDF, one PCM16 LSB, final conversion only',
        'loudness': 'Stereo K-weighted gated integrated estimate; constant gain per clip; no compression',
        'variants': VARIANTS, 'tracks': {}, 'source_hashes': {},
    }
    for path in [repo / 'android/get_deps/tool_versions.conf',
                 repo / 'android/app/src/main/cpp/extract/test_music_synth.c',
                 repo / 'android/app/src/main/cpp/shared/music_synth.cpp',
                 repo / 'android/app/src/main/cpp/shared/hmp_android_shared.c',
                 repo / 'android/app/src/main/cpp/shared/midi_seek_timeline.c',
                 repo / 'android/tests/fm_feasibility/patch_live.py',
                 repo / 'android/tests/fm_feasibility/patch_hmi.py',
                 *Path(__file__).parent.glob('*.py')]:
        report['source_hashes'][str(path.relative_to(repo))] = sha(path)
    up, down = RATE // math.gcd(RATE, NATIVE_RATE), NATIVE_RATE // math.gcd(RATE, NATIVE_RATE)
    taps = signal.firwin(96 * max(up, down) + 1, 0.94 / max(up, down), window=('kaiser', 8.6))
    report['resampler'] = {'up': up, 'down': down, 'taps': len(taps),
                           'cutoff_hz': 22560, 'window': 'Kaiser beta=8.6', 'delay_compensated': True}
    def resample(data):
        return signal.resample_poly(data, up, down, axis=0, window=taps)
    # Test the anti-aliasing improvement separately from the music
    tone_time = np.arange(NATIVE_RATE) / NATIVE_RATE
    tone_results = {}
    for frequency in (1000, 20000, 24600):
        converted = resample(np.sin(2 * np.pi * frequency * tone_time))
        tone_results[str(frequency)] = float(20 * np.log10(np.sqrt(2 * np.mean(converted[4800:-4800] ** 2))))
    assert abs(tone_results['1000']) < 0.05 and abs(tone_results['20000']) < 0.1, tone_results
    assert tone_results['24600'] < -65, tone_results
    report['resampler_tone_gain_db'] = tone_results
    dc_filter = signal.butter(1, 5, btype='highpass', fs=RATE, output='sos')
    warm_filters = {mode: signal.butter(order, 8000, fs=RATE, output='sos')
                    for mode, order in [('warm', 2), ('warm1', 1)]}
    frequencies = [1000, 4000, 8000, 12000, 16000]
    report['warm_filter_response_db_before_level_matching'] = {}
    for mode, coefficients in warm_filters.items():
        _, response = signal.sosfreqz(np.vstack([dc_filter, coefficients]), worN=frequencies, fs=RATE)
        report['warm_filter_response_db_before_level_matching'][mode] = dict(zip(
            map(str, frequencies), (20 * np.log10(np.abs(response))).tolist()))
    upper_target = -20.0
    for song in ('game01', 'game07', 'game08'):
        print(f'Rendering {song}', flush=True)
        arrangement = f'{song}.hmp' if song == 'game07' else f'{song}.hmq'
        selection = None
        for mode in ('current', 'float', 'native'):
            executable = args.build / f'fm_quality_{mode}.exe'
            wav = raw / f'{song}-{mode}.wav'
            registers = raw / f'{song}-{mode}.registers.txt'
            command = [str(executable.resolve()), '--render', str(sf), str(args.hog.resolve()),
                       f'{song}.hmp', str(wav), 'ymfm', str(args.seconds + (mode == 'native')), str(registers)]
            result = subprocess.run(command, capture_output=True, text=True, check=True)
            (raw / f'{song}-{mode}.log').write_text(result.stdout + result.stderr, encoding='utf8')
            assert 'actual=ymfm' in result.stdout, result.stdout
            assert f'sequence={arrangement}' in result.stderr, result.stderr
            current_selection = next(line for line in result.stderr.splitlines() if line.startswith('renderer='))
            if selection is None:
                selection = current_selection
            assert selection == current_selection, f'{song}: instrument bank / arrangement selection differs'
        expected = trace(raw / f'{song}-current.registers.txt', RATE, args.seconds)
        assert expected == trace(raw / f'{song}-float.registers.txt', RATE, args.seconds), f'{song}: float events differ'
        assert expected == trace(raw / f'{song}-native.registers.txt', NATIVE_RATE, args.seconds), f'{song}: native events differ'
        # Repeat native and baseline independently, compare bytes, then discard duplicate files
        for mode in ('current', 'native'):
            repeated = raw / f'{song}-{mode}-repeat.wav'
            result = subprocess.run([str((args.build / f'fm_quality_{mode}.exe').resolve()), '--render',
                str(sf), str(args.hog.resolve()), f'{song}.hmp', str(repeated), 'ymfm',
                str(args.seconds + (mode == 'native'))], capture_output=True, text=True, check=True)
            assert sha(repeated) == sha(raw / f'{song}-{mode}.wav'), f'{song}: nondeterministic {mode}'
            repeated.unlink()
        rate, native = read_audio(raw / f'{song}-native.wav')
        assert rate == NATIVE_RATE
        clean = resample(native)[:args.seconds * RATE] * 10 ** (-10 / 20)
        del native
        wavfile.write(raw / f'{song}-sinc.wav', RATE, clean.astype(np.float32))
        dc_removed = signal.sosfilt(dc_filter, clean, axis=0)
        for mode, coefficients in warm_filters.items():
            warm = signal.sosfilt(coefficients, dc_removed, axis=0)
            wavfile.write(raw / f'{song}-{mode}.wav', RATE, warm.astype(np.float32))
        track = {'arrangement': arrangement, 'selection': selection,
                 'register_writes': len(expected), 'register_events_equal': True,
                 'current_and_native_repeat_exactly': True, 'variants': {}}
        for mode in VARIANTS:
            rate, data = read_audio(raw / f'{song}-{mode}.wav')
            assert rate == RATE and len(data) == args.seconds * RATE
            stats = measure(data)
            upper_target = min(upper_target, stats['loudness_lufs'] - stats['true_peak_estimate_dbtp'] - 1.5)
            track['variants'][mode] = dict(stats, raw_sha256=sha(raw / f'{song}-{mode}.wav'))
        report['tracks'][song] = track
    # Shared target across every track and variant, with no limiter and generous peak margin
    target = math.floor(upper_target * 10) / 10
    report['target_lufs'] = target
    for song, track in report['tracks'].items():
        outputs = {}
        for index, mode in enumerate(VARIANTS):
            _, data = read_audio(raw / f'{song}-{mode}.wav')
            stats = track['variants'][mode]
            gain_db = target - stats['loudness_lufs']
            data *= 10 ** (gain_db / 20)
            rng = np.random.default_rng(20260923 + index)
            integers = np.rint(data * 32768 + rng.random(data.shape) - rng.random(data.shape))
            assert np.max(np.abs(integers)) < 32767, f'{song}/{mode}: clipping'
            pcm = integers.astype(np.int16)
            filename = f'{song}-{mode}.wav'
            wavfile.write(output / filename, RATE, pcm)
            measured = measure(pcm.astype(np.float64) / 32768)
            assert abs(measured['loudness_lufs'] - target) < 0.1, measured
            assert measured['true_peak_estimate_dbtp'] < -1.3, measured
            stats.update(file=filename, listen_gain_db=gain_db, listen_metrics=measured, sha256=sha(output / filename))
            outputs[mode] = pcm.astype(np.float64) / 32768
        reference = outputs['current']
        for mode in VARIANTS:
            if mode == 'current':
                continue
            difference = outputs[mode] - reference
            assert np.any(difference), f'{song}/{mode}: duplicate audio'
            track['variants'][mode]['difference_from_current_rms_dbfs'] = float(10 * np.log10(np.mean(difference ** 2)))
    (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    page = (Path(__file__).parent / 'listen.html').read_text(encoding='utf8')
    page = page.replace('/*REPORT*/null', json.dumps(report).replace('</', '<\\/'))
    (output / 'listen.html').write_text(page, encoding='utf8')
    shutil.copyfile(Path(__file__).parent / 'README.md', output / 'README.md')
    count = len(report['tracks']) * len(VARIANTS)
    print(f'PASS: {count} clips, equal register events, repeatable renders, no clipping; target {target:.1f} LUFS', flush=True)
    print(output / 'listen.html', flush=True)


if __name__ == '__main__':
    main()
