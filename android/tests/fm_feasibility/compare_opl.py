"""Compare original DOS OPL note state with HMP/BNK using ordered note alignment.

This measures driver behavior, not waveform fidelity. Matching excludes level
registers deliberately, so volume differences remain visible in the report.
"""

import argparse
import collections
import difflib
import math
from pathlib import Path
import statistics

from experiment import hmp_events, hog_members, read_bank, write_json
from opl_trace import note_states, read_dro


def signature(operators, connection, pitch):
    return (pitch, connection & 15, *(x for op in operators for i, x in enumerate(op) if i != 1))


def source_notes(hog, song, seconds):
    files = hog_members(hog)
    bank_song = song[:-1] + 'p' if song.endswith('.hmq') else song
    row = next(row.split() for row in files['descent.sng'].decode('ascii').splitlines()
               if row.split() and row.split()[0].lower() == bank_song)
    melodic, drums = (read_bank(files[name.lower()]) for name in row[1:3])
    rate, events, arrangement = hmp_events(files[song], seconds)
    program, volume, pan = [0] * 16, [127] * 16, [64] * 16
    notes = []
    for tick, _track, _order, status, a, b in events:
        channel, kind = status & 15, status & 0xf0
        if kind == 0xc0:
            program[channel] = a
        elif kind == 0xb0 and a == 7:
            volume[channel] = b
        elif kind == 0xb0 and a == 10:
            pan[channel] = b
        elif kind == 0x90 and b:
            patch = (drums if channel == 9 else melodic)[a if channel == 9 else program[channel]]
            # Captures of all five rhythm-flagged D1 entries show ordinary
            # paired two-operator playback; HMI does not enable hardware rhythm
            pitch = patch['fixed_note'] if channel == 9 else a
            notes.append({'ms': tick * 1000 / rate, 'channel': channel, 'note': a,
                          'pitch': pitch, 'velocity': b, 'volume': volume[channel],
                          'pan': pan[channel], 'program': program[channel], 'patch': patch,
                          'signature': signature(patch['operators'], patch['connection'], pitch)})
    return notes, arrangement


def compare(hog, song, capture, output, seconds=120):
    source, arrangement = source_notes(hog, song, seconds)
    writes, capture_report = read_dro(capture)
    notes, _offs = note_states(writes)
    banks = [[n for n in notes if n['chip'] == bank] for bank in (0, 1)]
    # Each bank has its own key-on, so matching bank 0 avoids double counting
    keys = [signature(n['operators'], n['connection'], round(69 + 12 * math.log2(n['frequency_hz'] / 440)))
            for n in banks[0]]
    matcher = difflib.SequenceMatcher(None, [n['signature'] for n in source], keys, autojunk=False)
    matches = []
    for block in matcher.get_matching_blocks():
        for i in range(block.size):
            s, d = source[block.a + i], banks[0][block.b + i]
            matches.append({'source_index': block.a + i, 'dos_index': block.b + i,
                            'source_ms': round(s['ms'], 3), 'dos_ms': d['ms'],
                            'offset_ms': round(d['ms'] - s['ms'], 3),
                            'channel': s['channel'], 'note': s['note'], 'pitch': s['pitch'],
                            'velocity': s['velocity'], 'volume': s['volume'], 'pan': s['pan'],
                            'patch_name': s['patch']['name'], 'fnum': d['fnum'], 'block': d['block'],
                            'base_levels': [op[1] & 63 for op in s['patch']['operators']],
                            'dos_levels': [op[1] & 63 for op in d['operators']],
                            'ksl_matches': all((a[1] & 192) == (b[1] & 192)
                                               for a, b in zip(s['patch']['operators'], d['operators']))})
    if not matches:
        raise ValueError('No matching note sequence')
    pairs = []
    for left, right in zip(*banks):
        pairs.append({'same_pitch': (left['fnum'], left['block']) == (right['fnum'], right['block']),
                      'same_patch': signature(left['operators'], left['connection'], 0) ==
                                    signature(right['operators'], right['connection'], 0),
                      'routing': (left['connection'] & 0xf0, right['connection'] & 0xf0),
                      'delay_ms': right['ms'] - left['ms'],
                      'carrier_difference': (left['operators'][1][1] & 63) - (right['operators'][1][1] & 63)})
    bins = []
    for start in range(0, int(seconds * 1000), 10000):
        offsets = [m['offset_ms'] for m in matches if start <= m['source_ms'] < start + 10000]
        if offsets:
            bins.append({'source_start_ms': start, 'notes': len(offsets),
                         'median_offset_ms': round(statistics.median(offsets), 3),
                         'min_offset_ms': min(offsets), 'max_offset_ms': max(offsets)})
    pitches = collections.Counter((m['pitch'], m['fnum'], m['block']) for m in matches)
    report = {'capture': capture_report, 'arrangement': arrangement,
              'matching_method': 'Ordered exact pitch/patch signature alignment, excluding TL/KSL; no time fitting',
              'source_notes': len(source), 'dos_notes_per_bank': [len(b) for b in banks],
              'matched_notes': len(matches), 'first_match': matches[0], 'last_match': matches[-1],
              'ksl_mismatches': sum(not m['ksl_matches'] for m in matches),
              'paired_pitch_mismatches': sum(not p['same_pitch'] for p in pairs),
              'paired_patch_mismatches': sum(not p['same_patch'] for p in pairs),
              'paired_max_delay_ms': max(abs(p['delay_ms']) for p in pairs),
              'pair_routing_counts': dict(collections.Counter(str(p['routing']) for p in pairs)),
              'pairs_with_different_carrier_level': sum(bool(p['carrier_difference']) for p in pairs),
              'timing_bins': bins,
              'pitch_observations': [{'midi_pitch': p, 'fnum': f, 'block': b, 'count': n}
                                     for (p, f, b), n in sorted(pitches.items())],
              'limits': ['Repeated motifs can make sequence alignment ambiguous; inspect offsets and matched context',
                         'Only key-on state is compared; sustain changes, note-off and voice stealing need separate checks',
                         'Source is limited to the first pass; unmatched tail can extend beyond the source window']}
    output.mkdir(parents=True, exist_ok=True)
    write_json(output / 'driver-comparison.json', report)
    write_json(output / 'matched-notes.json', matches)
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--hog', type=Path, required=True)
    parser.add_argument('--song', default='game07.hmp')
    parser.add_argument('--capture', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--seconds', type=float, default=120)
    args = parser.parse_args()
    result = compare(args.hog, args.song, args.capture, args.output, args.seconds)
    print(f"Matched {result['matched_notes']} source notes; report: {args.output / 'driver-comparison.json'}")
