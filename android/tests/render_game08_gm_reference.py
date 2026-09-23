"""Compare a DOS game08 GM capture with production MIDI and same-SF2 audio.

This validates events and makes an apples-to-apples synth comparison available;
it is not an SC-55 emulation or a subjective calibration target.
"""
import argparse
from array import array
import json
import math
from pathlib import Path
import subprocess
import sys
import wave

from midi_diff import compare, note_states, read_midi
from run_dos_midi_parity import run


def metrics(path):
    with wave.open(str(path), 'rb') as wav:
        assert (wav.getnchannels(), wav.getsampwidth(), wav.getframerate()) == (2, 2, 48000)
        pcm = array('h', wav.readframes(wav.getnframes()))
    if sys.byteorder != 'little':
        pcm.byteswap()
    rms = math.sqrt(sum(float(x) * x for x in pcm) / len(pcm)) / 32768
    return {'rms_dbfs': 20 * math.log10(rms), 'peak': max(abs(x) for x in pcm) / 32768,
            'clipped_samples': sum(abs(x) >= 32767 for x in pcm)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bin', type=Path, default=Path('android/build/host-extract-tests/Release'))
    parser.add_argument('--reference', type=Path, default=Path('game_data/music/dos-references/descent14-game08'))
    parser.add_argument('--fixture', type=Path, default=Path('android/tests/fixtures/dos-midi/descent14-game08.json'))
    parser.add_argument('--sf2', type=Path, default=Path('android/app/src/main/assets/gm.sf2'))
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--second-capture', type=Path)
    args = parser.parse_args()
    if not run((args.bin / 'hmp_midi_export.exe').resolve(), args.fixture, args.reference, args.output):
        raise RuntimeError('DOS parity regression failed')
    spec = json.loads(args.fixture.read_text())
    start = spec['reference_start_ms']
    assert start == 0, 'This title capture compares cold starts'
    dos = args.reference / spec['capture']
    candidate = args.output / 'repeat.mid'
    reference_events, _ = read_midi(dos)
    candidate_events, _ = read_midi(Path(str(candidate) + '.tml.mid'))
    notes = [e for e in reference_events if e.status == 0x99 and e.data[0] == 67 and e.data[1] and e.ms < 20000]
    states = [s for s in note_states(reference_events, 0, 20000)[9] if s['note'][0] == 67]
    candidate_states = [s for s in note_states(candidate_events, 0, 20000)[9] if s['note'][0] == 67]
    assert states == candidate_states
    report = {'opening_agogo_states_match': True,
              'opening_agogo_notes': [dict(ms=e.ms, **s) for e, s in zip(notes, states)],
              'audio': {}, 'scope': 'Same bundled SF2 and TinySoundFont, fixed gain; not SC-55 hardware audio'}
    if args.second_capture:
        repeat_check = compare(dos, args.second_capture, duration=30010, tolerance=10)
        assert repeat_check['passed'], repeat_check
        (args.output / 'unattended-repeat-diff.json').write_text(json.dumps(repeat_check, indent=2) + '\n', encoding='utf8')
        report['unattended_second_capture_matches'] = True
    for label, source in [('dos', dos), ('production', candidate)]:
        for stem, options in [('mix', [-1, source, 0]), ('agogo', [9, 67])]:
            target = args.output / f'{label}-{stem}.wav'
            subprocess.run(list(map(str, [args.bin / 'midi_tsf_render.exe', args.sf2, source, target,
                                          0, 20000, *options])), check=True)
            report['audio'][f'{label}-{stem}'] = metrics(target)
    for stem in ('mix', 'agogo'):
        report['audio'][stem + '_production_minus_dos_db'] = (
            report['audio']['production-' + stem]['rms_dbfs'] - report['audio']['dos-' + stem]['rms_dbfs'])
    (args.output / 'audio-comparison.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    clips = [('dos-mix', 'Original DOS General MIDI through bundled SF2'),
             ('production-mix', 'Production HMP conversion through the same SF2'),
             ('dos-agogo', 'DOS: isolated opening agogo'),
             ('production-agogo', 'Production: isolated opening agogo')]
    sections = ''.join(f'<h2>{label}</h2><audio controls preload="none" src="{file}.wav"></audio>' for file, label in clips)
    (args.output / 'listen.html').write_text(
        '<!doctype html><meta charset="utf-8"><title>Game08 verified GM comparison</title>'
        '<style>body{font:18px system-ui;max-width:850px;margin:40px auto;padding:0 20px}'
        'audio{width:100%}h2{font-size:20px}</style><h1>Game08: verified HMP / GM comparison</h1>'
        '<p>The original DOS game and our converter produce matching GM messages in the '
        '50.010-second test window. Both recordings below use the same bundled SF2, '
        'TinySoundFont, voice limit and fixed gain. No per-clip normalization or bell attenuation.</p>'
        '<p>These are host synth renders of the first 20 seconds, not original SC-55 audio. '
        'They test the effect of MIDI conversion; they do not establish ideal soundfont balance.</p>'
        + sections + '<script>document.addEventListener("play",e=>document.querySelectorAll("audio")'
        '.forEach(a=>{if(a!==e.target)a.pause()}),true)</script>', encoding='utf8')
    print(json.dumps(report['audio'], indent=2))


if __name__ == '__main__':
    main()
