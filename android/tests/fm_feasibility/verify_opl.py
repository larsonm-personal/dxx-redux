"""Real Level 7 regression: DOS alignment, neutral pitch fix, and decoder rejection."""

import argparse
import json
from pathlib import Path
import subprocess
import tempfile

from compare_opl import compare
from experiment import hmp_events, hog_members, make_wopl, read_bank, write_events, write_json
from opl_trace import note_states, read_dro


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--hog', type=Path, required=True)
    parser.add_argument('--capture', type=Path, required=True)
    parser.add_argument('--probe-fixed', type=Path, required=True)
    parser.add_argument('--probe-pitch', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output
    report = compare(args.hog, 'game07.hmp', args.capture, output)
    assert report['capture']['sha256'] == '8bcae87ea5d113d1c79bdd3c7d30e0ff1860b1268bbb7d2e3053ebe3c794f7d8', 'Expected recorded Level 7 fixture'
    assert report['matched_notes'] == 1971 and report['source_notes'] == 2040
    assert report['first_match']['source_index'] == 69 and report['last_match']['source_index'] == 2039
    assert not report['ksl_mismatches'] and not report['paired_pitch_mismatches'] and not report['paired_patch_mismatches']
    assert report['pair_routing_counts'] == {'(32, 16)': 2202}
    files = hog_members(args.hog)
    row = next(r.split() for r in files['descent.sng'].decode('ascii').splitlines() if r.startswith('game07.hmp'))
    (output / 'descent.wopl').write_bytes(make_wopl(*(read_bank(files[name.lower()]) for name in row[1:3])))
    rate, events, _arrangement = hmp_events(files['game07.hmp'], 120)
    write_events(output, 'full-fm', rate, events, 120)
    observations = report['pitch_observations']
    calibration = [(0, 0, 0, 0xc0, 0, 0)]
    for i, item in enumerate(observations):
        calibration.extend([(i * 120, 0, i * 2 + 1, 0x90, item['midi_pitch'], 100),
                            (i * 120 + 60, 0, i * 2 + 2, 0x80, item['midi_pitch'], 0)])
    write_events(output, 'pitch-calibration', 120, calibration, len(observations))
    checks = {}
    for name, probe in [('baseline', args.probe_fixed), ('hmi-pitch', args.probe_pitch)]:
        trace = output / f'{name}.tsv'
        subprocess.run([str(probe), 'live-opl2', str(output / 'descent.wopl'), str(output / 'full-fm.events'),
                        str(output / f'{name}.wav'), '120', str(trace)], check=True)
        writes = [tuple(map(int, line.split())) for line in trace.read_text().splitlines()]
        notes, _ = note_states(writes)
        full_count = len(notes)
        calibration_trace = output / f'{name}-calibration.tsv'
        subprocess.run([str(probe), 'live-opl2', str(output / 'descent.wopl'), str(output / 'pitch-calibration.events'),
                        str(output / f'{name}-calibration.wav'), str(len(observations)), str(calibration_trace)], check=True)
        calibrated, _ = note_states([tuple(map(int, line.split())) for line in calibration_trace.read_text().splitlines()])
        assert len(calibrated) == len(observations), 'Isolated calibration note lost'
        exact = sum((n['fnum'], n['block']) == (o['fnum'], o['block']) for n, o in zip(calibrated, observations))
        checks[name] = {'full_song_key_ons': full_count, 'source_note_ons': len([e for e in events if e[3] & 0xf0 == 0x90 and e[5]]),
                        'isolated_dos_pitch_matches': exact, 'compared_pitches': len(observations)}
    assert checks['hmi-pitch']['isolated_dos_pitch_matches'] == len(observations), 'Measured HMI pitch table differs from DOS'
    assert checks['baseline']['isolated_dos_pitch_matches'] < len(observations), 'Negative control unexpectedly matches'
    assert checks['hmi-pitch']['full_song_key_ons'] == checks['baseline']['full_song_key_ons'], 'Pitch experiment changed key-on count'
    # A damaged or unfinalized capture must not silently become the oracle
    data = args.capture.read_bytes()
    with tempfile.TemporaryDirectory(dir=output) as scratch:
        for label, bad in [('truncated', data[:-1]), ('unfinalized', data[:12] + bytes(8) + data[20:])]:
            path = Path(scratch) / (label + '.dro')
            path.write_bytes(bad)
            try:
                read_dro(path)
            except ValueError:
                pass
            else:
                raise AssertionError(f'Decoder accepted {label} capture')
    write_json(output / 'verification.json', {'passed': True, 'pitch_comparison': checks,
                                             'scope': 'This fixture and isolated neutral pitches only; full-song key-on deficit remains unresolved'})
    print(json.dumps(checks, indent=2))
    print('PASS: DOS sequence/patch pairing, measured pitch, baseline negative control, malformed captures rejected')


if __name__ == '__main__':
    main()
