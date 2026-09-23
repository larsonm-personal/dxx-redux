"""Measure HMI stereo levels from controlled DOS probes and held-out songs.

This is a black-box measurement of original driver output. It does not read
third-party MIDI driver implementation code or copy instrument bank records.
"""
import argparse
import collections
import json
from pathlib import Path

from compare_opl import compare
from experiment import write_json
from opl_trace import read_dro, note_states


def volume_index(velocity, volume, pan, side):
    # Full song/master volume is 127 at each separate HMI scaling stage
    level = ((volume * 127) >> 7) * 127 >> 7
    level = level * velocity >> 7
    level = level * min(64, pan if side else 127 - pan) >> 6
    return level >> 1


def banks(path):
    writes, metadata = read_dro(path)
    notes, _ = note_states(writes)
    sides = [[note for note in notes if note['chip'] == side] for side in (0, 1)]
    if len(sides[0]) != len(sides[1]):
        raise ValueError('Capture has unequal left/right key-on counts')
    for left, right in zip(*sides):
        if ((left['fnum'], left['block'], left['voice']) !=
                (right['fnum'], right['block'], right['voice']) or
                left['connection'] & 0xf0 != 0x20 or right['connection'] & 0xf0 != 0x10):
            raise ValueError('Capture does not use paired left/right voices')
    return sides, metadata


def measure(probe, capture, references, output):
    output.mkdir(parents=True, exist_ok=True)
    stimulus = json.loads(probe.read_text())
    sides, metadata = banks(capture)
    rows = stimulus['notes']
    if len(sides[0]) < len(rows):
        raise ValueError('The controlled probe was not recorded in full')
    table = [None] * 64
    for i, row in enumerate(rows):
        if row['section'] != 'velocity':
            continue
        for side in (0, 1):
            index = volume_index(row['velocity'], row['volume'], row['pan'], side)
            level = sides[side][i]['operators'][1][1] & 63
            if table[index] not in (None, level):
                raise ValueError('Inconsistent velocity sweep')
            table[index] = level
    if any(value is None for value in table[:63]):
        raise ValueError('Velocity sweep did not cover every reachable table index')
    observations = collections.Counter()

    def check(row, side, base, actual):
        index = volume_index(row['velocity'], row['volume'], row['pan'], side)
        attenuation = table[index]
        if attenuation is None:
            raise ValueError('Observation uses an unmeasured table index')
        predicted = attenuation + (63 - attenuation) * base // 63
        if predicted != actual:
            raise ValueError(f'Level mismatch: {row}, side={side}, base={base}, '
                             f'predicted={predicted}, actual={actual}')
        observations[(row['velocity'], row['volume'], row['pan'], side, base, actual)] += 1

    for i, row in enumerate(rows):
        for side in (0, 1):
            check(row, side, 0, sides[side][i]['operators'][1][1] & 63)
    songs = []
    for number, (hog, song, dro) in enumerate(references):
        folder = output / f'song-{number}'
        _, capture_info = read_dro(Path(dro))
        compare(Path(hog), song, Path(dro), folder, capture_info['duration_ms'] / 1000 + 1)
        matched = json.loads((folder / 'matched-notes.json').read_text())
        notes, capture_meta = banks(Path(dro))
        checked = 0
        for row in matched:
            for side in (0, 1):
                note = notes[side][row['dos_index']]
                for op in (0, 1):
                    actual = note['operators'][op][1] & 63
                    base = row['base_levels'][op]
                    if op == 1:
                        check(row, side, base, actual)
                    elif actual != base:
                        raise ValueError('FM modulator unexpectedly volume-scaled')
                    checked += 1
        songs.append(dict(song=song, capture=capture_meta, matched_notes=len(matched),
                          captured_notes=len(notes[0]), checked_operator_levels=checked,
                          first_source_index=matched[0]['source_index'],
                          last_source_index=matched[-1]['source_index']))
    report = dict(probe_sha256=stimulus['sha256'], probe_capture=metadata,
                  probe_notes_checked=len(rows), attenuation_table=table,
                  unmeasured_indices=[i for i, value in enumerate(table) if value is None],
                  songs=songs, checked_scaled_levels=sum(observations.values()),
                  observations=[dict(velocity=v, volume=c, pan=p, side=s, base_level=b,
                                     dos_level=a, count=count)
                                for (v, c, p, s, b, a), count in sorted(observations.items())],
                  limits=['Full song/master volume only; each HMI stage is fixed at 127',
                          'Table index 63 is unreachable at these two full-volume stages',
                          'Pitch bends, voice stealing and rhythm mode are separate measurements'])
    write_json(output / 'volume-measurements.json', report)
    print(json.dumps({key: value for key, value in report.items() if key != 'observations'}, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--capture', type=Path, required=True)
    parser.add_argument('--reference', nargs=3, action='append', default=[],
                        metavar=('HOG', 'SONG', 'DRO'))
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    measure(args.probe, args.capture, args.reference, args.output)
