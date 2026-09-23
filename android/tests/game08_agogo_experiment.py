"""Create measured audio-only agogo balance variants and listening clips.

This is an offline experiment, not a default asset or MIDI velocity change.
Requires numpy 2.2.6 and soundfile 0.13.1 for waveform measurements.
"""
import argparse
import hashlib
import html
import json
from pathlib import Path
import struct
import subprocess

import numpy as np
import soundfile as sf

from fm_feasibility.experiment import hog_members


def chunks(data, start=12, end=None):
    end = len(data) if end is None else end
    while start < end:
        tag, size = struct.unpack_from('<4sI', data, start)
        payload = start + 8
        assert payload + size <= end
        if tag == b'LIST':
            yield from chunks(data, payload + 4, payload + size)
        else:
            yield tag, payload, size
        start = payload + size + (size & 1)


def agogo_generators(data):
    table = {tag: (offset, size) for tag, offset, size in chunks(data)}

    def records(tag, fmt):
        offset, size = table[tag]
        return list(struct.iter_unpack(fmt, data[offset:offset + size]))

    ph = records(b'phdr', '<20sHHHIII')
    pb = records(b'pbag', '<HH')
    pg = records(b'pgen', '<HH')
    instruments = records(b'inst', '<20sH')
    bags = records(b'ibag', '<HH')
    generators = records(b'igen', '<HH')
    samples = records(b'shdr', '<20sIIIIIBbHH')
    result = []
    for index, preset in enumerate(ph[:-1]):
        if preset[1:3] != (24, 128):
            continue
        for bag in range(preset[3], ph[index + 1][3]):
            pg_values = dict(pg[pb[bag][0]:pb[bag + 1][0]])
            if 41 not in pg_values:
                continue
            instrument = pg_values[41]
            for zone in range(instruments[instrument][1], instruments[instrument + 1][1]):
                start, end = bags[zone][0], bags[zone + 1][0]
                values = dict(generators[start:end])
                key = values.get(43, 0)
                if key not in (67 | 67 << 8, 68 | 68 << 8):
                    continue
                assert samples[values[53]][0].split(b'\0')[0] == b'Agogo Bell'
                attenuation = next(i for i in range(start, end) if generators[i][0] == 48)
                result.append({'instrument': instrument, 'key': key & 255,
                               'offset': table[b'igen'][0] + attenuation * 4 + 2,
                               'attenuation_cb': values[48], 'generators': values})
    assert len(result) == 4
    for key in (67, 68):
        pair = [r for r in result if r['key'] == key]
        assert len(pair) == 2 and pair[0]['generators'] == pair[1]['generators']
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bin', type=Path, default=Path('android/build/host-extract-tests/Release'))
    parser.add_argument('--hog', type=Path, required=True)
    parser.add_argument('--sf2', type=Path, default=Path('android/app/src/main/assets/gm.sf2'))
    parser.add_argument('--reference', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    original = args.sf2.read_bytes()
    zones = agogo_generators(original)
    hmp = args.output / 'game08.hmp'
    midi = args.output / 'game08.mid'
    hmp.write_bytes(hog_members(args.hog)['game08.hmp'])

    def run(*parts):
        return subprocess.run(list(map(str, parts)), check=True, capture_output=True, text=True, timeout=60)

    run(args.bin / 'hmp_midi_export.exe', hmp, midi, 'once')
    report = {'original_sha256': hashlib.sha256(original).hexdigest(), 'zones': zones, 'variants': {}}
    for db in (0, 6, 12):
        font = args.sf2
        full = args.output / f'mix-minus{db}dB.wav'
        run(args.bin / 'midi_tsf_render.exe', font, midi, full, 0, 20000, -1, 9, 67, -db)
        isolated = args.output / f'agogo-minus{db}dB.wav'
        # A diagnostic MIDI containing only agogo notes retains all controllers
        from midi_diff import read_midi
        from fm_feasibility.experiment import write_events
        events, _ = read_midi(midi)
        selected = [(round(e.ms * 120 / 1000), 0, i, e.status, e.data[0], e.data[1] if len(e.data) == 2 else 0)
                    for i, e in enumerate(events) if e.ms < 20000 and
                    (e.status & 0xf0 not in (0x80, 0x90) or e.status & 15 == 9 and e.data[0] == 67)]
        write_events(args.output, 'isolated', 120, selected, 20)
        run(args.bin / 'midi_tsf_render.exe', font, args.output / 'isolated.mid', isolated, 0, 20000, 9, 9, 67, -db)
        agogo, rate = sf.read(isolated)
        rms = float(np.sqrt(np.mean(agogo * agogo)))
        report['variants'][str(db)] = {'font': str(font), 'isolated_rms': rms}
        # Keep exactly the same playback gain across mixes; never renormalize each
        data, rate = sf.read(full)
        sf.write(args.output / f'listen-minus{db}dB.wav', data * 0.75, rate, subtype='PCM_16')
        if db:
            measured = 20 * np.log10(rms / report['variants']['0']['isolated_rms'])
            assert abs(measured + db) < 0.03, measured
            report['variants'][str(db)]['measured_change_db'] = float(measured)
        # Other channels must remain sample-identical under the diagnostic control
        for name, channel, key in [('kick', 9, 36), ('melodic-agogo', 6, -1)]:
            stem = args.output / f'{name}-minus{db}dB.wav'
            if key >= 0:
                run(args.bin / 'midi_tsf_render.exe', font, midi, stem, 0, 20000, channel, key)
            else:
                run(args.bin / 'midi_tsf_render.exe', font, midi, stem, 0, 20000, channel, 9, 67, -db)
            if db:
                assert stem.read_bytes() == (args.output / f'{name}-minus0dB.wav').read_bytes()
    reference, rate = sf.read(args.reference)
    reference = reference[:rate * 20]
    # Reference gain matches overall RMS of the unmodified mix, not each candidate
    baseline, _ = sf.read(args.output / 'listen-minus0dB.wav')
    gain = min(float(np.sqrt(np.mean(baseline * baseline) / np.mean(reference * reference))),
               0.95 / float(np.max(np.abs(reference))))
    sf.write(args.output / 'reference.wav', reference * gain, rate, subtype='PCM_16')
    report['reference_gain_db'] = float(20 * np.log10(gain))
    report['passed'] = True
    (args.output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    clips = [('reference.wav', 'Supplied SC-55 recording (arrangement unverified)'),
             ('listen-minus0dB.wav', 'GM HMP with bundled SF2 unchanged'),
             ('listen-minus6dB.wav', 'Electronic agogo 6 dB quieter'),
             ('listen-minus12dB.wav', 'Electronic agogo 12 dB quieter'),
             ('agogo-minus0dB.wav', 'Isolated opening agogo (original level)'),
             ('melodic-agogo-minus0dB.wav', 'Separate melodic agogo (enters around 8.4 seconds)')]
    sections = ''.join(f'<h2>{html.escape(label)}</h2><audio controls preload="none" src="{file}"></audio>'
                       for file, label in clips)
    (args.output / 'listen.html').write_text(
        '<!doctype html><meta charset="utf-8"><title>Game08 agogo balance</title>'
        '<style>body{font:18px system-ui;max-width:850px;margin:40px auto;padding:0 20px}'
        'audio{width:100%}h2{font-size:20px}</style><h1>Game08 opening bell balance</h1>'
        '<p>The opening bell is drum note 67, Agogo Bell, in the Electronic kit. '
        'These audio-only candidates reduce note 67 on the percussion channel. '
        'Mixes use identical gain; only the reference is adjusted for overall level. '
        'All generated clips use GM HMP. The supplied recording has not been verified '
        'as an HMP reference; the separate DOS FM capture uses HMQ. '
        'These trials demonstrate a balance change, but cannot establish the correct level. '
        'A DOS General MIDI event comparison and an independent synth comparison are '
        'needed before choosing a correction.</p>' + sections +
        '<p>The bundled font and app playback are unchanged. The diagnostic control is '
        'specific to this offline renderer; it is not a proposed global MIDI velocity change.</p>'
        '<script>document.addEventListener("play",e=>document.querySelectorAll("audio")'
        '.forEach(a=>{if(a!==e.target)a.pause()}),true)</script>', encoding='utf8')
    print(args.output / 'listen.html')


if __name__ == '__main__':
    main()
