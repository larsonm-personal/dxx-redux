"""Reproduce the game01 SC-55 voice-limit comparison and isolated high part."""
import argparse
import html
import json
from pathlib import Path
import subprocess

import numpy as np
import soundfile as sf


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fluid', type=Path, required=True)
    parser.add_argument('--font', type=Path, required=True)
    parser.add_argument('--midi', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    variants = [('voices48', 'Original experiment: 48 voices', 48, -1),
                ('voices128', '128 voices', 128, -1),
                ('choir', 'Isolated channel 5: Synth Strings 2 (128 voices)', 128, 4)]
    report = {}
    page = ['<!doctype html><meta charset="utf-8"><title>game01 SC-55 diagnosis</title>',
            '<h1>game01 SC-55: 27-34 seconds</h1>',
            '<p>Same MIDI, gain, reverb and chorus. No filtering or level normalization. '
            'The isolated channel identifies a part; it is not a proposed mix change.</p>']
    for name, label, voices, channel in variants:
        raw = args.output / (name + '.wav')
        result = subprocess.run([str(args.fluid), str(args.font), str(args.midi), str(raw),
                                 '36', '1', '1', '24', str(voices), str(channel), '7'],
                                capture_output=True, text=True, check=True, timeout=180)
        (args.output / (name + '.log')).write_text(result.stdout + result.stderr)
        report[name] = json.loads(result.stdout)
        pcm, rate = sf.read(raw)
        assert rate == 48000 and len(pcm) == rate * 36
        excerpt = name + '-excerpt.wav'
        sf.write(args.output / excerpt, pcm[rate * 27:rate * 34], rate, subtype='PCM_16')
        page.append(f'<h2>{html.escape(label)}</h2><audio controls src="{excerpt}"></audio>')
    a, _ = sf.read(args.output / 'voices48.wav')
    b, _ = sf.read(args.output / 'voices128.wav')
    diff = a[29 * 48000:32 * 48000] - b[29 * 48000:32 * 48000]
    report['difference_29_32_seconds'] = {'rms': float(np.sqrt(np.mean(diff * diff))),
                                        'peak': float(np.max(np.abs(diff)))}
    (args.output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    (args.output / 'listen.html').write_text('\n'.join(page), encoding='utf8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
