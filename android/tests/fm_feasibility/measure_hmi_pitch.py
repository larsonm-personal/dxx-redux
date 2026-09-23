"""Derive HMI's pitch-wheel lookup from a complete original-driver scale probe.

Only numeric OPL behavior is emitted, not original game assets or driver code.
The independent verifier checks the resulting renderer against other captures.
"""
import argparse
import hashlib
import json
from pathlib import Path

from opl_trace import note_states, read_dro
from verify_hmi_driver import active_snapshots


def measure(session, output):
    stimulus = json.loads((session / 'probe.json').read_text())
    capture = json.loads((session / 'unattended-capture.json').read_text())
    if stimulus['kind'] != 'pitch-scale' or not capture['completed']:
        raise ValueError('Expected a completed pitch-scale probe')
    song = (session / 'game' / stimulus['song'].upper()).read_bytes()
    if hashlib.sha256(song).hexdigest() != stimulus['sha256']:
        raise ValueError('Probe source hash changed')
    writes, metadata = read_dro(Path(capture['capture']))
    if metadata['sha256'] != capture['capture_sha256']:
        raise ValueError('Capture hash changed')
    notes, _ = note_states(writes)
    origin = notes[0]['ms'] - stimulus['notes'][0]['tick'] * 1000 / stimulus['rate']
    events, key = [], None
    for event in stimulus['events']:
        message = event['message']
        if message[0] == 0x92:
            key = message[1]
        elif message[0] == 0xe2:
            events.append((event['tick'], key, message[2]))
    if [(key, wheel) for _, key, wheel in events] != [(key, wheel) for key in range(48, 60) for wheel in range(128)]:
        raise ValueError('Expected all 128 positions for each of 12 semitones')
    # This probe holds each wheel value for two ticks; sample midway between them
    queries = [(tick + 1) * 1000 / stimulus['rate'] + origin for tick, _, _ in events]
    states = active_snapshots(writes, queries, 1)
    table = [[] for _ in range(12)]
    for (_, key, _), state in zip(events, states):
        if set(state) != {'0:0', '1:0'} or state['0:0'][:2] != state['1:0'][:2]:
            raise ValueError('Expected one active paired voice with identical pitch')
        freq, block = state['0:0'][:2]
        if block not in (3, 4) or not 0 < freq < 1024:
            raise ValueError('Unexpected pitch register range')
        table[key - 48].append(freq | ((block - 3) << 10))
    lines = [
        '// Measured from original DOS HMI, not copied from an implementation',
        '// Regenerate with measure_hmi_pitch.py --session <pitch-scale session> --output <this file>',
        '// Capture SHA-256: ' + metadata['sha256'],
        '// Probe SHA-256: ' + stimulus['sha256'],
        '// Row: semitone; column: wheel MSB; bits 0..9: F-number; bit 10: added octave',
        'static const uint16_t hmiBend[12][128] = {',
    ]
    for row in table:
        lines.append('    {')
        for offset in range(0, 128, 16):
            lines.append('        ' + ', '.join(str(value) for value in row[offset:offset + 16]) + ',')
        lines.append('    },')
    lines.append('};')
    output.write_text('\n'.join(lines) + '\n', encoding='utf8')
    print(f'Wrote {sum(map(len, table))} measured pitch states to {output}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--session', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    measure(args.session, args.output)
