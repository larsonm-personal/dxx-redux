"""Create local listening clips from the Level 7 parity runner's MIDI artifacts."""
import argparse
import array
import hashlib
import html
import json
import math
from pathlib import Path
import subprocess
import sys
import wave


def measurements(path):
    with wave.open(str(path)) as audio:
        if audio.getsampwidth() != 2:
            raise ValueError('Expected PCM16 WAV')
        samples = array.array('h', audio.readframes(audio.getnframes()))
        if sys.byteorder != 'little':
            samples.byteswap()
        rms = math.sqrt(sum(x * x for x in samples) / len(samples)) / 32768
        return {'seconds': audio.getnframes() / audio.getframerate(),
                'rms_dbfs': round(20 * math.log10(max(rms, 1e-12)), 3),
                'peak': round(max(map(abs, samples)) / 32768, 6),
                'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}


def main():
    repo = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--renderer', type=Path, default=repo / 'android/build/host-extract-tests/Release/midi_tsf_render.exe')
    parser.add_argument('--midi-directory', type=Path, default=repo / 'temp/midi-parity/reference/descent14-game07')
    parser.add_argument('--reference', type=Path, default=repo / 'game_data/music/dos-references/descent14-game07/dos.mid')
    parser.add_argument('--fixture', type=Path, default=Path(__file__).parent / 'fixtures/dos-midi/descent14-game07.json')
    parser.add_argument('--soundfont', type=Path, default=repo / 'android/app/src/main/assets/gm.sf2')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--adlib-wav', type=Path, help='Optional raw DOSBox WAV; preserves its lead-in without claiming alignment')
    args = parser.parse_args()
    spec = json.loads(args.fixture.read_text())
    if hashlib.sha256(args.reference.read_bytes()).hexdigest() != spec['capture_sha256']:
        raise ValueError('Reference MIDI hash mismatch')
    args.output.mkdir(parents=True, exist_ok=True)
    clips = [('before', 'all-devices.mid.tml.mid', -1), ('after', 'repeat.mid.tml.mid', -1),
             ('before-drums', 'all-devices.mid.tml.mid', 9), ('after-drums', 'repeat.mid.tml.mid', 9)]
    results = {}
    for name, midi, channel in clips:
        output = args.output / f'{name}.wav'
        subprocess.run([str(args.renderer), str(args.soundfont), str(args.midi_directory / midi),
                        str(output), '0', '20000', str(channel), str(args.reference),
                        str(spec['reference_start_ms'])], check=True)
        results[name] = measurements(output)
    output = args.output / 'dos-gm-same-synth.wav'
    subprocess.run([str(args.renderer), str(args.soundfont), str(args.reference), str(output),
                    str(spec['reference_start_ms']), '20000'], check=True)
    results['dos-gm-same-synth'] = measurements(output)
    if args.adlib_wav:
        output = args.output / 'dos-adlib.wav'
        with wave.open(str(args.adlib_wav)) as source, wave.open(str(output), 'wb') as target:
            target.setparams(source.getparams())
            target.writeframes(source.readframes(source.getframerate() * 22))
        results['dos-adlib'] = measurements(output)
    report = {'duration_ms': 20000, 'gain_db': -10, 'max_voices': 48, 'sample_rate': 48000,
              'soundfont_sha256': hashlib.sha256(args.soundfont.read_bytes()).hexdigest(),
              'drum_rms_change_db': round(results['after-drums']['rms_dbfs'] - results['before-drums']['rms_dbfs'], 3),
              'clips': results,
              'notes': 'Before deliberately enables every device track. Same synth/gain/context for GM clips. AdLib keeps its original gain and briefing lead-in. Prefix reconstruction omits preceding voices. No subjective listening judgment is inferred.'}
    (args.output / 'measurements.json').write_text(json.dumps(report, indent=2) + '\n')
    labels = {'before': 'Before: overlapping FM, GM and digital tracks',
              'after': 'After: General MIDI tracks only',
              'dos-gm-same-synth': 'Captured DOS General MIDI through the same Android synth',
              'dos-adlib': 'Original DOS AdLib audio (different synthesis/gain, includes briefing lead-in)',
              'before-drums': 'Before: percussion channel alone',
              'after-drums': 'After: percussion channel alone'}
    sections = '\n'.join(f'<p>{html.escape(labels[name])}<br><audio controls preload="none" src="{name}.wav"></audio></p>'
                         for name in labels if name in results)
    (args.output / 'listen.html').write_text(
        '<!doctype html><html lang="en"><meta charset="utf-8"><title>Descent Level 7 music comparison</title>'
        '<style>body{font:18px system-ui;max-width:850px;margin:40px auto;padding:0 20px}audio{width:100%}</style>'
        '<h1>Descent Level 7</h1><p>The GM clips use the same soundfont, gain, and preceding-song state. '
        'The corrected arrangement removes overlapping instrument parts. AdLib uses different synthesis; '
        'its clip is not loudness-normalized or precisely time-aligned.</p>' + sections + '</html>\n', encoding='utf8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
