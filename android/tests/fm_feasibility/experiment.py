"""Prepare and measure offline FM experiments; no changes to production playback.

BNK layout: Tero Totto's Ad Lib programming guide, Appendix A
https://franticware.github.io/miscellaneous/adlib-programming-guide.html
HMI drum fixed pitch in the name-record flag byte is an experiment hypothesis,
supported by the local bank's clshat97/Opnhat96 names and corresponding 97/96 bytes.
WOPL serialization targets the pinned BSD ymfmidi reader; no GPL implementation
or third-party patch collection is used.
"""

import argparse
import array
import hashlib
import html
import json
import math
from pathlib import Path
import struct
import subprocess
import sys
import wave


HERE = Path(__file__).resolve().parent
MANIFEST = json.loads((HERE / 'dependencies.json').read_text())


def run(command):
    return subprocess.check_output(list(map(str, command)), text=True).strip()


def sha(data):
    return hashlib.sha256(data).hexdigest()


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf8')


def dependencies(root):
    result = {}
    for name, spec in MANIFEST.items():
        path = root / name
        if not path.exists():
            subprocess.run(['git', 'clone', '--quiet', '--no-checkout', spec['url'], str(path)], check=True)
            subprocess.run(['git', '-C', str(path), 'checkout', '--quiet', spec['commit']], check=True)
        if run(['git', '-C', path, 'rev-parse', 'HEAD']) != spec['commit']:
            raise ValueError(f'{name}: dependency revision mismatch; use a separate work directory')
        if run(['git', '-C', path, 'status', '--porcelain', '--untracked-files=no']):
            raise ValueError(f'{name}: dependency has tracked modifications')
        license_bytes = (path / spec['license_file']).read_bytes()
        result[name] = dict(spec, license_sha256=sha(license_bytes))
    return result


def hog_members(path):
    data = path.read_bytes()
    if data[:3] != b'DHF':
        raise ValueError('Expected a DHF HOG')
    pos, files = 3, {}
    while pos < len(data):
        if pos + 17 > len(data):
            raise ValueError('Truncated HOG directory entry')
        name = data[pos:pos + 13].split(b'\0')[0].decode('ascii').lower()
        length = struct.unpack_from('<I', data, pos + 13)[0]
        pos += 17
        if pos + length > len(data) or name in files:
            raise ValueError('Truncated or duplicate HOG member')
        files[name] = data[pos:pos + length]
        pos += length
    return files


def read_bank(data):
    if len(data) < 28 or data[2:8] != b'ADLIB-':
        raise ValueError('Not an AdLib BNK')
    used, count, names, records = struct.unpack_from('<HHII', data, 8)
    if used > count or not 1 <= count <= 128 or names < 28 or records < names + count * 12:
        raise ValueError('Unsupported BNK layout')
    result = []
    for program in range(count):
        pos = names + program * 12
        if pos + 12 > len(data):
            raise ValueError('Truncated BNK name')
        index, fixed_note = struct.unpack_from('<HB', data, pos)
        start = records + index * 30
        if start + 30 > len(data) or fixed_note > 127:
            raise ValueError('Invalid BNK instrument offset/pitch')
        raw = data[start:start + 30]
        if raw[0] > 1:
            raise ValueError('Unknown BNK instrument mode')
        ops = []
        for op in range(2):
            p = raw[2 + op * 13:15 + op * 13]
            # Carrier feedback/connection fields contain unrelated bytes in HMI banks
            mode = ((p[9] & 1) << 7 | (p[10] & 1) << 6 | (p[5] & 1) << 5 |
                    (p[11] & 1) << 4 | (p[1] & 15))
            ops.append([mode, (p[0] & 3) << 6 | p[8] & 63,
                        (p[3] & 15) << 4 | p[6] & 15,
                        (p[4] & 15) << 4 | p[7] & 15, raw[28 + op] & 3])
        result.append({'program': program, 'data_index': index,
                       'name': data[pos + 3:pos + 12].split(b'\0')[0].decode('ascii'),
                       'mode': raw[0], 'voice': raw[1],
                       'fixed_note': fixed_note, 'operators': ops,
                       'connection': (raw[4] & 7) << 1 | raw[14] & 1})
    return result


def make_wopl(melodic, drums, invert_connection=False):
    # WOPL v2, one melodic and one percussion bank, 62-byte instruments
    output = bytearray(b'WOPL3-BANK\0' + struct.pack('<H', 2) + struct.pack('>HH', 1, 1) + b'\0\0')
    output += bytes(68)
    for bank in (melodic, drums):
        for number in range(128):
            patch = bytearray(62)
            if number >= len(bank) or bank[number]['mode']:
                patch[39] = 4
            else:
                source = bank[number]
                name = source['name'].encode('ascii')[:31]
                patch[:len(name)] = name
                patch[38] = source['fixed_note']
                patch[40] = source['connection'] ^ int(invert_connection)
                # WOPL stores carrier first, then modulator
                patch[42:47] = bytes(source['operators'][1])
                patch[47:52] = bytes(source['operators'][0])
            output += patch
    return bytes(output)


def number(data, pos, hmp=False):
    value = 0
    for i in range(4):
        if pos >= len(data):
            raise ValueError('Truncated event delta')
        byte = data[pos]
        pos += 1
        value = value | ((byte & 127) << (i * 7)) if hmp else (value << 7) | (byte & 127)
        if bool(byte & 128) == hmp:
            return value, pos
    raise ValueError('Oversized event delta')


def hmp_events(data, seconds, device=0xa002):
    if len(data) < 0x308 or data[:8] != b'HMIMIDIP' or data[8] != 0:
        raise ValueError('Expected standard Descent HMP')
    tracks = struct.unpack_from('<I', data, 0x30)[0]
    rate = struct.unpack_from('<I', data, 0x38)[0]
    if not 1 <= tracks <= 32 or not 1 <= rate <= 32767:
        raise ValueError('Invalid HMP tracks/tick rate')
    selected, events, removed_controls = [], [], {}
    offset = 0x308
    for track in range(tracks):
        devices = struct.unpack_from('<4I', data, 0x94 + (track - 1) * 20) if track else ()
        enabled = not any(devices) or device in devices
        if enabled:
            selected.append(track)
        if offset + 12 > len(data):
            raise ValueError('Missing HMP track')
        size = struct.unpack_from('<I', data, offset + 4)[0]
        if size < 12 or offset + size > len(data):
            raise ValueError('Invalid HMP track size')
        payload = data[offset + 12:offset + size]
        offset += size
        pos, tick, shift, ended = 0, 0, 0, False
        while pos < len(payload):
            first = pos == 0
            delta, pos = number(payload, pos, True)
            tick += delta
            if first:
                shift = int(delta != 0)
            if pos >= len(payload):
                raise ValueError('Missing HMP status')
            status = payload[pos]
            pos += 1
            if status == 255:
                if pos >= len(payload):
                    raise ValueError('Missing meta event')
                kind = payload[pos]
                length, pos = number(payload, pos + 1)
                pos += length
                if pos > len(payload):
                    raise ValueError('Truncated meta event')
                if kind == 47:
                    if length or pos != len(payload):
                        raise ValueError('Invalid end-of-track')
                    ended = True
                    break
                continue
            if not 128 <= status < 240:
                raise ValueError('Unsupported HMP status')
            length = 1 if status & 0xf0 in (0xc0, 0xd0) else 2
            message = payload[pos:pos + length]
            if len(message) != length:
                raise ValueError('Truncated HMP message')
            pos += length
            time = max(0, tick - shift)
            if status & 0xf0 == 0xb0 and 108 <= message[0] <= 119:
                if enabled:
                    key = str(message[0])
                    removed_controls[key] = removed_controls.get(key, 0) + 1
                    if message[0] == 111 and time < seconds * rate:
                        raise ValueError('Clip crosses an HMI branch; shorten --seconds')
                continue
            if any(byte > 127 for byte in message):
                raise ValueError('Non-MIDI event data')
            if enabled and time < seconds * rate:
                events.append((time, track, len(events), status, *message, *([0] if length == 1 else [])))
        if not ended:
            raise ValueError('Missing HMP end-of-track')
    events.sort(key=lambda e: e[:3])
    if not any(e[3] & 0xf0 == 0x90 and e[5] for e in events):
        raise ValueError('No notes in selected arrangement')
    return rate, events, {'selected_tracks': selected, 'removed_hmi_controls': removed_controls}


def vlq(value):
    result = [value & 127]
    while value >> 7:
        value >>= 7
        result.insert(0, 128 | (value & 127))
    return bytes(result)


def write_events(folder, stem, rate, events, seconds):
    payload, previous, rows = bytearray(b'\0\xff\x51\x03\x0f\x42\x40'), 0, []
    for tick, _track, _order, status, a, b in events:
        payload += vlq(tick - previous) + bytes((status, a))
        if status & 0xf0 not in (0xc0, 0xd0):
            payload.append(b)
        previous = tick
        rows.append(f'{round(tick * 48000 / rate)} {status} {a} {b}\n')
    payload += vlq(max(previous, math.ceil(seconds * rate)) - previous) + b'\xff\x2f\0'
    (folder / f'{stem}.mid').write_bytes(b'MThd' + struct.pack('>IHHH', 6, 0, 1, rate) + b'MTrk' + struct.pack('>I', len(payload)) + payload)
    (folder / f'{stem}.events').write_text(''.join(rows), encoding='ascii')


def samples(path):
    with wave.open(str(path)) as source:
        if source.getsampwidth() != 2 or source.getnchannels() != 2:
            raise ValueError('Expected stereo PCM16 WAV')
        pcm = array.array('h', source.readframes(source.getnframes()))
        if sys.byteorder != 'little':
            pcm.byteswap()
        return source.getframerate(), pcm


def metrics(path):
    rate, pcm = samples(path)
    rms = math.sqrt(sum(s * s for s in pcm) / len(pcm)) / 32768
    return {'seconds': len(pcm) / (rate * 2), 'sample_rate': rate,
            'rms_dbfs': round(20 * math.log10(max(rms, 1e-12)), 3),
            'peak': max(map(abs, pcm)), 'clipped_samples': sum(s in (-32768, 32767) for s in pcm),
            'sha256': sha(path.read_bytes())}


def envelope(path, window_ms=10):
    rate, pcm = samples(path)
    step = round(rate * window_ms / 1000) * 2
    return [math.sqrt(sum(x * x for x in pcm[pos:pos + step]) / step)
            for pos in range(0, len(pcm) - step + 1, step)]


def envelope_alignment(candidate, reference):
    """Rhythm-only diagnostic: search reference lead-in, no time stretching."""
    a, b = envelope(candidate), envelope(reference)
    # Leave the final half-second out of the search to avoid clip boundaries
    a = a[:-50]
    mean_a = sum(a) / len(a)
    centered_a = [x - mean_a for x in a]
    energy_a = sum(x * x for x in centered_a)
    best = (-2.0, 0)
    for lag in range(min(300, len(b) - len(a)) + 1):
        clip = b[lag:lag + len(a)]
        mean_b = sum(clip) / len(clip)
        centered_b = [x - mean_b for x in clip]
        denominator = math.sqrt(energy_a * sum(x * x for x in centered_b))
        correlation = sum(x * y for x, y in zip(centered_a, centered_b)) / denominator if denominator else 0
        best = max(best, (correlation, lag))
    return {'envelope_correlation': round(best[0], 4), 'reference_lead_in_ms': best[1] * 10,
            'meaning': 'RMS-envelope timing diagnostic only; not timbre or sample parity'}


def listening_copy(source, target, seconds, start_ms=0):
    """Approximate timing alignment and RMS matching for human comparison only."""
    rate, pcm = samples(source)
    begin = round(start_ms * rate / 1000) * 2
    pcm = pcm[begin:begin + round(seconds * rate) * 2]
    rms = math.sqrt(sum(x * x for x in pcm) / len(pcm))
    gain = min((32768 * 0.1) / max(rms, 1), 30000 / max(1, max(map(abs, pcm))))
    scaled = array.array('h', (round(x * gain) for x in pcm))
    if sys.byteorder != 'little':
        scaled.byteswap()
    with wave.open(str(target), 'wb') as out:
        out.setnchannels(2)
        out.setsampwidth(2)
        out.setframerate(rate)
        out.writeframes(scaled.tobytes())
    return {'start_ms': start_ms, 'gain_db': round(20 * math.log10(gain), 3)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work', type=Path, default=HERE.parents[2] / 'temp/fm-feasibility')
    parser.add_argument('--hog', type=Path)
    parser.add_argument('--song', default='game07.hmp')
    parser.add_argument('--seconds', type=float, default=20)
    parser.add_argument('--fetch-only', action='store_true')
    parser.add_argument('--probe', type=Path, help='Omit to prepare assets without rendering')
    parser.add_argument('--probe-fixed', type=Path, help='Optional separately built live-bookkeeping experiment')
    parser.add_argument('--adb', type=Path, help='Run Android probes over adb instead of on the host')
    parser.add_argument('--serial', help='Explicit adb device serial')
    parser.add_argument('--adlib-wav', type=Path)
    args = parser.parse_args()
    if not 1 <= args.seconds <= 120:
        parser.error('--seconds must be in [1, 120]')
    if args.adb and (not args.probe or not args.serial):
        parser.error('--adb requires --probe and --serial')
    args.work.mkdir(parents=True, exist_ok=True)
    deps = dependencies(args.work / 'deps')
    if args.fetch_only:
        write_json(args.work / 'dependencies.json', deps)
        return
    if not args.hog:
        parser.error('--hog is required unless --fetch-only')
    files = hog_members(args.hog)
    song = args.song.lower()
    rows = [row.split() for row in files['descent.sng'].decode('ascii').splitlines()]
    sng_name = song[:-4] + '.hmp' if song.endswith('.hmq') else song
    row = next(row for row in rows if len(row) >= 3 and row[0].lower() == sng_name)
    melodic_name, drum_name = row[1].lower(), row[2].lower()
    melodic, drums = read_bank(files[melodic_name]), read_bank(files[drum_name])
    output = args.work / (song.replace('.', '-') + ('-android' if args.adb else ''))
    output.mkdir(exist_ok=True)
    (output / 'descent.wopl').write_bytes(make_wopl(melodic, drums))
    (output / 'inverted-connection.wopl').write_bytes(make_wopl(melodic, drums, True))
    write_json(output / 'instruments.json', {'melodic': melodic, 'percussion': drums})
    rate, events, arrangement = hmp_events(files[song], args.seconds)
    programs, used_patches = [0] * 16, set()
    for _tick, _track, _order, status, a, b in events:
        channel = status & 15
        if status & 0xf0 == 0xc0:
            programs[channel] = a
        elif status & 0xf0 == 0x90 and b:
            percussion = channel == 9
            key = a if percussion else programs[channel]
            bank = drums if percussion else melodic
            if key >= len(bank) or bank[key]['mode']:
                raise ValueError(f'Clip uses unsupported patch: percussion={percussion}, key={key}')
            used_patches.add(('percussion' if percussion else 'melodic', key))
    write_events(output, 'fm', rate, events, args.seconds)
    write_events(output, 'drums', rate, [e for e in events if e[3] & 15 == 9], args.seconds)
    # Equivalent MIDI note-off encodings should render identically
    test_events = [(0, 0, 0, 0xc0, 0, 0), (0, 0, 1, 0x90, 60, 100),
                   (60, 0, 2, 0x90, 60, 0), (120, 0, 3, 0x90, 60, 100),
                   (180, 0, 4, 0x80, 60, 0)]
    write_events(output, 'noteoff-zero', 120, test_events, 2)
    test_events[2] = (60, 0, 2, 0x80, 60, 0)
    write_events(output, 'noteoff-explicit', 120, test_events, 2)
    write_events(output, 'noteoff-omitted', 120, [test_events[0], test_events[1]], 2)
    report = {'dependencies': deps, 'source_hog_sha256': sha(args.hog.read_bytes()),
              'assets': {name: sha(files[name]) for name in (song, melodic_name, drum_name)},
              'song': song, 'tick_rate': rate, 'event_count': len(events), **arrangement,
              'used_patches': sorted(used_patches),
              'unused_unsupported_patches': {'melodic': [p['program'] for p in melodic if p['mode']],
                                              'percussion': [p['program'] for p in drums if p['mode']]},
              'assumptions': ['Initial nonzero track countdown shortened by one tick',
                              'Raw CC7 and default MIDI pitch; no GM-specific corrections',
                              'HMI drum pitch taken from name-record flag byte; not yet DOS-verified',
                              'Only the opening pass; no branch/EOF loop reconstruction',
                              'ymfmidi OPL2 mode is nine voices on an OPL3 core'],
              'renders': {}}
    if args.probe:
        remote = '/data/local/tmp/dxx-fm-feasibility'
        adb = [args.adb, '-s', args.serial] if args.adb else []
        if adb:
            run([*adb, 'shell', 'mkdir', '-p', remote])
            for local in output.iterdir():
                if local.suffix in ('.wopl', '.mid', '.events'):
                    run([*adb, 'push', local, f'{remote}/{local.name}'])
            for binary, name in ((args.probe, 'probe'), (args.probe_fixed, 'probe-fixed')):
                if binary:
                    run([*adb, 'push', binary, f'{remote}/{name}'])
                    run([*adb, 'shell', 'chmod', '755', f'{remote}/{name}'])
            report['execution'] = {'platform': 'Android', 'serial': args.serial,
                                   'abi': run([*adb, 'shell', 'getprop', 'ro.product.cpu.abi'])}
        else:
            report['execution'] = {'platform': sys.platform}

        def render(binary, mode, bank, sequence, target, duration, fixed=False):
            if adb:
                name = 'probe-fixed' if fixed else 'probe'
                result = run([*adb, 'shell', f'{remote}/{name}', mode, f'{remote}/{bank.name}',
                              f'{remote}/{sequence.name}', f'{remote}/{target.name}', duration])
                run([*adb, 'pull', f'{remote}/{target.name}', target])
            else:
                result = run([binary.resolve(), mode, bank, sequence, target, duration])
            return json.loads(result)

        tasks = [('fm-opl2', 'file-opl2', 'fm.mid', 'descent.wopl', args.seconds),
                 ('fm-opl3', 'file-opl3', 'fm.mid', 'descent.wopl', args.seconds),
                 ('fm-live', 'live-opl2', 'fm.events', 'descent.wopl', args.seconds),
                 ('drums-opl2', 'file-opl2', 'drums.mid', 'descent.wopl', args.seconds),
                 ('connection-inverted', 'file-opl2', 'fm.mid', 'inverted-connection.wopl', args.seconds),
                 ('tone-ymfm', 'tone-ymfm', 'fm.mid', 'descent.wopl', 4),
                 ('tone-emu8950', 'tone-emu8950', 'fm.mid', 'descent.wopl', 4)]
        for stem in ('noteoff-zero', 'noteoff-explicit', 'noteoff-omitted'):
            for mode in ('file-opl2', 'live-opl2'):
                suffix = 'events' if mode == 'live-opl2' else 'mid'
                tasks.append((f'{stem}-{mode}', mode, f'{stem}.{suffix}', 'descent.wopl', 2))
        for name, mode, sequence, bank, seconds in tasks:
            target = output / f'{name}.wav'
            timing = render(args.probe, mode, output / bank, output / sequence, target, seconds)
            report['renders'][name] = dict(metrics(target), **timing)
            if report['renders'][name]['peak'] == 0:
                raise ValueError(f'{name}: silent renderer output')
        if args.probe_fixed:
            for stem in ('fm', 'noteoff-zero', 'noteoff-explicit', 'noteoff-omitted'):
                name = stem + '-live-fixed'
                target = output / f'{name}.wav'
                duration = args.seconds if stem == 'fm' else 2
                timing = render(args.probe_fixed, 'live-opl2', output / 'descent.wopl',
                                output / f'{stem}.events', target, duration, True)
                report['renders'][name] = dict(metrics(target), **timing)
        report['noteoff_equivalence'] = {}
        for mode in ('file-opl2', 'live-opl2'):
            _rate, a = samples(output / f'noteoff-zero-{mode}.wav')
            _rate, b = samples(output / f'noteoff-explicit-{mode}.wav')
            report['noteoff_equivalence'][mode] = {'identical': a == b,
                                                  'max_sample_difference': max(abs(x - y) for x, y in zip(a, b))}
        report['noteoff_effect'] = {}
        for suffix in ('file-opl2', 'live-opl2', *(['live-fixed'] if args.probe_fixed else [])):
            _rate, on_off = samples(output / f'noteoff-explicit-{suffix}.wav')
            _rate, on_only = samples(output / f'noteoff-omitted-{suffix}.wav')
            report['noteoff_effect'][suffix] = {'identical_to_no_noteoffs': on_off == on_only}
        if args.probe_fixed:
            report['fixed_live_matches_file'] = (samples(output / 'fm-opl2.wav')[1] ==
                                                 samples(output / 'fm-live-fixed.wav')[1])
        if args.adlib_wav:
            with wave.open(str(args.adlib_wav)) as source, wave.open(str(output / 'dos-adlib.wav'), 'wb') as target:
                target.setparams(source.getparams())
                target.writeframes(source.readframes(int((args.seconds + 2) * source.getframerate())))
            report['renders']['dos-adlib'] = metrics(output / 'dos-adlib.wav')
            report['dos_reference_sha256'] = sha(args.adlib_wav.read_bytes())
            report['dos_envelope_comparisons'] = {name: envelope_alignment(output / f'{name}.wav', output / 'dos-adlib.wav')
                                                  for name in ('fm-opl2', 'fm-opl3', 'connection-inverted')}
            lag = report['dos_envelope_comparisons']['fm-opl2']['reference_lead_in_ms']
            report['listening_adjustments'] = {
                'compare-dos': listening_copy(output / 'dos-adlib.wav', output / 'compare-dos.wav', args.seconds, lag),
                'compare-fm': listening_copy(output / 'fm-opl2.wav', output / 'compare-fm.wav', args.seconds)}

        def audio(name, label):
            return f'<p>{html.escape(label)}<br><audio controls preload="none" src="{name}.wav"></audio></p>'

        comparison = ''
        if args.adlib_wav:
            comparison = ('<h2>Listening comparison</h2><p>Copies with approximately matched RMS level. '
                          f'DOS lead-in trimmed by {lag} ms using an envelope correlation search; '
                          'this is approximate alignment, with no time stretching.</p>' +
                          audio('compare-dos', 'Original DOSBox AdLib') +
                          audio('compare-fm', 'Experimental BSD FM renderer'))
        sections = ''.join(audio(name, name) for name in report['renders'])
        (output / 'listen.html').write_text(
            '<!doctype html><html lang="en"><meta charset="utf-8"><title>FM feasibility</title>'
            '<style>body{font:18px system-ui;max-width:850px;margin:40px auto;padding:0 20px}audio{width:100%}</style>'
            '<h1>FM library experiments</h1><p>Original Descent banks and FM arrangement. '
            'Provisional driver behavior, not a DOS parity claim. DOS audio includes briefing lead-in; '
            'clips retain raw gain and are not precisely aligned. Connection-inverted is a bank decoding '
            'sensitivity check. Tone clips compare chip cores with the same register stimulus.</p>' + comparison +
            '<details><summary>Raw renders and diagnostic clips</summary>' + sections + '</details></html>\n',
            encoding='utf8')
    write_json(output / 'report.json', report)
    print(output / 'report.json')


if __name__ == '__main__':
    main()
