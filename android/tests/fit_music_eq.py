"""Fit one measured D1+D2 SoundFont EQ, with three smoothing strengths."""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from scipy.ndimage import gaussian_filter1d
from scipy.optimize import least_squares
from scipy import signal

from compare_music_spectra import ROOT, axes_style, figure_data, plt


def coefficients(kind, frequency, gain, width, rate=48000):
    a = 10 ** (gain / 40)
    w = 2 * np.pi * frequency / rate
    c, s = np.cos(w), np.sin(w)
    if kind == 'peak':
        alpha = s / (2 * width)
        b = [1 + alpha * a, -2 * c, 1 - alpha * a]
        d = [1 + alpha / a, -2 * c, 1 - alpha / a]
    else:
        alpha = s / 2 * np.sqrt((a + 1 / a) * (1 / width - 1) + 2)
        t = 2 * np.sqrt(a) * alpha
        if kind == 'low_shelf':
            b = [a * ((a + 1) - (a - 1) * c + t), 2 * a * ((a - 1) - (a + 1) * c), a * ((a + 1) - (a - 1) * c - t)]
            d = [(a + 1) + (a - 1) * c + t, -2 * ((a - 1) + (a + 1) * c), (a + 1) + (a - 1) * c - t]
        else:
            b = [a * ((a + 1) + (a - 1) * c + t), -2 * a * ((a - 1) + (a + 1) * c), a * ((a + 1) + (a - 1) * c - t)]
            d = [(a + 1) - (a - 1) * c + t, 2 * ((a - 1) - (a + 1) * c), (a + 1) - (a - 1) * c - t]
    return np.array(b + d) / d[0]


KINDS = ['low_shelf', 'peak', 'peak', 'peak', 'high_shelf']


def bands_for(parameters):
    return [dict(type=kind, frequency_hz=float(np.exp(parameters[5 + i])), gain_db=float(parameters[i]),
                 width=float(parameters[10 + i - 1]) if 1 <= i <= 3 else 1.) for i, kind in enumerate(KINDS)]


def response(bands, frequencies, rate=48000):
    sos = np.array([coefficients(b['type'], b['frequency_hz'], b['gain_db'], b['width'], rate) for b in bands])
    _, values = signal.sosfreqz(sos, worN=frequencies, fs=rate)
    return 20 * np.log10(np.maximum(np.abs(values), 1e-15))


def fit(frequencies, target):
    start = np.r_[[-8., 0., 0., -2., -4.], np.log([100, 350, 1500, 4000, 9000]), [.6, .6, .6]]
    lower = np.r_[np.full(5, -12.), np.log([60, 200, 800, 2500, 7000]), [.3, .3, .3]]
    upper = np.r_[np.full(5, 2.), np.log([180, 700, 2200, 7000, 12000]), [1.4, 1.4, 1.4]]
    def residual(parameters):
        return np.r_[response(bands_for(parameters), frequencies) - target, parameters[:5] * .08]
    result = least_squares(residual, start, bounds=(lower, upper), max_nfev=1500)
    if not result.success:
        raise RuntimeError(result.message)
    return bands_for(result.x)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, default=ROOT / 'temp/music-spectral/report.json')
    parser.add_argument('--output', type=Path, default=ROOT / 'temp/music-eq')
    parser.add_argument('--publish', action='store_true', help='Publish generated measured preset to the app')
    args = parser.parse_args()
    baseline = json.loads(args.baseline.read_text(encoding='utf8'))
    rows = [r for r in baseline['tracks'] if r.get('alignment', {}).get('accepted')]
    frequencies = np.array(rows[0]['frequencies_hz'])
    differences = np.array([[np.nan if v is None else v for v in r['difference_db']] for r in rows])
    valid = (frequencies >= 60) & (frequencies <= 12000) & np.all(np.isfinite(differences), axis=0)
    frequencies = frequencies[valid]
    differences = differences[:, valid]
    mean = np.mean(differences, axis=0)
    step = float(np.median(np.diff(np.log2(frequencies))))
    font = ROOT / 'android/app/src/main/assets/gm.sf2'
    data = font.read_bytes()
    assert hashlib.sha256(data).hexdigest() == baseline['soundfont_sha256'], 'Baseline used a different bank'
    fnv = 14695981039346656037
    for value in data:
        fnv = ((fnv ^ value) * 1099511628211) & ((1 << 64) - 1)
    preset = dict(id='measured-sc55', name='Measured EQ - match SC-55 recordings',
                  soundfont='nitro-shoe SC-55-style 1.34', soundfont_sha256=baseline['soundfont_sha256'],
                  baseline_sha256=hashlib.sha256(args.baseline.read_bytes()).hexdigest(),
                  soundfont_bytes=len(data), soundfont_fnv64=f'{fnv:016x}',
                  method='Equal-song arithmetic mean of midrange-anchored dB differences; Gaussian log-frequency smoothing; five broad biquads',
                  songs=[r['id'] for r in rows], trusted_range_hz=[60, 12000], profiles=[])
    fig, axes = plt.subplots(2, 1, figsize=(11, 8), constrained_layout=True)
    axes[0].plot(frequencies, -mean, color='#bbbbbb', label='Inverse mean (28 songs)')
    colors = ['#2589a8', '#8846b0', '#cd7b24']
    for index, (name, fwhm, color) in enumerate(zip(['Detail', 'Balanced', 'Broad'], [.5, 1., 2.], colors), 1):
        target = -gaussian_filter1d(mean, fwhm / 2.35482 / step, mode='nearest')
        bands = fit(frequencies, target)
        # Static attenuation prevents boosting the maximum frequency response
        dense = np.geomspace(10, 23990, 3000)
        preamp = -max(0., float(np.max(response(bands, dense))))
        correction = response(bands, frequencies)
        before = np.sqrt(np.mean(differences ** 2, axis=1))
        after = np.sqrt(np.mean((differences + correction) ** 2, axis=1))
        profile = dict(native_id=index, name=name, smoothing_fwhm_octaves=fwhm, preamp_db=preamp, bands=bands,
                       fit_rmse_db=float(np.sqrt(np.mean((correction - target) ** 2))),
                       mean_song_error_before_db=float(np.mean(before)), mean_song_error_after_db=float(np.mean(after)),
                       improved_songs=int(np.count_nonzero(after < before)),
                       mean_residual_db=(mean + correction).tolist())
        # Leave-one-song-out validates the fitting strategy; the final preset uses all songs
        held_out = []
        for i in range(len(rows)):
            training = np.mean(np.delete(differences, i, axis=0), axis=0)
            candidate = fit(frequencies, -gaussian_filter1d(training, fwhm / 2.35482 / step, mode='nearest'))
            held_out.append(float(np.sqrt(np.mean((differences[i] + response(candidate, frequencies)) ** 2))))
        profile['leave_one_out_mean_error_db'] = float(np.mean(held_out))
        profile['leave_one_out_improved_songs'] = int(np.count_nonzero(np.array(held_out) < before))
        preset['profiles'].append(profile)
        axes[0].plot(frequencies, correction, color=color, label=f'{name}: {fwhm:g}-octave smoothing')
        axes[1].plot(frequencies, mean + correction, color=color, label=name)
        print(name, 'before', np.mean(before), 'after', np.mean(after), 'held out', np.mean(held_out), flush=True)
    axes[0].set_ylabel('Correction (dB), excluding safety preamp')
    axes[1].set_ylabel('Mean residual (dB)')
    for axis in axes:
        axes_style(axis)
        axis.axhline(0, color='black', linewidth=.8)
        axis.legend()
    args.output.mkdir(parents=True, exist_ok=True)
    image = figure_data(fig, args.output / 'measured-eq.png')
    (args.output / 'preset.json').write_text(json.dumps(preset, indent=2) + '\n', encoding='utf8')
    table = ''.join(f'<tr><td>{p["name"]}</td><td>{p["mean_song_error_before_db"]:.2f}</td><td>{p["mean_song_error_after_db"]:.2f}</td><td>{p["leave_one_out_mean_error_db"]:.2f}</td><td>{p["leave_one_out_improved_songs"]}/{len(rows)}</td></tr>' for p in preset['profiles'])
    native_link = '<p><a href="native/report.html">Production before/after graphs and listening comparisons</a></p>' if (args.output / 'native/report.html').exists() else ''
    (args.output / 'report.html').write_text(f'''<!doctype html><html lang="en"><meta charset="utf-8"><title>Measured SC-55 EQ</title>
<style>body{{font:17px system-ui;max-width:1100px;margin:35px auto;padding:20px;background:#f5f6fa;color:#243043}}img{{width:100%}}td,th{{padding:12px;text-align:left}}p{{line-height:1.6}}</style>
<h1>One measured correction for D1 + D2</h1><p>Equal weight per accepted song: 23 D1 and 5 D2 songs. The curves average differences in dB relative to the midrange, then smooth across frequency before fitting five broad parametric filters. Balanced is the recommended compromise. All songs use the same selected curve.</p>
<img alt="Smoothing variants and mean residual" src="{image}"><table><tr><th>Smoothing</th><th>Before error (dB)</th><th>After error (dB)</th><th>Held-out error (dB)</th><th>Held-out songs improved</th></tr>{table}</table>
<p>Error is root-mean-square spectral difference over 60 Hz-12 kHz, averaged over songs. Held-out fits omit the evaluated song. This validates the averaging strategy within this corpus, not an independent recording collection. Safety preamp is a constant gain and is excluded from tonal error.</p>
<p>Measurements outside 60 Hz-12 kHz do not drive the fit; shelves extend naturally to the limits. Flat remains available. The preset applies only to the measured nitro-shoe SC-55-style 1.34 SoundFont, after synthesis effects, before conversion to PCM16. It does not change FM, recordings or sound effects.</p>
<p><a href="preset.json">Exact parameters and provenance</a></p>{native_link}</html>''', encoding='utf8')
    if args.publish:
        asset = ROOT / 'android/app/src/main/assets/music_eq_sc55.json'
        asset.write_text(json.dumps(preset, indent=2) + '\n', encoding='utf8')
        header = ['// Generated by android/tests/fit_music_eq.py --publish; do not edit', '#ifndef DXX_MUSIC_EQ_PRESETS_H', '#define DXX_MUSIC_EQ_PRESETS_H', '#include <cstdint>',
                  f'constexpr uint64_t music_eq_font_fnv = UINT64_C(0x{fnv:016x});', f'constexpr size_t music_eq_font_bytes = {len(data)};',
                  'struct music_eq_band { int type; double frequency, gain, width; };',
                  'struct music_eq_profile { double preamp; music_eq_band bands[5]; };',
                  'constexpr music_eq_profile music_eq_profiles[] = {']
        for profile in preset['profiles']:
            header.append('    { %.12g, {' % profile['preamp_db'])
            for band in profile['bands']:
                header.append('        { %d, %.12g, %.12g, %.12g },' % (['low_shelf', 'peak', 'high_shelf'].index(band['type']), band['frequency_hz'], band['gain_db'], band['width']))
            header.append('    } },')
        header.extend(['};', '#endif', ''])
        (ROOT / 'android/app/src/main/cpp/shared/music_eq_presets.h').write_text('\n'.join(header), encoding='utf8')


if __name__ == '__main__':
    main()
