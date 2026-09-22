"""Independent Standard MIDI File comparison, with explicit capture windows.

Compares ordered channel messages, timing, and effective volume at every note.
Running status and track layout are normalized; tempo maps are evaluated.
No sequence alignment or automatic clock stretching can hide dropped events.
"""
import argparse
from dataclasses import dataclass
import json
from pathlib import Path
import struct


@dataclass(frozen=True)
class Event:
    ms: float
    status: int
    data: tuple


def vlq(data, offset):
    value = 0
    for _ in range(4):
        if offset >= len(data):
            raise ValueError('Truncated MIDI variable-length number')
        byte = data[offset]
        offset += 1
        value = (value << 7) | (byte & 127)
        if not byte & 128:
            return value, offset
    raise ValueError('Oversized MIDI variable-length number')


def read_midi(path):
    data = Path(path).read_bytes()
    if len(data) < 14 or data[:4] != b'MThd':
        raise ValueError('Not an SMF')
    size, fmt, tracks, division = struct.unpack_from('>IHHH', data, 4)
    if size < 6 or fmt not in (0, 1) or not tracks or not 0 < division < 32768:
        raise ValueError('Unsupported SMF header (requires format 0/1, PPQN timing)')
    pos = 8 + size
    raw = []
    for track in range(tracks):
        if data[pos:pos + 4] != b'MTrk' or pos + 8 > len(data):
            raise ValueError('Missing SMF track')
        size = struct.unpack_from('>I', data, pos + 4)[0]
        chunk = data[pos + 8:pos + 8 + size]
        pos += 8 + size
        if len(chunk) != size or not size:
            raise ValueError('Unfinalized or truncated SMF track')
        offset = tick = running = 0
        ended = False
        while offset < size:
            delta, offset = vlq(chunk, offset)
            tick += delta
            if offset >= size:
                raise ValueError('Missing event')
            status = chunk[offset]
            if status & 128:
                offset += 1
                if status < 240:
                    running = status
            else:
                if not running:
                    raise ValueError('Invalid running status')
                status = running
            if status == 255:
                if offset >= size:
                    raise ValueError('Missing meta type')
                subtype = chunk[offset]
                length, offset = vlq(chunk, offset + 1)
                payload = (subtype, *chunk[offset:offset + length])
                if len(payload) != length + 1:
                    raise ValueError('Truncated meta event')
                offset += length
                if subtype == 47:
                    if length or offset != size:
                        raise ValueError('Invalid end of track')
                    ended = True
            elif status in (240, 247):
                running = 0
                length, offset = vlq(chunk, offset)
                payload = tuple(chunk[offset:offset + length])
                if len(payload) != length:
                    raise ValueError('Truncated SysEx')
                offset += length
            elif 128 <= status < 240:
                length = 1 if status & 240 in (192, 208) else 2
                payload = tuple(chunk[offset:offset + length])
                if len(payload) != length or any(x > 127 for x in payload):
                    raise ValueError('Invalid channel message data')
                offset += length
            else:
                raise ValueError(f'Unsupported status {status:02x}')
            raw.append((tick, track, len(raw), status, payload))
        if not ended:
            raise ValueError('Missing end of track')
    if pos != len(data):
        raise ValueError('Unexpected trailing SMF data')
    tempo, last_tick, ms = 500000, 0, 0.0
    events = []
    for tick, _, _, status, payload in sorted(raw):
        ms += (tick - last_tick) * tempo / division / 1000
        last_tick = tick
        if status == 255:
            if payload[0] == 81:
                if len(payload) != 4:
                    raise ValueError('Invalid tempo')
                tempo = int.from_bytes(bytes(payload[1:]), 'big')
                if not tempo:
                    raise ValueError('Zero tempo')
        else:
            events.append(Event(ms, status, payload))
    return events, ms


def window(events, start, duration, strip_initial_volume=False):
    selected = []
    initial = []
    for e in events:
        if start <= e.ms < start + duration:
            relative = Event(e.ms - start, e.status, e.data)
            if strip_initial_volume and abs(relative.ms) < 0.001 and e.status & 240 == 176 and e.data == (7, 0):
                initial.append(relative)
            else:
                selected.append(relative)
    return selected, initial


def note_volumes(events, start, duration):
    volume = [100] * 16
    expression = [127] * 16
    result = [[] for _ in range(16)]
    for e in events:
        ch, kind = e.status & 15, e.status & 240
        if kind == 176:
            if e.data[0] == 7:
                volume[ch] = e.data[1]
            elif e.data[0] == 11:
                expression[ch] = e.data[1]
            elif e.data[0] == 121:
                expression[ch] = 127
        elif kind == 144 and e.data[1] and start <= e.ms < start + duration:
            result[ch].append((e.data[0], e.data[1], volume[ch], expression[ch]))
    return result


def note_states(events, start, duration):
    states = [dict(program=0, bank_msb=0, bank_lsb=0, volume=100, expression=127,
                   pan=64, pitch_bend=8192, sustain=0) for _ in range(16)]
    controls = {0: 'bank_msb', 32: 'bank_lsb', 7: 'volume', 11: 'expression', 10: 'pan', 64: 'sustain'}
    result = [[] for _ in range(16)]
    for e in events:
        if e.status >= 240:
            continue
        ch, kind = e.status & 15, e.status & 240
        state = states[ch]
        if kind == 176:
            if e.data[0] in controls:
                state[controls[e.data[0]]] = e.data[1]
            elif e.data[0] == 121:
                state.update(expression=127, sustain=0, pitch_bend=8192)
        elif kind == 192:
            state['program'] = e.data[0]
        elif kind == 224:
            state['pitch_bend'] = e.data[0] | (e.data[1] << 7)
        elif kind == 144 and e.data[1] and start <= e.ms < start + duration:
            result[ch].append(dict(note=list(e.data), **state))
    return result


def compare(reference, candidate, reference_start=0, candidate_start=0,
            duration=None, tolerance=10, candidate_initial_volume=False,
            shared_initial_state=False):
    ref, ref_end = read_midi(reference)
    got, got_end = read_midi(candidate)
    if duration is None:
        duration = max(ref_end - reference_start, got_end - candidate_start) + 0.001
    if duration <= 0 or reference_start < 0 or candidate_start < 0 or tolerance < 0:
        raise ValueError('Invalid comparison window/tolerance')
    if ref_end + 0.01 < reference_start + duration or got_end + 0.01 < candidate_start + duration:
        raise ValueError('Comparison window extends beyond a MIDI file; refusing a partial comparison')
    a, _ = window(ref, reference_start, duration)
    b, init = window(got, candidate_start, duration, candidate_initial_volume)
    volumes_a = note_volumes(ref, reference_start, duration)
    volumes_b = note_volumes(got, candidate_start, duration)
    states_a = note_states(ref, reference_start, duration)
    context = []
    if shared_initial_state:
        # Standalone conversion has no preceding song. Supply only the state
        # retained by the production HMP transition helper, not volume or notes.
        context = [Event(-1, e.status, e.data) for e in ref if e.ms < reference_start and
                   (e.status & 240 == 192 or (e.status & 240 == 176 and e.data[0] in (0, 32, 10, 42)))]
    states_b = note_states(context + got, candidate_start, duration)
    channels = []
    passed = True
    for ch in range(16):
        x = [e for e in a if e.status < 240 and e.status & 15 == ch]
        y = [e for e in b if e.status < 240 and e.status & 15 == ch]
        mismatch = next((i for i, (u, v) in enumerate(zip(x, y)) if (u.status, u.data) != (v.status, v.data)), None)
        errors = [abs(u.ms - v.ms) for u, v in zip(x, y) if (u.status, u.data) == (v.status, v.data)]
        max_error = max(errors, default=0)
        state_ok = volumes_a[ch] == volumes_b[ch]
        all_state_ok = states_a[ch] == states_b[ch]
        ok = len(x) == len(y) and mismatch is None and max_error <= tolerance and all_state_ok
        entry = {'channel': ch + 1, 'reference_events': len(x), 'candidate_events': len(y),
                 'first_message_mismatch': mismatch, 'max_time_error_ms': round(max_error, 6),
                 'note_volume_state_matches': state_ok, 'note_state_matches': all_state_ok, 'passed': ok}
        if not all_state_ok:
            state_index = next((i for i, (u, v) in enumerate(zip(states_a[ch], states_b[ch])) if u != v), None)
            if state_index is not None:
                entry['first_note_state_mismatch'] = {'index': state_index, 'reference': states_a[ch][state_index], 'candidate': states_b[ch][state_index]}
        if mismatch is not None:
            entry['reference_message'] = [x[mismatch].status, *x[mismatch].data]
            entry['candidate_message'] = [y[mismatch].status, *y[mismatch].data]
        channels.append(entry)
        passed &= ok
    sysex_a = [e for e in a if e.status >= 240]
    sysex_b = [e for e in b if e.status >= 240]
    sysex_ok = len(sysex_a) == len(sysex_b) and all(u.status == v.status and u.data == v.data and abs(u.ms - v.ms) <= tolerance for u, v in zip(sysex_a, sysex_b))
    return {'passed': bool(passed and sysex_ok), 'reference_start_ms': reference_start,
            'candidate_start_ms': candidate_start, 'duration_ms': duration,
            'tolerance_ms': tolerance, 'candidate_initial_cc7_messages': len(init),
            'candidate_retained_state_from_reference_prefix': shared_initial_state,
            'sysex_matches': sysex_ok, 'channels': channels}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('reference', type=Path)
    parser.add_argument('candidate', type=Path, nargs='?')
    parser.add_argument('--dump-events', type=Path, help='Decode reference MIDI to JSON instead of comparing')
    parser.add_argument('--reference-start-ms', type=float, default=0)
    parser.add_argument('--candidate-start-ms', type=float, default=0)
    parser.add_argument('--duration-ms', type=float)
    parser.add_argument('--tolerance-ms', type=float, default=10)
    parser.add_argument('--candidate-initial-volume', action='store_true', help='Separate synthetic time-zero CC7=0 from message diff; still check their effect on every note')
    parser.add_argument('--shared-initial-state', action='store_true', help='Prime candidate program/bank/pan from the reference prefix, matching the HMP song-transition helper; otherwise compare cold-start state')
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    if args.dump_events:
        events, duration = read_midi(args.reference)
        args.dump_events.write_text(json.dumps({'duration_ms': duration, 'events': [dict(ms=round(e.ms, 6), status=e.status, data=e.data) for e in events]}, indent=2) + '\n', encoding='utf8')
        return 0
    if args.candidate is None:
        parser.error('candidate MIDI is required unless --dump-events is used')
    result = compare(args.reference, args.candidate, args.reference_start_ms,
                     args.candidate_start_ms, args.duration_ms, args.tolerance_ms,
                     args.candidate_initial_volume, args.shared_initial_state)
    output = json.dumps(result, indent=2) + '\n'
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(output, encoding='utf8')
    print(output, end='')
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
