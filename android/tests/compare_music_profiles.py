"""Export and compare actual PCM from the production SF2/FM HMP paths.

Requires test_music_synth and the original registered D1 HOG. This checks
renderer selection and different PCM, not perceptual or DOS playback accuracy.
"""
import argparse
from array import array
import hashlib
import html
import json
import math
from pathlib import Path
import subprocess
import sys
import wave


def read_pcm(path):
    with wave.open(str(path), 'rb') as wav:
        assert (wav.getnchannels(), wav.getsampwidth(), wav.getframerate()) == (2, 2, 48000)
        data = wav.readframes(wav.getnframes())
    samples = array('h', data)
    if sys.byteorder != 'little':
        samples.byteswap()
    return data, samples


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--renderer', type=Path, required=True)
    parser.add_argument('--hog', type=Path, required=True)
    parser.add_argument('--sf2', type=Path, default=Path('android/app/src/main/assets/gm.sf2'))
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    report = {'songs': {}, 'passed': False}
    rows = []
    for song in ('descent', 'game01', 'game07', 'game08'):
        recordings = {}
        profiles = {}
        for selected in ('sf2', 'ymfm'):
            filename = f'{song}-{selected}.wav'
            result = subprocess.run(
                [str(args.renderer), '--render', str(args.sf2), str(args.hog),
                 song + '.hmp', str(args.output / filename), selected],
                capture_output=True, text=True, check=True, timeout=60)
            actual = result.stdout.split('actual=', 1)[1].split()[0]
            data, samples = read_pcm(args.output / filename)
            recordings[selected] = (data, samples)
            profiles[selected] = {
                'actual': actual, 'file': filename,
                'pcm_sha256': hashlib.sha256(data).hexdigest(),
                'peak': max(abs(x) for x in samples),
                'rms': math.sqrt(sum(x * x for x in samples) / len(samples)),
                'clipped_samples': sum(abs(x) >= 32767 for x in samples),
            }
            (args.output / f'{song}-{selected}.log').write_text(
                result.stdout + result.stderr, encoding='utf8')
        same = recordings['sf2'][0] == recordings['ymfm'][0]
        a, b = recordings['sf2'][1], recordings['ymfm'][1]
        difference = math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b)) / len(a))
        report['songs'][song] = {'profiles': profiles, 'identical_pcm': same, 'difference_rms': difference}
        assert profiles['sf2']['actual'] == 'sf2'
        assert profiles['ymfm']['actual'] == 'ymfm' and not same
        assert difference > 100 and profiles['ymfm']['peak'] > 100
        rows.append(f'<h2>{html.escape(song)}.hmp</h2>')
        rows.append('<p>' + ('Identical PCM: FM selection falls back to SF2.' if same else
                            'Different PCM: the FM selection uses ymfm.') + '</p>')
        for selected, profile in profiles.items():
            rows.append(f'<p>Selected: {selected}; actual: {profile["actual"]}</p>'
                        f'<audio controls preload="none" src="{profile["file"]}"></audio>')
        print(f'{song}: actual FM selection={profiles["ymfm"]["actual"]}, identical PCM={same}, difference RMS={difference:.1f}')
    report['passed'] = True
    (args.output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    (args.output / 'listen.html').write_text(
        '<!doctype html><meta charset="utf-8"><title>Production music profile comparison</title>'
        '<style>body{font:18px system-ui;max-width:760px;margin:40px auto;padding:0 20px}'
        'audio{width:100%}</style><h1>Soundfont / FM comparison</h1>'
        '<p>First 20 seconds through the production converter, synth and timeline. '
        'Cold start, bundled SF2 unless overridden, 48 kHz, -10 dB, 48 SF2 voices. '
        'No loudness normalization. These are host renders, not phone recordings.</p>'
        + ''.join(rows) + '<script>document.addEventListener("play",e=>{'
        'document.querySelectorAll("audio").forEach(a=>{if(a!==e.target)a.pause()})},true)</script>',
        encoding='utf8')


if __name__ == '__main__':
    main()
