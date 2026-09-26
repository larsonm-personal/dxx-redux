"""Render production HMP audio and create an offline spectral comparison report.

Run from any directory; manifest asset paths resolve against the repository.
See music_spectral/README.md for reproduction and measurement limitations.
"""
import argparse
import base64
import csv
import hashlib
import html
import io
import json
from pathlib import Path
import re
import subprocess
import sys
import wave

import imageio_ffmpeg
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
from scipy import signal

ROOT = Path(__file__).resolve().parents[2]
RATE = 48000
BANDS = [(80, 250), (250, 2000), (2000, 4000), (4000, 8000), (8000, 16000)]
EDGES = np.geomspace(40, 18000, 73)


def run(command, **kwargs):
    return subprocess.run([str(x) for x in command], check=True, capture_output=True,
                          timeout=180, **kwargs)


def digest(path):
    with Path(path).open('rb') as source:
        return hashlib.file_digest(source, 'sha256').hexdigest()


def db(power):
    return 10 * np.log10(np.maximum(power, 1e-20))


def decode(ffmpeg, path, seconds):
    result = run([ffmpeg, '-v', 'error', '-i', path, '-t', seconds,
                  '-f', 'f32le', '-ac', 2, '-ar', RATE, '-'])
    return np.frombuffer(result.stdout, dtype='<f4').reshape(-1, 2).copy()


def envelope(pcm):
    n = len(pcm) // 480
    energy = np.mean(pcm[:n * 480].reshape(n, 480, 2).astype(float) ** 2, axis=(1, 2))
    values = np.log(np.maximum(energy, 1e-10))
    # Remove slow level changes while retaining the rhythmic envelope
    return values - signal.savgol_filter(values, min(101, (n // 2) * 2 - 1), 2)


def best_offset(a, b, max_lag=800):
    scores = []
    for lag in range(-max_lag, max_lag + 1):
        aa, bb = a[max(0, -lag):], b[max(0, lag):]
        n = min(len(aa), len(bb))
        if n < 200:
            scores.append(-1.)
            continue
        aa, bb = aa[:n] - np.mean(aa[:n]), bb[:n] - np.mean(bb[:n])
        scores.append(float(np.dot(aa, bb) / max(np.linalg.norm(aa) * np.linalg.norm(bb), 1e-12)))
    index = int(np.argmax(scores))
    return index - max_lag, scores[index]


def align(ours, reference):
    a, b = envelope(ours), envelope(reference)
    lag, score = best_offset(a, b)
    start_a, start_b = max(0, -lag) * 480, max(0, lag) * 480
    n = min(len(ours) - start_a, len(reference) - start_b)
    aa, bb = ours[start_a:start_a + n], reference[start_b:start_b + n]
    # Discard matched leading/trailing near-silence, retaining internal rests
    count = n // 480
    energies = [np.mean(x[:count * 480].reshape(count, 480, 2) ** 2, axis=(1, 2)) for x in (aa, bb)]
    active = np.flatnonzero((energies[0] > max(energies[0].max() * 1e-5, 1e-10)) &
                           (energies[1] > max(energies[1].max() * 1e-5, 1e-10)))
    if len(active) < 200:
        raise ValueError('Less than two seconds of shared active audio')
    lo, hi = int(active[0]) * 480, (int(active[-1]) + 1) * 480
    aa, bb = aa[lo:hi], bb[lo:hi]
    residuals, scores = [], []
    ea, eb = envelope(aa), envelope(bb)
    for indexes in np.array_split(np.arange(len(ea)), 3):
        if len(indexes) >= 300:
            offset, local_score = best_offset(ea[indexes], eb[indexes], 50)
            residuals.append(offset / 100)
            scores.append(local_score)
    drift = max(residuals, default=0) - min(residuals, default=0)
    accepted = score >= .35 and drift <= .20 and min(scores, default=0) >= .25
    return aa, bb, dict(offset_seconds=lag / 100, correlation=score,
                        section_offsets_seconds=residuals, section_correlations=scores,
                        drift_seconds=drift, accepted=accepted,
                        ours_start_seconds=(start_a + lo) / RATE,
                        reference_start_seconds=(start_b + lo) / RATE,
                        seconds=len(aa) / RATE)


def spectrum(pcm):
    frequency, power = signal.welch(pcm, fs=RATE, window='hann', nperseg=4096,
                                    noverlap=3072, axis=0, detrend=False)
    return frequency, np.mean(power, axis=1)


def band_power(frequency, power, low, high):
    width = frequency[1] - frequency[0]
    overlap = np.maximum(0, np.minimum(frequency + width / 2, high) -
                         np.maximum(frequency - width / 2, low))
    return float(np.sum(power * overlap))


def compare(ours, reference):
    frequency, a = spectrum(ours)
    _, b = spectrum(reference)
    # Anchor to the midrange, making the treble/mid balance independent of volume
    anchor = float(db(band_power(frequency, b, 250, 2000)) - db(band_power(frequency, a, 250, 2000)))
    curves = [np.array([band_power(frequency, p, lo, hi) for lo, hi in zip(EDGES[:-1], EDGES[1:])]) for p in (a, b)]
    delta = db(curves[0]) + anchor - db(curves[1])
    valid = (curves[0] > max(curves[0]) * 1e-6) & (curves[1] > max(curves[1]) * 1e-6)
    delta[~valid] = np.nan
    bands = {f'{low}-{high}': float(db(band_power(frequency, a, low, high)) + anchor -
                                  db(band_power(frequency, b, low, high))) for low, high in BANDS}
    return curves, delta, anchor, bands


def write_wav(path, pcm):
    with wave.open(str(path), 'wb') as target:
        target.setparams((2, 2, RATE, 0, 'NONE', 'not compressed'))
        target.writeframes(np.rint(np.clip(pcm * 32768, -32768, 32767)).astype('<i2').tobytes())


def levels(ffmpeg, pcm):
    result = run([ffmpeg, '-hide_banner', '-f', 'f32le', '-ar', RATE, '-ac', 2, '-i', '-',
                  '-af', 'loudnorm=I=-23:TP=-2:LRA=7:print_format=json', '-f', 'null', '-'],
                 input=pcm.astype('<f4').tobytes())
    match = re.search(rb'\{\s*"input_i".*?\}', result.stderr, re.S)
    if not match:
        raise ValueError('FFmpeg did not return loudness measurements')
    values = json.loads(match.group())
    return dict(lufs=float(values['input_i']), true_peak_dbfs=float(values['input_tp']),
                rms_dbfs=float(db(np.mean(pcm.astype(float) ** 2))),
                boundary_samples=int(np.count_nonzero((pcm >= 32767 / 32768) | (pcm <= -1))))


def figure_data(figure, path):
    figure.savefig(path, dpi=140, bbox_inches='tight')
    stream = io.BytesIO()
    figure.savefig(stream, format='png', dpi=120, bbox_inches='tight')
    plt.close(figure)
    return 'data:image/png;base64,' + base64.b64encode(stream.getvalue()).decode('ascii')


def axes_style(axis):
    axis.set_xscale('log')
    axis.set_xlim(40, 18000)
    axis.set_xticks([63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000],
                   ['63', '125', '250', '500', '1k', '2k', '4k', '8k', '16k'])
    axis.grid(True, alpha=.2)
    axis.set_xlabel('Frequency (Hz)')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, default=Path(__file__).parent / 'music_spectral/sc55.json')
    parser.add_argument('--renderer', type=Path, default=ROOT / 'android/build/host-extract-tests/Release/test_music_synth.exe')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--seconds', type=int, default=90, choices=range(20, 121), metavar='20..120')
    parser.add_argument('--limit', type=int, help='Limit pairs for a smoke run')
    parser.add_argument('--reuse-renders', action='store_true', help='Reuse only hash-verified PCM from a prior report')
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    previous = json.loads((output / 'report.json').read_text(encoding='utf8')) if args.reuse_renders and (output / 'report.json').exists() else {}
    manifest = json.loads(args.manifest.read_text(encoding='utf8'))
    ffmpeg = imageio_ffmpeg.get_ffmpeg_exe()
    font = ROOT / manifest['soundfont']
    cases = []
    for game in manifest['games']:
        songs = {song: song for song in game['songs']}
        songs.update(game.get('aliases', {}))
        cases.extend((game, song, reference) for song, reference in songs.items())
    if args.limit:
        cases = cases[:args.limit]
    report = dict(family=manifest['family'], manifest_sha256=digest(args.manifest),
                  renderer_sha256=digest(args.renderer), soundfont_sha256=digest(font),
                  revision=run(['git', '-C', ROOT, 'rev-parse', 'HEAD'], text=True).stdout.strip(),
                  ffmpeg=run([ffmpeg, '-version'], text=True).stdout.splitlines()[0],
                  python=sys.version, numpy=np.__version__, scipy=__import__('scipy').__version__,
                  matplotlib=matplotlib.__version__,
                  settings=dict(rate=RATE, seconds=args.seconds, voices=128, gain_db=-10,
                                fluid_gain=.4, interpolation='fourth-order', reverb=True, chorus=True,
                                start='cold', spectrum='Welch 4096 Hann, 75% overlap, stereo power average',
                                normalization='250-2000 Hz power anchor; listening clips use constant LUFS gain'),
                  excluded=manifest['excluded'], tracks=[])
    prior_tracks = {row['id']: row for row in previous.get('tracks', [])}
    panels, deltas = [], {'d1': [], 'd2': []}
    centers = np.sqrt(EDGES[:-1] * EDGES[1:])
    for game, song, reference_name in cases:
        name = f'{game["game"]}-{song}'
        reference = ROOT / game['references'] / (reference_name + '.ogg')
        row = dict(id=name, game=game['game'], song=song, reference=str(reference.relative_to(ROOT)))
        print(f'Rendering {name}', flush=True)
        try:
            wav = output / (name + '-raw.wav')
            prior = prior_tracks.get(name, {})
            reusable = (all(previous.get(key) == report[key] for key in ('renderer_sha256', 'soundfont_sha256', 'revision', 'settings'))
                        and prior.get('hog_sha256') == digest(ROOT / game['hog'])
                        and wav.exists() and prior.get('pcm_sha256') == digest(wav))
            if not reusable:
                result = run([args.renderer.resolve(), '--render', font, ROOT / game['hog'],
                              song + '.hmp', wav, 'sf2', args.seconds], text=True)
                (output / (name + '.log')).write_text(result.stdout + result.stderr, encoding='utf8')
                if 'actual=sf2' not in result.stdout:
                    raise ValueError('Renderer did not select SF2')
            ours, ref, alignment = align(decode(ffmpeg, wav, args.seconds), decode(ffmpeg, reference, args.seconds + 8))
            curves, delta, anchor, bands = compare(ours, ref)
            ours_levels, ref_levels = levels(ffmpeg, ours), levels(ffmpeg, ref)
            if not all(np.isfinite(x) for x in [ours_levels['lufs'], ref_levels['lufs']]):
                raise ValueError('Nonfinite loudness for matched section')
            row.update(alignment=alignment, ours=ours_levels, reference_levels=ref_levels,
                       midrange_gain_db=anchor, band_difference_db=bands,
                       frequencies_hz=centers.tolist(), difference_db=[float(x) if np.isfinite(x) else None for x in delta],
                       reference_sha256=digest(reference), hog_sha256=digest(ROOT / game['hog']), pcm_sha256=digest(wav))
            if alignment['accepted']:
                deltas[game['game']].append(delta)
            # A common target ensures listening gains are static and never introduce clipping
            target = min(-23., ours_levels['lufs'] - ours_levels['true_peak_dbfs'] - 2,
                         ref_levels['lufs'] - ref_levels['true_peak_dbfs'] - 2)
            row['listening_target_lufs'] = target
            for label, pcm, measurement in [('ours', ours, ours_levels), ('reference', ref, ref_levels)]:
                write_wav(output / f'{name}-{label}.wav', pcm[:30 * RATE] * 10 ** ((target - measurement['lufs']) / 20))
            fig, axes = plt.subplots(2, 1, figsize=(11, 6), constrained_layout=True)
            axes[0].plot(centers, db(curves[0]) + anchor, label='Our playback (midrange matched)', color='#dc6a25')
            axes[0].plot(centers, db(curves[1]), label='SC-55 recording', color='#187bbf')
            axes[0].set_ylabel('Band power (dBFS)')
            axes[0].legend(loc='lower left')
            axes[0].set_title(name + ' | ' + ('aligned' if alignment['accepted'] else 'alignment uncertain: excluded from summary'))
            axes[1].plot(centers, delta, color='#7447bd')
            axes[1].axhline(0, color='black', linewidth=.8)
            axes[1].set_ylabel('Ours minus recording (dB)')
            for axis in axes:
                axes_style(axis)
            image = figure_data(fig, output / (name + '.png'))
            band_text = ' | '.join(f'{band} Hz: {value:+.1f} dB' for band, value in bands.items())
            panels.append(f'<section id="{name}"><h2>{name}</h2><p>{html.escape(band_text)}</p>'
                          f'<p>Compared {alignment["seconds"]:.1f}s; reference offset {alignment["offset_seconds"]:+.2f}s; '
                          f'envelope correlation {alignment["correlation"]:.2f}; section drift {alignment["drift_seconds"]:.2f}s. '
                          f'Raw loudness: ours {ours_levels["lufs"]:.1f}, reference {ref_levels["lufs"]:.1f} LUFS. '
                          f'Our true peak {ours_levels["true_peak_dbfs"]:.1f} dBFS; PCM boundary samples {ours_levels["boundary_samples"]}.</p>'
                          f'<img alt="Spectral comparison for {name}" src="{image}">'
                          f'<div class="players"><label>Our playback<audio controls preload="none" src="{name}-ours.wav"></audio></label>'
                          f'<label>Recording<audio controls preload="none" src="{name}-reference.wav"></audio></label></div>'
                          '<p>First 30 seconds of the matched section, static loudness gain measured over the full compared section.</p></section>')
        except (subprocess.CalledProcessError, ValueError, OSError) as error:
            row['error'] = str(error)
            if isinstance(error, subprocess.CalledProcessError):
                row['error'] += '\n' + str(error.stderr)[-3000:]
            panels.append(f'<section><h2>{name}: unavailable</h2><pre>{html.escape(row["error"])}</pre></section>')
            print(row['error'], flush=True)
        report['tracks'].append(row)
        (output / 'report.json').write_text(json.dumps(report, indent=2, allow_nan=False) + '\n', encoding='utf8')
    fig, axes = plt.subplots(1, 2, figsize=(12, 4), constrained_layout=True)
    report['summary'] = {}
    summary_rows = []
    for axis, (game, values) in zip(axes, deltas.items()):
        if values:
            data = np.array(values)
            median = np.nanmedian(data, axis=0)
            low, high = np.nanpercentile(data, [25, 75], axis=0)
            axis.plot(centers, median, color='#7447bd')
            axis.fill_between(centers, low, high, alpha=.2, color='#7447bd', label='Middle 50% of songs')
            axis.legend()
            report['summary'][game] = dict(accepted_tracks=len(values), median_difference_db=[float(x) if np.isfinite(x) else None for x in median])
            tracks = [row for row in report['tracks'] if row['game'] == game and row.get('alignment', {}).get('accepted')]
            band_medians = {f'{low}-{high}': float(np.median([row['band_difference_db'][f'{low}-{high}'] for row in tracks])) for low, high in BANDS}
            report['summary'][game]['median_band_difference_db'] = band_medians
            summary_rows.append(f'<tr><td>{game.upper()}</td><td>{len(values)}</td>' +
                                ''.join(f'<td>{value:+.1f} dB</td>' for value in band_medians.values()) + '</tr>')
        axis.axhline(0, color='black', linewidth=.8)
        axis.set_title(f'{game.upper()}: {len(values)} accepted pairs')
        axis.set_ylabel('Ours minus recording (dB)')
        axes_style(axis)
    summary_image = figure_data(fig, output / 'summary.png')
    accepted = sum(len(x) for x in deltas.values())
    links = ' '.join(f'<a href="#{row["id"]}">{row["id"]}</a>' for row in report['tracks'] if 'error' not in row)
    document = f'''<!doctype html><html lang="en"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>D1 / D2 spectral comparison</title>
<style>body{{font:16px system-ui;color:#202a38;background:#f3f5f8;max-width:1200px;margin:32px auto;padding:0 20px}}
section,.intro{{background:white;padding:24px;margin:24px 0;border-radius:12px}}h1{{font-size:34px}}p{{line-height:1.6}}
img{{width:100%;height:auto}}nav a{{display:inline-block;margin:5px}}.players{{display:flex;gap:25px;flex-wrap:wrap}}
audio{{display:block;width:360px;max-width:100%}}pre{{white-space:pre-wrap}}.badge{{color:#704519;font-weight:bold}}</style>
<h1>D1 / D2: is our MIDI playback brighter?</h1><div class="intro">
<p class="badge">Measured baseline. No EQ applied. {accepted} of {len(cases)} pairs accepted for the summary.</p>
<p>Bundled SoundFont through the production FluidSynth renderer versus local SC-55-labelled OGG recordings.
Each graph matches energy at 250 Hz-2 kHz. Positive differences above that range mean more treble relative to the midrange,
independent of the overall volume setting. Shading shows variation across songs, not a confidence interval.</p>
<p>These are cold-start host renders, up to {args.seconds} seconds per song, with reverb/chorus and 128 voices.
Alignment uses rhythmic energy envelopes and three section checks. Poorly aligned pairs are excluded from the summary.
Passing this check does not prove identical arrangement or patch selection. Codec/mastering differences and reference provenance
can affect high frequencies; this is evidence for investigation, not an automatically justified corrective EQ.</p>
<p>{html.escape(manifest['excluded'])}</p><img alt="Aggregate spectral difference" src="{summary_image}">
<div style="overflow:auto"><table cellpadding="10"><caption>Median band differences, relative to matched midrange</caption>
<thead><tr><th>Game</th><th>Pairs</th>{''.join(f'<th>{lo}-{hi} Hz</th>' for lo, hi in BANDS)}</tr></thead>
<tbody>{''.join(summary_rows)}</tbody></table></div>
<p><a href="report.json">Detailed measurements and provenance</a> | <a href="summary.csv">CSV measurements</a></p>
<nav>{links}</nav></div>{''.join(panels)}
<script>document.addEventListener('play',e=>{{document.querySelectorAll('audio').forEach(a=>{{if(a!==e.target)a.pause()}})}},true)</script></html>'''
    (output / 'report.html').write_text(document, encoding='utf8')
    (output / 'report.json').write_text(json.dumps(report, indent=2, allow_nan=False) + '\n', encoding='utf8')
    with (output / 'summary.csv').open('w', newline='', encoding='utf8') as target_file:
        writer = csv.writer(target_file)
        writer.writerow(['track', 'accepted', 'correlation', 'seconds', *[f'{a}-{b} Hz dB' for a, b in BANDS], 'error'])
        for row in report['tracks']:
            writer.writerow([row['id'], row.get('alignment', {}).get('accepted', False),
                             row.get('alignment', {}).get('correlation'), row.get('alignment', {}).get('seconds'),
                             *[row.get('band_difference_db', {}).get(f'{a}-{b}') for a, b in BANDS], row.get('error', '')])
    print(f'Report: {output / "report.html"}; accepted {accepted}/{len(cases)}', flush=True)
    if not accepted:
        raise SystemExit('No reliable pairs; inspect per-track errors and alignment')


if __name__ == '__main__':
    main()
