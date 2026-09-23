"""Compare an experimental driver's actual key-on registers with DOS probes.

Requires locally captured original-game probes; no game assets are bundled.
Complete soundtrack coverage remains a separate gate.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import subprocess
import struct

from experiment import hog_members, read_bank, make_wopl, hmp_events, write_events, write_json
from opl_trace import OPERATORS, read_dro, note_states


def transitions(writes, bank, note_count):
    """First complete probe pass, including releases after its final note-on."""
    state, result, ons = [False] * 9, [], 0
    for time, reg, value in writes:
        if reg >> 8 != bank or not 0xb0 <= (reg & 255) <= 0xb8:
            continue
        voice, on = (reg & 255) - 0xb0, bool(value & 32)
        if state[voice] != on:
            result.append((time, voice, on))
            state[voice] = on
            ons += on
            if ons == note_count and not any(state):
                return result
    raise ValueError('Missing complete first-pass key-on/release sequence')


def compare_transitions(actual, expected, count):
    results = []
    for bank in (0, 1):
        a, e = transitions(actual, bank, count), transitions(expected, bank, count)
        same = len(a) == len(e) and all(x[1:] == y[1:] for x, y in zip(a, e))
        # Candidate timestamps are PCM frames at 48 kHz, DOS timestamps are ms
        delta = [abs((x[0] - a[0][0]) / 48 - (y[0] - e[0][0])) for x, y in zip(a, e)] if same else []
        results.append(dict(bank=bank, actual=len(a), expected=len(e), order_matches=same,
                            max_relative_timing_error_ms=round(max(delta), 3) if delta else None))
    return results


def active_snapshots(writes, queries, units_per_ms):
    registers, position, snapshots = [0] * 512, 0, []
    for query in queries:
        while position < len(writes) and writes[position][0] / units_per_ms <= query:
            _, reg, value = writes[position]
            registers[reg] = value
            position += 1
        active = {}
        for bank in (0, 256):
            for voice, operator in enumerate(OPERATORS):
                high = registers[bank + 0xb0 + voice]
                if not high & 32:
                    continue
                active[f'{bank >> 8}:{voice}'] = [registers[bank + 0xa0 + voice] | ((high & 3) << 8),
                    (high >> 2) & 7, registers[bank + 0xc0 + voice],
                    *[registers[bank + operator + offset + base] for offset in (0, 3)
                      for base in (0x20, 0x40, 0x60, 0x80, 0xe0)]]
        snapshots.append(active)
    return snapshots


def compare_active_events(actual, expected, events, rate):
    # Probe events are separated by known ticks. Inspect settled state halfway
    # to the next event, avoiding DOS's millisecond register-write serialization
    ticks = sorted({e[0] for e in events})
    first = next(e[0] for e in events if e[3] & 0xf0 == 0x90 and e[5]) * 1000 / rate
    relative = [(tick + min(6, ((ticks[i + 1] if i + 1 < len(ticks) else tick + 2) - tick) / 2))
                * 1000 / rate - first for i, tick in enumerate(ticks)]
    a_start = note_states(actual)[0][0]['ms'] / 48
    e_start = note_states(expected)[0][0]['ms']
    a = active_snapshots(actual, [t + a_start for t in relative], 48)
    e = active_snapshots(expected, [t + e_start for t in relative], 1)
    differences = [dict(tick=tick, actual=x, dos=y) for tick, x, y in zip(ticks, a, e) if x != y]
    return dict(snapshots=len(ticks), mismatches=len(differences), first_mismatches=differences[:4])


def render_trace(renderer, output, name, files, melodic, drums, rate, events, seconds):
    bank = output / (name + '.wopl')
    bank.write_bytes(make_wopl(read_bank(files[melodic.lower()]), read_bank(files[drums.lower()])))
    write_events(output, name, rate, events, seconds)
    result = subprocess.run([str(renderer), 'live-opl3', str(bank), str(output / (name + '.events')),
                             str(output / (name + '.wav')), str(seconds), str(output / (name + '.tsv'))],
                            capture_output=True, text=True, check=True)
    writes = [tuple(map(int, line.split())) for line in (output / (name + '.tsv')).read_text().splitlines()]
    return writes, json.loads(result.stdout)


def compare_notes(actual, expected):
    differences = []
    for index, (candidate, dos) in enumerate(zip(actual, expected)):
        for key in ('voice', 'chip', 'fnum', 'block', 'connection', 'operators'):
            if candidate[key] != dos[key]:
                differences.append(dict(note=index, field=key, actual=candidate[key], dos=dos[key]))
    return differences


def verify(renderer, cases, output, song_cases=(), production_renderer=None, sf2=None, repeat_probes=False):
    output.mkdir(parents=True, exist_ok=True)
    report = {}
    for name, session in cases:
        if not name.replace('-', '').isalnum():
            raise ValueError('Case name must contain letters, digits or hyphens')
        session = Path(session)
        stimulus = json.loads((session / 'probe.json').read_text())
        capture = json.loads((session / 'unattended-capture.json').read_text())
        if not capture['completed'] or capture['format'] != 'opl':
            raise ValueError('Expected a successfully finalized OPL capture')
        files = hog_members(session / 'game/DESCENT.HOG')
        song, melodic, drums = (session / 'game/DESCENT.SNG').read_text().splitlines()[0].split()
        data = (session / 'game' / (song[:-1] + 'q').upper()).read_bytes()
        if hashlib.sha256(data).hexdigest() != stimulus['sha256']:
            raise ValueError('Probe sequence changed since capture')
        seconds = stimulus['duration_seconds']
        rate, events, _ = hmp_events(data, seconds)
        count = len(stimulus['notes'])
        if production_renderer:
            # Private minimal HOG: original local banks plus generated stimuli
            fixture = output / (name + '.hog')
            members = {'descent.sng': f'{song} {melodic} {drums}\n'.encode('ascii'),
                       song.lower(): data, (song[:-1] + 'q').lower(): data,
                       melodic.lower(): files[melodic.lower()], drums.lower(): files[drums.lower()]}
            with fixture.open('wb') as stream:
                stream.write(b'DHF')
                for member, payload in members.items():
                    stream.write(member.encode('ascii').ljust(13, b'\0') + struct.pack('<I', len(payload)) + payload)
            if repeat_probes:
                events += [(t + round(seconds * rate), tr, order, status, a, b)
                           for t, tr, order, status, a, b in events]
                seconds *= 2
                count *= 2
            result = subprocess.run([str(production_renderer), '--render-repeat' if repeat_probes else '--render',
                                      str(sf2), str(fixture), song, str(output / (name + '.wav')), 'ymfm',
                                      str(math.ceil(seconds + 1)), str(output / (name + '.tsv'))],
                                     capture_output=True, text=True, check=True, timeout=120)
            if 'actual=ymfm' not in result.stdout:
                raise ValueError('Production probe fell back to SF2')
            writes = [tuple(map(int, line.split())) for line in (output / (name + '.tsv')).read_text().splitlines()]
            render = dict(production=result.stdout.strip(), passes=2 if repeat_probes else 1)
        else:
            writes, render = render_trace(renderer, output, name, files, melodic, drums, rate, events, seconds)
        actual, _ = note_states(writes)
        original, metadata = read_dro(Path(capture['capture']))
        if metadata['sha256'] != capture['capture_sha256']:
            raise ValueError('Original capture hash changed')
        expected, _ = note_states(original)
        # DOS loops after the probe ends; compare exactly one complete first pass
        expected = expected[:count * 2]
        differences = compare_notes(actual, expected)
        key_transitions = compare_transitions(writes, original, count)
        active_events = compare_active_events(writes, original, events, rate)
        passed = (len(expected) == len(actual) == count * 2 and not differences
                  and all(t['order_matches'] for t in key_transitions) and not active_events['mismatches'])
        report[name] = dict(passed=passed, probe_sha256=stimulus['sha256'], capture=metadata,
                            actual_notes=len(actual), expected_notes=len(expected),
                            mismatches=len(differences), first_mismatches=differences[:12],
                            transitions=key_transitions, active_events=active_events, render=render)
        print(f"{name}: {len(actual)} paired key-ons, {len(differences)} key-on / {active_events['mismatches']} settled-state mismatches, passed={passed}")
    for name, hog, song, capture in song_cases:
        if not name.replace('-', '').isalnum() or name in report:
            raise ValueError('Expected a unique alphanumeric/hyphen case name')
        files = hog_members(Path(hog))
        song = song.lower()
        bank_song = song[:-1] + 'p' if song.endswith('.hmq') else song
        _, melodic, drums = next(row.split() for row in files['descent.sng'].decode('ascii').splitlines()
                                  if row.split() and row.split()[0].lower() == bank_song)
        original, metadata = read_dro(Path(capture))
        expected, _ = note_states(original)
        counts = [sum(n['chip'] == bank for n in expected) for bank in (0, 1)]
        if not counts[0] or counts[0] != counts[1]:
            raise ValueError('Expected a paired-voice song capture beginning at the first note')
        # Compare the entire captured prefix, without motif alignment or skipped notes
        rate, events, _ = hmp_events(files[song], metadata['duration_ms'] / 1000 + 2)
        used, count = [], 0
        for event in events:
            used.append(event)
            count += event[3] & 0xf0 == 0x90 and event[5] > 0
            if count == counts[0]:
                break
        if count != counts[0]:
            raise ValueError('Capture extends beyond the available source first pass')
        if production_renderer:
            result = subprocess.run([str(production_renderer), '--render', str(sf2), str(hog), bank_song,
                                      str(output / (name + '.wav')), 'ymfm',
                                      str(math.ceil(metadata['duration_ms'] / 1000 + 2)), str(output / (name + '.tsv'))],
                                     capture_output=True, text=True, check=True, timeout=120)
            if 'actual=ymfm' not in result.stdout:
                raise ValueError('Production renderer fell back to SF2')
            writes = [tuple(map(int, line.split())) for line in (output / (name + '.tsv')).read_text().splitlines()]
            render = dict(production=result.stdout.strip())
        else:
            writes, render = render_trace(renderer, output, name, files, melodic, drums, rate, used,
                                          used[-1][0] / rate + 0.1)
        actual, _ = note_states(writes)
        extra_notes = max(0, len(actual) - len(expected))
        if production_renderer:
            actual = actual[:len(expected)]
        differences = compare_notes(actual, expected)
        passed = len(actual) == len(expected) and not differences
        report[name] = dict(passed=passed, song=song, source_sha256=hashlib.sha256(files[song]).hexdigest(),
                            capture=metadata, actual_notes=len(actual), expected_notes=len(expected),
                            extra_rendered_notes=extra_notes,
                            mismatches=len(differences), first_mismatches=differences[:12], render=render,
                            scope='All ordered key-on states in the captured first-pass prefix; no motif matching')
        print(f'{name}: {len(actual)} paired key-ons, {len(differences)} register mismatches, passed={passed}')
    write_json(output / 'controlled-register-report.json', report)
    if not all(case['passed'] for case in report.values()):
        raise AssertionError('Original-driver probe comparison failed')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--renderer', type=Path, required=True)
    parser.add_argument('--case', nargs=2, action='append', default=[], metavar=('NAME', 'SESSION'))
    parser.add_argument('--song-case', nargs=4, action='append', default=[], metavar=('NAME', 'HOG', 'SONG', 'DRO'))
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--production-renderer', type=Path)
    parser.add_argument('--repeat-probes', action='store_true')
    parser.add_argument('--sf2', type=Path, default=Path('android/app/src/main/assets/gm.sf2'))
    args = parser.parse_args()
    if not args.case and not args.song_case:
        parser.error('Supply at least one --case or --song-case')
    if args.repeat_probes and not args.production_renderer:
        parser.error('--repeat-probes requires --production-renderer')
    verify(args.renderer, args.case, args.output, args.song_case, args.production_renderer, args.sf2, args.repeat_probes)
