"""Replay a DOSBox register capture and prepare a gain-matched listening page."""

import argparse
import html
import json
import math
from pathlib import Path
import subprocess

from experiment import listening_copy, metrics, samples, write_json
from opl_trace import export


def envelope(path):
    rate, pcm = samples(path)
    result = []
    for i in range(len(pcm) * 100 // (2 * rate)):
        block = pcm[round(i * rate / 100) * 2:round((i + 1) * rate / 100) * 2]
        result.append(math.sqrt(sum(x * x for x in block) / len(block)))
    return result


def locate(reference, replay, seconds=20):
    # Search the beginning of the WAV anywhere in the longer register replay
    a, b = envelope(reference)[:seconds * 100], envelope(replay)
    if len(a) != seconds * 100 or len(b) < len(a):
        raise ValueError('Capture audio too short for alignment')
    mean = sum(a) / len(a)
    a = [x - mean for x in a]
    energy = sum(x * x for x in a)
    best = (-2, 0)
    for lag in range(len(b) - len(a) + 1):
        clip = b[lag:lag + len(a)]
        total = sum(clip)
        denominator = math.sqrt(max(0, energy * (sum(x * x for x in clip) - total * total / len(a))))
        score = sum(x * y for x, y in zip(a, clip)) / denominator if denominator else 0
        best = max(best, (score, lag))
    return {'replay_start_ms': best[1] * 10, 'envelope_correlation': round(best[0], 6),
            'meaning': 'Timing/amplitude-envelope diagnostic; not waveform or timbre parity; no time stretching'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', type=Path, required=True)
    parser.add_argument('--wav', type=Path, required=True, help='WAV recorded during the same DRO capture')
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--adb', type=Path)
    parser.add_argument('--serial')
    args = parser.parse_args()
    if bool(args.adb) != bool(args.serial):
        parser.error('--adb and --serial must be provided together')
    output = args.output
    report = {'capture': export(args.capture, output), 'reference_wav': metrics(args.wav), 'renders': {}}
    duration = math.ceil(report['capture']['duration_ms'] / 1000)
    remote = '/data/local/tmp/dxx-fm-feasibility'
    adb = [str(args.adb), '-s', args.serial] if args.adb else []
    if adb:
        subprocess.run([*adb, 'shell', 'mkdir', '-p', remote], check=True)
        for local, name in [(args.probe, 'fm_probe_opl'), (output / 'registers.tsv', 'registers.tsv')]:
            subprocess.run([*adb, 'push', str(local), f'{remote}/{name}'], check=True)
        subprocess.run([*adb, 'shell', 'chmod', '755', f'{remote}/fm_probe_opl'], check=True)
    for mode in ('registers-opl3', 'registers-ymfm', 'registers-emu8950'):
        target = output / f'{mode}.wav'
        command = ([*adb, 'shell', f'{remote}/fm_probe_opl', mode, '-', f'{remote}/registers.tsv', f'{remote}/{mode}.wav', str(duration)]
                   if adb else [str(args.probe), mode, '-', str(output / 'registers.tsv'), str(target), str(duration)])
        timing = json.loads(subprocess.check_output(command, text=True))
        if adb:
            subprocess.run([*adb, 'pull', f'{remote}/{mode}.wav', str(target)], check=True)
        report['renders'][mode] = dict(metrics(target), **timing)
    alignment = locate(args.wav, output / 'registers-opl3.wav')
    report['alignment'] = alignment
    comparisons = [('DOSBox original', args.wav, 0, 'compare-dos.wav'),
                   ('BSD ymfm: original OPL3 writes', output / 'registers-opl3.wav', alignment['replay_start_ms'], 'compare-opl3.wav'),
                   ('BSD ymfm: dual OPL2 approximation', output / 'registers-ymfm.wav', alignment['replay_start_ms'], 'compare-dual-ymfm.wav'),
                   ('MIT emu8950: dual OPL2 approximation', output / 'registers-emu8950.wav', alignment['replay_start_ms'], 'compare-dual-emu8950.wav')]
    sections = []
    for label, source, start, filename in comparisons:
        report.setdefault('listening_copies', {})[filename] = listening_copy(source, output / filename, 20, start)
        sections.append(f'<h2>{html.escape(label)}</h2><audio controls preload="metadata" src="{filename}"></audio>')
    page = '''<!doctype html><html lang="en"><meta charset="utf-8"><title>Level 7 OPL register replay</title>
<style>body{font:18px system-ui;max-width:900px;margin:40px auto;padding:0 20px;background:#17212b;color:#e9f0f5}audio{width:100%}h2{font-size:21px}a{color:#87cefa}</style>
<h1>Level 7: replaying the original DOS register writes</h1>
<p>Twenty-second excerpts from the same DOSBox session, approximately aligned and gain-matched.
This is a chip comparison using captured original driver output. It is not yet live MIDI-driven playback.</p>
<p>The game enables OPL3 and pairs its nine voices across left/right outputs with different levels.
The dual OPL2 versions approximate those writes; native OPL3 is the faithful hardware configuration.</p>
<p>The capture begins partway through the song. These clips do not contain the opening drums.</p>'''
    page += ''.join(sections)
    page += f'<p>Envelope correlation: {alignment["envelope_correlation"]}; timing diagnostic only.</p>'
    page += '<p><a href="replay-report.json">Measurements</a> | <a href="driver-comparison.json">HMP/register comparison</a></p></html>'
    (output / 'listen.html').write_text(page, encoding='utf8')
    write_json(output / 'replay-report.json', report)
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
