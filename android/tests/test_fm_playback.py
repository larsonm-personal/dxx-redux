"""Compare production FM HMP conversion with the independent experiment parser.

Requires the host test_music_synth executable and a user-supplied D1 or D2 HOG.
The executable also verifies nonzero/unclipped PCM and exact reset rendering.
"""
import argparse
import json
from pathlib import Path
import subprocess
import struct

from fm_feasibility.experiment import hmp_events, hog_members
from midi_diff import read_midi


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--renderer', type=Path, required=True)
    parser.add_argument('--hog', type=Path, required=True)
    parser.add_argument('--sf2', type=Path, default=Path('android/app/src/main/assets/gm.sf2'))
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    members = hog_members(args.hog)
    report = {'songs': {}, 'passed': False}
    songs = [row.split()[0].lower() for row in members['descent.sng'].decode('ascii').splitlines()
             if len(row.split()) == 3 and row.split()[0].lower() in members]
    assert songs, 'No available SNG songs'
    for song in dict.fromkeys(songs):
        sequence = song[:-1] + 'q' if song[:-1] + 'q' in members else song
        try:
            rate, source, selection = hmp_events(members[sequence], 20)
        except ValueError as error:
            if str(error) != 'No notes in selected arrangement':
                raise
            # Some original files (D2 briefing) supply GM music but no FM notes
            # Require the application's fallback to preserve the GM conversion
            result = subprocess.run([str(args.renderer), str(args.sf2), str(args.hog), song,
                                      str(args.output / (song + '.mid')), '--expect-sf2'],
                                     capture_output=True, text=True, timeout=120)
            assert result.returncode == 0, result.stdout + result.stderr
            assert 'exact GM fallback conversion' in result.stdout
            report['songs'][song] = dict(renderer='sf2', reason='No source FM notes in compared prefix',
                                        native=result.stdout.strip())
            print(f'PASS {song}: no source FM notes, verified GM fallback')
            continue
        midi = args.output / (song + '.mid')
        result = subprocess.run([str(args.renderer), str(args.sf2), str(args.hog), song, str(midi)],
                                capture_output=True, text=True, timeout=120)
        assert result.returncode == 0, result.stdout + result.stderr
        assert f'sequence={sequence}' in result.stderr
        events, _ = read_midi(midi)
        # FM uses driver defaults; synthetic wheel messages would alter allocation
        # Short songs may already have restarted in the 20-second render
        # Compare the source prefix, then let the native test check full repeats
        assert source, song
        cutoff = min(20000, source[-1][0] * 1000 / rate)
        source = [e for e in source if e[0] * 1000 / rate < cutoff - 1e-6]
        actual = [e for e in events if e.ms < cutoff - 1e-6]
        assert len(actual) == len(source), (song, len(actual), len(source))
        for event, (tick, _track, _order, status, a, b) in zip(actual, source):
            payload = (a,) if status & 0xf0 in (0xc0, 0xd0) else (a, b)
            assert (event.status, event.data) == (status, payload), (song, event, status, payload)
            assert abs(event.ms - tick * 1000 / rate) < 0.001, (song, event, tick)
        report['songs'][song] = dict(selection, sequence=sequence, events=len(actual), compared_prefix_ms=cutoff,
                                    native=result.stdout.strip())
        print(f'PASS {song}: {len(actual)} source events, raw controllers, FM arrangement, restart PCM')
    if 'game08.hmp' not in members:
        report['passed'] = True
        (args.output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
        return
    # Explicit HMQ previews resolve the same SNG bank row as automatic selection
    direct = args.output / 'game08.hmq.mid'
    subprocess.run([str(args.renderer), str(args.sf2), str(args.hog), 'game08.hmq', str(direct)],
                   capture_output=True, text=True, check=True, timeout=60)
    assert direct.read_bytes() == (args.output / 'game08.hmp.mid').read_bytes()

    # A present but malformed HMQ must fall back to the original GM HMP
    broken = args.output / 'broken-hmq.hog'
    with broken.open('wb') as stream:
        stream.write(b'DHF')
        for name in ('descent.sng', 'game08.hmp', 'game08.hmq', 'rickmelo.bnk', 'rickdrum.bnk'):
            data = b'bad HMQ' if name.endswith('.hmq') else members[name]
            stream.write(name.encode('ascii').ljust(13, b'\0') + struct.pack('<I', len(data)) + data)
    outputs = []
    for preference in ('sf2', 'ymfm'):
        wav = args.output / f'broken-hmq-{preference}.wav'
        result = subprocess.run([str(args.renderer), '--render', str(args.sf2), str(broken),
                                 'game08.hmp', str(wav), preference],
                                capture_output=True, text=True, check=True, timeout=60)
        assert 'actual=sf2' in result.stdout
        outputs.append(wav.read_bytes())
    assert outputs[0] == outputs[1]
    report['checks'] = ['Explicit HMQ equals automatic selection', 'Malformed HMQ falls back to identical GM PCM']
    print('PASS explicit HMQ bank lookup and malformed-HMQ GM fallback')
    report['passed'] = True
    (args.output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')


if __name__ == '__main__':
    main()
