"""Build a Level 8 listening comparison with original SC-55 and DOS captures.

Offline dependencies: numpy 2.2.6, soundfile 0.13.1. No audio assets are fetched.
The before-FM clip is optional and must come from the pre-HMQ-fix renderer.
"""
import argparse
import hashlib
import html
import json
from pathlib import Path
import subprocess

import numpy as np
import soundfile as sf


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--renderer', type=Path, required=True)
    parser.add_argument('--hog', type=Path, required=True)
    parser.add_argument('--reference', type=Path, required=True)
    parser.add_argument('--dos-wav', type=Path, required=True)
    parser.add_argument('--before-fm', type=Path)
    parser.add_argument('--sf2', type=Path, default=Path('android/app/src/main/assets/gm.sf2'))
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    clips = [('reference', 'Your SC-55 recording', args.reference),
             ('dos', 'Original DOS FM capture (HMQ)', args.dos_wav)]
    if args.before_fm:
        clips.append(('before', 'Before: HMP with FM banks (wrong instruments)', args.before_fm))
    for profile, label in [('ymfm', 'Corrected FM: HMQ with original FM banks'),
                           ('sf2', 'Soundfont: GM HMP with bundled SF2')]:
        path = args.output / f'{profile}-raw.wav'
        result = subprocess.run([str(args.renderer), '--render', str(args.sf2), str(args.hog),
                                 'game08.hmp', str(path), profile, '40'],
                                capture_output=True, text=True, check=True, timeout=60)
        assert f'actual={profile}' in result.stdout
        if profile == 'ymfm':
            assert 'sequence=game08.hmq' in result.stderr
        (args.output / f'{profile}.log').write_text(result.stdout + result.stderr, encoding='utf8')
        clips.append((profile, label, path))
    report, sections = {}, []
    for name, label, path in clips:
        data, rate = sf.read(path, always_2d=True)
        start = 0
        if name == 'dos':
            # Remove DOS startup silence, retaining 125 ms before first energy
            block = rate // 100
            mono_power = np.mean(data * data, axis=1)
            blocks = mono_power[:len(data) // block * block].reshape(-1, block).mean(axis=1)
            active = np.flatnonzero(blocks > 1e-6)
            assert len(active), 'DOS capture is silent'
            start = max(0, int(active[0] * block - rate * 0.125))
        data = data[start:start + rate * 40]
        peak = float(np.max(np.abs(data)))
        rms = float(np.sqrt(np.mean(data * data)))
        assert rms > 0
        gain = min(0.1 / rms, 0.891 / peak)
        sf.write(args.output / f'{name}-listen.wav', data * gain, rate, subtype='PCM_16')
        report[name] = {'source': str(path), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                        'start_seconds': start / rate, 'seconds': len(data) / rate,
                        'raw_rms': rms, 'raw_peak': peak, 'listen_gain_db': float(20 * np.log10(gain))}
        sections.append(f'<h2>{html.escape(label)}</h2><audio controls preload="none" '
                        f'src="{name}-listen.wav"></audio>')
    (args.output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    (args.output / 'listen.html').write_text(
        '<!doctype html><html lang="en"><meta charset="utf-8"><title>Game08 instruments</title>'
        '<style>body{font:18px system-ui;max-width:850px;margin:40px auto;padding:0 20px}'
        'audio{width:100%}h2{font-size:20px}</style><h1>Game08: corrected FM arrangement</h1>'
        '<p>DOS selects game08.hmq for FM. The prototype previously combined the GM-oriented '
        'game08.hmp with the HMQ instrument banks, giving the opening incorrect instruments.</p>'
        '<p>Listening copies are approximately level-matched to -20 dBFS RMS, capped below clipping. '
        'DOS startup silence is trimmed; timing is not precisely aligned. The DOS capture is shorter '
        'than the 40-second host clips. The SC-55 and FM remain different sound profiles.</p>'
        + ''.join(sections) + '<script>document.addEventListener("play",e=>{'
        'document.querySelectorAll("audio").forEach(a=>{if(a!==e.target)a.pause()})},true)</script></html>',
        encoding='utf8')
    print(args.output / 'listen.html')


if __name__ == '__main__':
    main()
