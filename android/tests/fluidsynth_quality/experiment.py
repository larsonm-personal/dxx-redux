"""Audition identical exported HMP MIDI with TSF and isolated FluidSynth effects.

Requires numpy==2.2.6 and soundfile==0.13.1. Builds are separate; see README.md.
"""
import argparse
import hashlib
import html
import json
from pathlib import Path
import subprocess
import sys

import numpy as np
import soundfile as sf

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from fm_feasibility.experiment import hog_members

VARIANTS = [
    ('tsf', 'Current TinySoundFont', None),
    ('dry', 'FluidSynth: dry', (0, 0, 0)),
    ('reverb', 'FluidSynth: reverb', (1, 0, 0)),
    ('chorus', 'FluidSynth: chorus', (0, 1, 24)),
    ('both', 'FluidSynth: reverb + chorus', (1, 1, 24)),
]


def run(command, log):
    result = subprocess.run(list(map(str, command)), capture_output=True, text=True, timeout=180)
    log.write_text(result.stdout + result.stderr, encoding='utf8')
    if result.returncode:
        raise RuntimeError(f'{command[0]} failed; see {log}\n{result.stderr[-2000:]}')
    return result.stdout


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fluid', type=Path, required=True)
    parser.add_argument('--host-bin', type=Path, default=Path('android/build/host-extract-tests/Release'))
    parser.add_argument('--hog', type=Path, required=True)
    parser.add_argument('--font', action='append', required=True, help='Label=path.sf2; repeatable')
    parser.add_argument('--songs', nargs='+', default=['game01', 'game07', 'game08'])
    parser.add_argument('--seconds', type=int, default=60)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    assert 1 <= args.seconds <= 120
    args.output.mkdir(parents=True, exist_ok=True)
    raw = args.output / 'raw'
    raw.mkdir(exist_ok=True)
    fonts = [value.split('=', 1) for value in args.font]
    members = hog_members(args.hog)
    report = {'hog': str(args.hog.resolve()), 'hog_sha256': sha(args.hog), 'seconds': args.seconds,
              'method': 'Same shipping HMP export and millisecond event scheduler; 48 voices, 48 kHz; RMS matching with peak cap, not LUFS normalization',
              'fonts': [{'label': label, 'path': str(Path(path).resolve()), 'sha256': sha(Path(path))} for label, path in fonts],
              'variants': VARIANTS, 'clips': []}
    for song in args.songs:
        assert song.isalnum()
        hmp = raw / f'{song}.hmp'
        hmp.write_bytes(members[song + '.hmp'])
        midi = raw / f'{song}.mid'
        run([args.host_bin / 'hmp_midi_export.exe', hmp, midi, 'repeat'], raw / f'{song}-export.log')
        for index, (label, font) in enumerate(fonts):
            for variant, title, effects in VARIANTS:
                stem = f'{song}-font{index}-{variant}'
                wav = raw / f'{stem}.wav'
                if effects is None:
                    command = [args.host_bin / 'midi_tsf_render.exe', font, midi, wav, 0, args.seconds * 1000]
                else:
                    command = [args.fluid, font, midi, wav, args.seconds, *effects]
                output = run(command, raw / f'{stem}.log')
                data, rate = sf.read(wav, always_2d=True)
                assert rate == 48000 and data.shape == (args.seconds * rate, 2)
                assert np.isfinite(data).all()
                peak = float(np.max(np.abs(data)))
                rms = float(np.sqrt(np.mean(data * data)))
                assert rms > 0 and peak <= 1, stem
                gain = min(0.1 / rms, 0.891 / peak)
                listen = f'{stem}.wav'
                sf.write(args.output / listen, data * gain, rate, subtype='PCM_16')
                metrics = json.loads(output) if effects is not None else {}
                assert not metrics.get('clipped_samples', 0), stem
                report['clips'].append({'song': song, 'font': index, 'variant': variant, 'title': title,
                                        'file': listen, 'raw_sha256': sha(wav), 'midi_sha256': sha(midi),
                                        'rms_dbfs': float(20 * np.log10(rms)),
                                        'pcm_boundary_samples': int(np.count_nonzero((data <= -1) | (data >= 32767 / 32768))),
                                        'peak_dbfs': float(20 * np.log10(peak)),
                                        'listen_gain_db': float(20 * np.log10(gain)), **metrics})
                print(f'{song} / {label} / {title}', flush=True)
    (args.output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    song_options = ''.join(f'<option>{html.escape(s)}</option>' for s in args.songs)
    font_options = ''.join(f'<option value="{i}">{html.escape(label)}</option>' for i, (label, _) in enumerate(fonts))
    buttons = ''.join(f'<button data-variant="{v}">{html.escape(label)}</button>' for v, label, _ in VARIANTS)
    page = '''<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width">
<title>SoundFont renderer and effects comparison</title><style>
body{font:17px system-ui;max-width:900px;margin:35px auto;padding:0 20px;background:#17202b;color:#e8edf3}
select,button{font:inherit;margin:6px;padding:10px;border-radius:6px}button.active{background:#93ccef}
audio{width:100%;margin:20px 0}p{line-height:1.5}small{color:#bfd1e2}
</style><h1>SoundFont rendering: dry, reverb and chorus</h1>
<p>Start with Current vs FluidSynth dry, then try the effects separately. All versions use the same
MIDI events and soundfont. Switching variants retains the play position.</p>
<label>Song <select id="song">SONGS</select></label><label>Soundfont <select id="font">FONTS</select></label>
<div id="variants">BUTTONS</div><h2 id="label"></h2><audio id="audio" controls preload="metadata"></audio>
<p><b>Chorus is an explicit experiment:</b> its variants add MIDI CC93=24 on melodic channels;
drums retain their original send. Later song controls can override it. Reverb uses the font and
renderer defaults for sends, with fixed conservative FDN effect parameters.</p>
<p>FluidSynth 2.6.1 uses seventh-order interpolation; all renders use 48 voices and 48 kHz.
Listening copies are approximately RMS-matched with a peak cap, without EQ or compression.
This is a soundfont experiment, not an SC-55 emulation or a production app update.</p>
<small id="detail"></small><script>
const clips=CLIPS;
const player=document.querySelector('#audio'),song=document.querySelector('#song'),font=document.querySelector('#font');
let variant='tsf',request=0;
function update(preserve){const id=++request,time=preserve?player.currentTime:0,play=!player.paused;
const clip=clips.find(c=>c.song===song.value&&c.font===Number(font.value)&&c.variant===variant);
player.pause();player.src=clip.file;document.querySelector('#label').textContent=clip.title;
document.querySelector('#detail').textContent='Listening gain: '+clip.listen_gain_db.toFixed(2)+' dB; raw peak: '+clip.peak_dbfs.toFixed(2)+' dBFS';
document.querySelectorAll('button').forEach(b=>b.classList.toggle('active',b.dataset.variant===variant));
player.onloadedmetadata=()=>{if(id!==request)return;player.currentTime=Math.min(time,Math.max(0,player.duration-0.01));if(play)player.play().catch(()=>{});};}
document.querySelectorAll('button').forEach(b=>b.onclick=()=>{variant=b.dataset.variant;update(true);});
song.onchange=()=>update(false);font.onchange=()=>update(true);update(false);
</script></html>'''
    for key, value in [('SONGS', song_options), ('FONTS', font_options), ('BUTTONS', buttons),
                       ('CLIPS', json.dumps(report['clips']).replace('<', '\\u003c'))]:
        page = page.replace(key, value)
    (args.output / 'listen.html').write_text(page, encoding='utf8')
    print(args.output / 'listen.html')


if __name__ == '__main__':
    main()
