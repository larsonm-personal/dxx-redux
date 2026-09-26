"""Render the fitted EQ through production code and compare the full accepted corpus."""
import argparse
import html
import json
from pathlib import Path
import time

import imageio_ffmpeg
import numpy as np

from compare_music_spectra import ROOT, RATE, axes_style, compare, decode, digest, figure_data, levels, plt, run, write_wav


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, default=ROOT / 'temp/music-spectral/report.json')
    parser.add_argument('--output', type=Path, default=ROOT / 'temp/music-eq/native')
    parser.add_argument('--renderer', type=Path, default=ROOT / 'android/build/host-extract-tests/Release/test_music_synth.exe')
    parser.add_argument('--eq', type=int, choices=[1, 2, 3], default=2)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    baseline = json.loads(args.baseline.read_text())
    manifest = json.loads((ROOT / 'android/tests/music_spectral/sc55.json').read_text())
    font = ROOT / manifest['soundfont']
    assert digest(font) == baseline['soundfont_sha256']
    hogs = {g['game']: ROOT / g['hog'] for g in manifest['games']}
    ffmpeg = imageio_ffmpeg.get_ffmpeg_exe()
    rows, sections, checked_flat = [], [], set()
    for track in baseline['tracks']:
        if not track.get('alignment', {}).get('accepted'):
            continue
        name = track['id']
        print(name, flush=True)
        old_wav = args.baseline.parent / (name + '-raw.wav')
        assert digest(old_wav) == track['pcm_sha256']
        if track['game'] not in checked_flat:
            flat = args.output / (name + '-flat.wav')
            run([args.renderer, '--render-eq', font, hogs[track['game']], track['song'] + '.hmp', flat, 'sf2', baseline['settings']['seconds'], 0])
            assert digest(flat) == digest(old_wav), 'Flat changed production PCM'
            checked_flat.add(track['game'])
        corrected = args.output / (name + '-corrected.wav')
        start = time.monotonic()
        result = run([args.renderer, '--render-eq', font, hogs[track['game']], track['song'] + '.hmp', corrected, 'sf2', baseline['settings']['seconds'], args.eq], text=True)
        assert f'equalizer={args.eq}' in result.stdout
        elapsed = time.monotonic() - start
        pcm = decode(ffmpeg, corrected, baseline['settings']['seconds'])
        ref = decode(ffmpeg, ROOT / track['reference'], baseline['settings']['seconds'] + 8)
        a = track['alignment']
        n = round(a['seconds'] * RATE)
        pcm = pcm[round(a['ours_start_seconds'] * RATE):][:n]
        ref = ref[round(a['reference_start_seconds'] * RATE):][:n]
        _, delta, _, bands = compare(pcm, ref)
        before = np.array([np.nan if v is None else v for v in track['difference_db']])
        f = np.array(track['frequencies_hz'])
        valid = (f >= 60) & (f <= 12000) & np.isfinite(before) & np.isfinite(delta)
        measured = levels(ffmpeg, pcm)
        assert measured['boundary_samples'] == 0, f'{name} clips'
        rows.append(dict(id=name, game=track['game'], before_error_db=float(np.sqrt(np.mean(before[valid] ** 2))),
                         after_error_db=float(np.sqrt(np.mean(delta[valid] ** 2))), band_difference_db=bands,
                         delta=[float(x) if np.isfinite(x) else None for x in delta],
                         true_peak_dbfs=measured['true_peak_dbfs'], render_seconds=elapsed,
                         pcm_sha256=digest(corrected)))
        raw = decode(ffmpeg, old_wav, baseline['settings']['seconds'])[round(a['ours_start_seconds'] * RATE):][:n]
        target = min(-23., *[m['lufs'] - m['true_peak_dbfs'] - 2 for m in (track['ours'], track['reference_levels'], measured)])
        players = []
        for label, audio, measurement in [('Flat', raw, track['ours']), ('Measured', pcm, measured), ('Recording', ref, track['reference_levels'])]:
            filename = f'{name}-{label}.wav'
            write_wav(args.output / filename, audio[:30 * RATE] * 10 ** ((target - measurement['lufs']) / 20))
            players.append(f'<label>{label}<audio controls preload="none" src="{filename}"></audio></label>')
        sections.append(f'<section><h2>{html.escape(name)}</h2><p>Spectral error {rows[-1]["before_error_db"]:.2f} to {rows[-1]["after_error_db"]:.2f} dB</p><div class="players">{"".join(players)}</div></section>')
        (args.output / 'validation.json').write_text(json.dumps(rows, indent=2) + '\n')
    fig, axes = plt.subplots(1, 2, figsize=(12, 4), constrained_layout=True)
    for axis, game in zip(axes, ['d1', 'd2']):
        original = [np.array([np.nan if v is None else v for v in r['difference_db']]) for r in baseline['tracks'] if r['game'] == game and r.get('alignment', {}).get('accepted')]
        after = [np.array([np.nan if v is None else v for v in r['delta']]) for r in rows if r['game'] == game]
        axis.plot(f, np.nanmean(original, axis=0), label='Flat', color='#c57123')
        axis.plot(f, np.nanmean(after, axis=0), label='Measured EQ', color='#7947b2')
        axes_style(axis)
        axis.axhline(0, color='black', linewidth=.8)
        axis.set_ylabel('Mean difference from recording (dB)')
        axis.set_title(game.upper())
        axis.legend()
    image = figure_data(fig, args.output / 'native-comparison.png')
    before = float(np.mean([r['before_error_db'] for r in rows]))
    after = float(np.mean([r['after_error_db'] for r in rows]))
    (args.output / 'report.html').write_text(f'''<!doctype html><html lang="en"><meta charset="utf-8"><title>Native measured EQ validation</title>
<style>body{{font:17px system-ui;max-width:1150px;margin:40px auto;padding:15px;background:#f5f6fa}}img{{width:100%}}section{{background:white;padding:20px;margin:20px 0}}.players{{display:flex;flex-wrap:wrap;gap:20px}}audio{{display:block;width:310px}}</style>
<h1>Measured EQ through the production renderer</h1><p>One shared preset across {len(rows)} accepted songs. Mean spectral error: {before:.2f} to {after:.2f} dB, over 60 Hz-12 kHz. Flat PCM matches the pre-EQ implementation exactly on a D1 and D2 song. No PCM clipping in corrected compared sections.</p>
<img alt="Production EQ comparison" src="{image}"><p>Graphs re-match each section's midrange. Listening clips use constant gains derived from full-section LUFS, not dynamic normalization. These are the first 30 seconds of each matched section.</p>
<p><a href="../report.html">Smoothing strategy</a> | <a href="validation.json">Raw validation results</a></p>{''.join(sections)}
<script>document.addEventListener('play',e=>{{document.querySelectorAll('audio').forEach(a=>{{if(a!==e.target)a.pause()}})}},true)</script></html>''', encoding='utf8')
    print(f'Mean measured error {before:.3f} -> {after:.3f} dB; {len(rows)} tracks', flush=True)


if __name__ == '__main__':
    main()
