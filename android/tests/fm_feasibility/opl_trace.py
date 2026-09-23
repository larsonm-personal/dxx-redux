"""Decode DOSBox DRO v2 into timed register writes and inspect key-on state.

Format documentation: https://moddingwiki.shikadi.net/wiki/DRO_Format
Independent decoder; no DOSBox/AdPlug implementation code is included.
"""
import argparse
import collections
import hashlib
import json
from pathlib import Path
import struct


OPERATORS = (0, 1, 2, 8, 9, 10, 16, 17, 18)


def read_dro(path):
    data = Path(path).read_bytes()
    if len(data) < 26 or data[:8] != b'DBRAWOPL':
        raise ValueError('Not a DRO capture')
    major, minor, pairs, duration, hardware, fmt, compression, short, long, size = struct.unpack_from('<HHIIBBBBBB', data, 8)
    if (major, minor) != (2, 0) or hardware not in (0, 1, 2) or fmt or compression:
        raise ValueError('Expected uncompressed interleaved DRO v2')
    if not 1 <= size <= 128 or len(data) != 26 + size + pairs * 2:
        raise ValueError('Unfinalized/truncated DRO or invalid code table')
    if short == long or short < size or long < size:
        raise ValueError('Invalid delay opcodes')
    table = data[26:26 + size]
    writes, time = [], 0
    for pos in range(26 + size, len(data), 2):
        command, value = data[pos:pos + 2]
        if command == short:
            time += value + 1
        elif command == long:
            time += (value + 1) * 256
        else:
            index = command & 127
            if index >= size:
                raise ValueError('Register index outside code table')
            writes.append((time, table[index] | (256 if command & 128 else 0), value))
    if time != duration:
        raise ValueError(f'DRO duration mismatch: header={duration}, delays={time}')
    return writes, {'sha256': hashlib.sha256(data).hexdigest(), 'duration_ms': duration,
                    'hardware': ('opl2', 'dual-opl2', 'opl3')[hardware], 'pairs': pairs,
                    'register_writes': len(writes)}


def note_states(writes):
    registers = [0] * 512
    notes, offs = [], []
    for time, reg, value in writes:
        old = registers[reg]
        registers[reg] = value
        low, bank = reg & 255, reg & 256
        if 0xb0 <= low <= 0xb8:
            channel = low - 0xb0
            if not old & 32 and value & 32:
                op = OPERATORS[channel] + bank
                number = registers[bank + 0xa0 + channel] | ((value & 3) << 8)
                block = (value >> 2) & 7
                notes.append({'ms': time, 'chip': bank >> 8, 'voice': channel,
                              'fnum': number, 'block': block,
                              'frequency_hz': round(number * 49716 / (2 ** (20 - block)), 4),
                              'connection': registers[bank + 0xc0 + channel],
                              'operators': [[registers[op + offset + base] for base in (0x20, 0x40, 0x60, 0x80, 0xe0)]
                                            for offset in (0, 3)]})
            elif old & 32 and not value & 32:
                offs.append({'ms': time, 'chip': bank >> 8, 'voice': channel})
    return notes, offs


def export(path, output):
    writes, report = read_dro(path)
    notes, offs = note_states(writes)
    output.mkdir(parents=True, exist_ok=True)
    (output / 'registers.tsv').write_text(''.join(f'{round(ms * 49716 / 1000)} {reg} {value}\n'
                                                for ms, reg, value in writes), encoding='ascii')
    report['key_ons_per_chip'] = dict(sorted(collections.Counter(n['chip'] for n in notes).items()))
    report['key_offs'] = len(offs)
    report['opl3_enable_writes'] = [(ms, value) for ms, reg, value in writes if reg == 0x105]
    report['opl3_enabled'] = any(value & 1 for ms, value in report['opl3_enable_writes'])
    report['hardware_note'] = 'DOSBox 0.74 labels two-bank captures dual-opl2 unless four-operator mode is enabled; inspect register 0x105 as well'
    (output / 'notes.json').write_text(json.dumps(notes, indent=2) + '\n')
    (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    print(json.dumps(export(args.capture, args.output), indent=2))
