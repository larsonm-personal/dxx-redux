"""Write controlled HMP/HMQ stimuli into a prepared private DOS capture session.

Uses the original game's banks and driver, with generated MIDI events only.
The manifest records every stimulus; no captured instrument assets are published.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

from experiment import hog_members, read_bank


def delta(value):
    result = bytearray()
    while value >= 128:
        result.append(value & 127)
        value >>= 7
    result.append(value | 128)
    return result


def prepare(session, kind, declared_channel=None):
    game = session / 'game'
    if not (session / 'provenance.json').is_file() or not (session / 'capture.conf').is_file():
        raise ValueError('Expected a prepared private capture session')
    files = hog_members(game / 'DESCENT.HOG')
    song = 'game07.hmp'
    rows = []
    events = []
    tick = 120
    channel = 9 if kind.startswith('rhythm-') else 2

    def emit(status, a, b=None):
        events.append((tick, bytes([status | channel, a] + ([] if b is None else [b]))))

    def note(section, velocity=127, volume=127, pan=64, key=60, program=38):
        nonlocal tick
        emit(0xc0, program)
        emit(0xb0, 7, volume)
        emit(0xb0, 10, pan)
        emit(0x90, key, velocity)
        rows.append(dict(section=section, tick=tick, velocity=velocity, volume=volume,
                         pan=pan, key=key, program=program))
        tick += 9
        emit(0x80, key, 0)
        tick += 9

    if kind == 'volume':
        for v in range(1, 128):
            note('velocity', velocity=v)
        for v in range(128):
            note('volume', volume=v)
        for p in range(128):
            note('pan', pan=p)
        for v in (17, 46, 79, 113):
            for vol in (19, 53, 86, 127):
                for p in (17, 44, 63, 85, 110):
                    note('combined', velocity=v, volume=vol, pan=p)
    elif kind == 'controllers':
        emit(0xc0, 38)
        for index in range(8):
            emit(0x90, 60, 100)
            rows.append(dict(section=f'controls-{index}', tick=tick, channel=channel, key=60))
            tick += 12
            for control, value in ((7, 53), (10, 0), (10, 127), (10, 63), (10, 64),
                                   (11, 0), (11, 127), (1, 127), (1, 0), (64, 127)):
                emit(0xb0, control, value)
                tick += 12
            emit(0x80, 60, 0)
            tick += 12
            emit(0x90, 61, 100)
            rows.append(dict(section=f'controls-{index}', tick=tick, channel=channel, key=61))
            tick += 12
            emit(0xb0, 64, 0)
            tick += 12
            emit(0xe0, 127, 127)
            tick += 12
            emit(0x80, 61, 0)
            tick += 12
    elif kind == 'pitch-scale':
        emit(0xc0, 38)
        emit(0xb0, 7, 127)
        emit(0xb0, 10, 64)
        for key in range(48, 60):
            emit(0x90, key, 100)
            rows.append(dict(section=f'pitch-{key}', tick=tick, channel=channel, key=key))
            tick += 3
            for value in range(128):
                emit(0xe0, 0, value)
                tick += 2
            emit(0x80, key, 0)
            tick += 3
    elif kind in ('pitch', 'pitch-fine'):
        emit(0xc0, 38)
        emit(0xb0, 7, 127)
        emit(0xb0, 10, 64)
        for key in (12, 24, 36, 48, 60, 72, 84, 95):
            emit(0x90, key, 100)
            rows.append(dict(section=f'pitch-{key}', tick=tick, channel=channel, key=key))
            tick += 12
            for value in (0, 4096, 8192, 12288, 16383):
                emit(0xe0, value & 127, value >> 7)
                tick += 12
            emit(0x80, key, 0)
            tick += 12
            emit(0x90, 60, 100)
            rows.append(dict(section='after-wheel', tick=tick, channel=channel, key=60))
            tick += 12
            emit(0x80, 60, 0)
            tick += 12
        for channel in (2, 9):
            emit(0x90, 60, 100)
            rows.append(dict(section='sweep', tick=tick, channel=channel, key=60))
            tick += 12
            for value in range(0, 16384, 128):
                emit(0xe0, value & 127, value >> 7)
                tick += 3
            if kind == 'pitch-fine' and channel == 2:
                for high in (0, 32, 63, 64, 96, 127):
                    for low in range(128):
                        emit(0xe0, low, high)
                        tick += 2
            emit(0x80, 60, 0)
            tick += 12
        channel = 2
    elif kind == 'voices':
        emit(0xc0, 38)
        emit(0xb0, 7, 127)
        emit(0xb0, 10, 64)

        def on(section, key):
            nonlocal tick
            emit(0x90, key, 100)
            rows.append(dict(section=section, tick=tick, key=key))
            tick += 30

        def off(key):
            nonlocal tick
            emit(0x80, key, 0)
            tick += 30

        for key in range(48, 64):
            on('overflow', key)
        for key in range(48, 64):
            off(key)
        tick += 120
        for key in range(48, 57):
            on('fill', key)
        for key in (55, 49, 52):
            off(key)
        for key in (65, 66, 67):
            on('reuse', key)
        for key in range(48, 68):
            off(key)
        for _ in range(4):
            on('retrigger', 60)
        off(60)
        # Deliberately put note-off/retrigger at the same tick
        for _ in range(4):
            emit(0x90, 62, 100)
            rows.append(dict(section='same-tick', tick=tick, key=62))
            emit(0x80, 62, 0)
        tick += 120
    elif kind == 'channels':
        for case in ('baseline', 'modulation', 'bend', 'programs', 'velocities', 'reverse'):
            channels = [2, 5, 6, 7, 9, 2, 5, 6, 7]
            if case == 'reverse':
                channels.reverse()
            for channel in sorted(set(channels)):
                emit(0xc0, {2: 38, 5: 107, 6: 106, 7: 107, 9: 0}[channel] if case == 'programs' else 38)
                emit(0xb0, 7, 127)
                emit(0xb0, 10, 64)
                emit(0xb0, 1, 127 if case == 'modulation' and channel == 5 else 0)
                emit(0xe0, 0, 96 if case == 'bend' and channel == 5 else 64)
            for index in range(13):
                channel = channels[index] if index < 9 else 7
                key = 48 + index
                velocity = 30 + index * 7 if case == 'velocities' else 100
                emit(0x90, key, velocity)
                rows.append(dict(section=case, tick=tick, channel=channel, key=key, velocity=velocity))
                tick += 12
            for index in range(13):
                channel = channels[index] if index < 9 else 7
                emit(0x80, 48 + index, 0)
            tick += 120
        channel = 2
    elif kind == 'steal':
        for foreign in (5, 6, 7, 9):
            for position in range(9):
                section = f'channel-{foreign}-slot-{position}'
                for channel in (2, foreign):
                    emit(0xc0, 38)
                    emit(0xb0, 7, 127)
                    emit(0xb0, 10, 64)
                for index in range(11):
                    channel = foreign if index == position else 2
                    emit(0x90, 60, 100)
                    rows.append(dict(section=section, tick=tick, channel=channel, key=60))
                    tick += 3
                for channel in (2, foreign):
                    emit(0x80, 60, 0)
                tick += 24
        channel = 2
    elif kind == 'steal-fallback':
        for incoming in range(16):
            for channel in sorted({2, incoming}):
                emit(0xc0, 38)
                emit(0xb0, 7, 127)
                emit(0xb0, 10, 64)
                emit(0xe0, 0, 64)
            for index in range(12):
                channel = 2 if index < 9 else incoming
                emit(0x90, 48 + index, 100)
                rows.append(dict(section=f'incoming-{incoming}', tick=tick, channel=channel, key=48 + index))
                tick += 3
            for index in range(12):
                channel = 2 if index < 9 else incoming
                emit(0x80, 48 + index, 0)
            tick += 24
        channel = 2
    elif kind == 'steal-bent':
        for pitch in (0, 1, 8191, 8192, 16383):
            for pattern in ('mixed', 'single', 'last-low'):
                channels = [2] * 9 if pattern == 'single' else [2, 5, 6, 7, 9, 2, 5, 6, 7]
                if pattern == 'last-low':
                    channels = [2, 5, 6, 7, 2, 5, 6, 7, 9]
                for channel in (2, 5, 6, 7, 9):
                    emit(0xc0, 38)
                    emit(0xb0, 7, 127)
                    emit(0xb0, 10, 64)
                    emit(0xe0, pitch & 127, pitch >> 7)
                for index in range(15):
                    channel = channels[index] if index < 9 else 2
                    emit(0x90, 48 + index, 100)
                    rows.append(dict(section=f'{pattern}-{pitch}', tick=tick, channel=channel, key=48 + index))
                    tick += 3
                for index in range(15):
                    channel = channels[index] if index < 9 else 2
                    emit(0x80, 48 + index, 0)
                tick += 24
        channel = 2
    elif kind == 'steal-bend':
        channels = [2, 5, 6, 7, 9, 2, 5, 6, 7]
        for channel in sorted(set(channels)):
            emit(0xc0, 38)
            emit(0xb0, 7, 127)
            emit(0xb0, 10, 64)
        for changed in (-1, 2, 5, 6, 7, 9, 7, 6, 5, 2):
            channel = changed
            if changed >= 0:
                emit(0xe0, 0, 64)
            for index in range(15):
                channel = channels[index] if index < 9 else 7
                emit(0x90, 48 + index, 100)
                rows.append(dict(section=f'bend-{changed}', tick=tick, channel=channel, key=48 + index))
                tick += 6
            for index in range(15):
                channel = channels[index] if index < 9 else 7
                emit(0x80, 48 + index, 0)
            tick += 36
        channel = 2
    elif kind == 'steal-controls':
        channels = [2, 5, 6, 7, 9, 2, 5, 6, 7]
        for case in ('base', 'mod0', 'bend64', 'mod0-bend64', 'mod127', 'bend96', 'bend32', 'base-again'):
            for channel in sorted(set(channels)):
                emit(0xc0, 38)
                emit(0xb0, 7, 127)
                emit(0xb0, 10, 64)
                if 'mod' in case:
                    emit(0xb0, 1, 127 if case == 'mod127' else 0)
                if 'bend' in case:
                    emit(0xe0, 0, 96 if case == 'bend96' else 32 if case == 'bend32' else 64)
            for index in range(13):
                channel = channels[index] if index < 9 else 7
                emit(0x90, 48 + index, 100)
                rows.append(dict(section=case, tick=tick, channel=channel, key=48 + index))
                tick += 12
            for index in range(13):
                channel = channels[index] if index < 9 else 7
                emit(0x80, 48 + index, 0)
            tick += 36
        channel = 2
    elif kind == 'steal-spacing':
        channels = [2, 5, 6, 7, 9, 2, 5, 6, 7]
        for channel in sorted(set(channels)):
            emit(0xc0, 38)
            emit(0xb0, 7, 127)
            emit(0xb0, 10, 64)
        for spacing in (1, 2, 3, 4, 6, 8, 10, 12, 16, 24, 36):
            for pattern in ('mixed', 'single'):
                for index in range(13):
                    channel = (channels[index] if index < 9 else 7) if pattern == 'mixed' else 2
                    emit(0x90, 48 + index, 100)
                    rows.append(dict(section=f'{pattern}-{spacing}', tick=tick, channel=channel, key=48 + index))
                    tick += spacing
                for index in range(13):
                    channel = (channels[index] if index < 9 else 7) if pattern == 'mixed' else 2
                    emit(0x80, 48 + index, 0)
                tick += 36
        channel = 2
    elif kind == 'steal-keys':
        channels = [2, 5, 6, 7, 9, 2, 5, 6, 7]
        for channel in sorted(set(channels)):
            emit(0xc0, 38)
            emit(0xb0, 7, 127)
            emit(0xb0, 10, 64)
        for incoming in range(128):
            for index, channel in enumerate(channels):
                emit(0x90, 48 + index, 100)
                rows.append(dict(section=f'key-{incoming}', tick=tick, channel=channel, key=48 + index))
                tick += 1
            channel = 7
            emit(0x90, incoming, 100)
            rows.append(dict(section=f'key-{incoming}', tick=tick, channel=channel, key=incoming))
            tick += 1
            emit(0x80, incoming, 0)
            for index, channel in enumerate(channels):
                emit(0x80, 48 + index, 0)
            tick += 12
        channel = 2
    else:
        melodic, drums = (('hammelo.bnk', 'hamdrum.bnk') if kind == 'rhythm-ham'
                          else ('rickmelo.bnk', 'rickdrum.bnk'))
        lines = files['descent.sng'].decode('ascii').splitlines()
        lines[0] = f'{song} {melodic} {drums}'
        (game / 'DESCENT.SNG').write_text('\n'.join(lines) + '\n', encoding='ascii')
        patches = [patch for patch in read_bank(files[drums]) if patch['mode']]
        for _ in range(32 // len(patches)):
            for patch in patches:
                note('rhythm', key=patch['program'])
                tick += 102
    end = tick + 240
    def track_bytes(track_events):
        payload, previous = bytearray(), 0
        for when, message in track_events:
            payload += delta(when - previous) + message
            previous = when
        payload += delta(end - previous) + bytes.fromhex('ff 2f 00')
        return payload

    tracks = [(0, bytes.fromhex('80 ff 2f 00'))]
    if kind.startswith('steal') or kind == 'channels':
        for midi_channel in sorted({message[0] & 15 for _, message in events}):
            tracks.append((midi_channel, track_bytes([(t, m) for t, m in events if m[0] & 15 == midi_channel])))
    else:
        tracks.append((channel, track_bytes(events)))
    if declared_channel is not None:
        if len(tracks) != 2 or not 0 <= declared_channel < 16:
            raise ValueError('A declared-channel override requires a single music track and channel 0..15')
        tracks[1] = (declared_channel, tracks[1][1])
    header = bytearray(files[song][:0x308])
    struct.pack_into('<IIII', header, 0x30, len(tracks), 480, 120, (end + 119) // 120)
    for index, (midi_channel, track) in enumerate(tracks):
        if (kind.startswith('steal') or kind == 'channels') and index:
            struct.pack_into('<5I', header, 0x94 + (index - 1) * 20, 0xa002, 0, 0, 0, 0)
        header += struct.pack('<III', index, 12 + len(track), midi_channel) + track
    struct.pack_into('<I', header, 32, len(header))
    header += bytes(len(tracks))  # No HMI branches
    # Both names prevent DOS's FM HMQ substitution from selecting another stimulus
    for name in (song, song[:-1] + 'q'):
        (game / name.upper()).write_bytes(header)
    manifest = dict(song=song, kind=kind, rate=120, duration_seconds=end / 120,
                    declared_channel_override=declared_channel,
                    sha256=hashlib.sha256(header).hexdigest(), notes=rows,
                    events=[dict(tick=t, message=list(message)) for t, message in events])
    (session / 'probe.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf8')
    print(f'Prepared {len(rows)} notes, {end / 120:.1f} seconds')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--session', type=Path, required=True)
    parser.add_argument('--declared-channel', type=int)
    parser.add_argument('--kind', choices=('volume', 'controllers', 'pitch', 'pitch-fine', 'pitch-scale', 'voices', 'channels', 'steal', 'steal-keys', 'steal-spacing', 'steal-controls', 'steal-bend', 'steal-bent', 'steal-fallback', 'rhythm-ham', 'rhythm-rick'), default='volume')
    args = parser.parse_args()
    prepare(args.session, args.kind, args.declared_channel)
