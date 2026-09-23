"""Prepare an isolated DOS Descent capture runtime; never edits the source game."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct


def extract_member(hog, name):
    with hog.open('rb') as stream:
        if stream.read(3) != b'DHF':
            raise ValueError('Not a Descent HOG')
        while header := stream.read(17):
            if len(header) != 17:
                raise ValueError('Truncated HOG entry')
            entry = header[:13].split(b'\0')[0].decode('ascii')
            size = struct.unpack_from('<I', header, 13)[0]
            if size > 128 * 1024 * 1024:
                raise ValueError('Oversized HOG entry')
            if entry.lower() == name.lower():
                data = stream.read(size)
                if len(data) != size:
                    raise ValueError('Truncated HOG member')
                return data
            stream.seek(size, 1)
    raise ValueError(f'{name} not found in {hog}')


def prepare(source, output, executable, config_name, song=None, hog_name='DESCENT.HOG',
            music_device='general-midi', mute_effects=False, opl_capture=False):
    if opl_capture and music_device != 'adlib':
        raise ValueError('OPL capture requires the AdLib music device')
    source, output = source.resolve(), output.resolve()
    if output == source or source in output.parents:
        raise ValueError('Capture output must be outside the source runtime')
    if output.exists():
        raise ValueError('Use a new output directory to preserve previous captures')
    for name in (executable, 'DOSBOX/DOSBox.exe'):
        if not (source / name).is_file():
            raise ValueError(f'Missing {name} in extracted DOS runtime')
    # Copy a user-selected, already extracted runtime, including original drivers
    shutil.copytree(source, output / 'game')
    game = output / 'game'
    captures = output / 'captures'
    captures.mkdir()
    cfg = game / config_name
    if not cfg.exists():
        bundled = game / '__support/app' / config_name
        if not bundled.exists():
            raise ValueError(f'Missing original game config: {config_name}')
        shutil.copy2(bundled, cfg)
    original = cfg.read_text(encoding='ascii')
    (output / 'original-game.cfg').write_text(original, encoding='ascii')
    device, port = ('0xa001', '0x330') if music_device == 'general-midi' else ('0xa009', '0x388')
    settings = [('MidiDeviceID', device), ('MidiPort', port), ('MidiVolume', '8')]
    if mute_effects:
        settings.append(('DigiVolume', '0'))
    for name, value in settings:
        original, count = re.subn(rf'(?m)^{name}=.*$', f'{name}={value}', original)
        if count != 1:
            raise ValueError(f'Expected one {name} setting in {config_name}')
    cfg.write_text(original, encoding='ascii')
    config = f'''[sdl]
fullscreen=false
output=surface
autolock=false
[dosbox]
captures={captures}
memsize=16
[cpu]
core=auto
cycles=50000
[midi]
mpu401=intelligent
mididevice=default
[sblaster]
sbtype=sb16
sbbase=220
irq=5
dma=1
hdma=5
[autoexec]
@echo off
mount c "{game}"
c:
echo Press {'Ctrl-Alt-F7 for OPL' if opl_capture else 'Ctrl-Alt-F8 for MIDI' if music_device == 'general-midi' else 'Ctrl-F6 for WAV'}, then Space to launch
pause
{executable}
exit
'''
    (output / 'capture.conf').write_text(config, encoding='ascii')
    provenance = {}
    for path in sorted(game.iterdir()):
        if path.is_file() and path.suffix.lower() in ('.exe', '.386', '.hog', '.cfg', '.plr'):
            provenance[path.name] = {'bytes': path.stat().st_size, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
    dosbox = game / 'DOSBOX/DOSBox.exe'
    provenance['DOSBOX/DOSBox.exe'] = {'sha256': hashlib.sha256(dosbox.read_bytes()).hexdigest()}
    (output / 'provenance.json').write_text(json.dumps(provenance, indent=2) + '\n', encoding='utf8')
    if song:
        data = extract_member(game / hog_name, song)
        (output / song).write_bytes(data)
        (output / 'song.json').write_text(json.dumps({'hog': hog_name, 'song': song,
                                                    'sha256': hashlib.sha256(data).hexdigest()}, indent=2) + '\n', encoding='utf8')
    print(f'Prepared {output}')
    shortcut = 'Ctrl+Alt+F7' if opl_capture else 'Ctrl+Alt+F8' if music_device == 'general-midi' else 'Ctrl+F6'
    print(f'Arm {shortcut} at the pause; stop with the same shortcut before exiting')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True, help='Extracted GOG DOS runtime, containing DOSBOX/')
    parser.add_argument('--output', type=Path, required=True, help='New private capture directory')
    parser.add_argument('--exe', default='DESCENTR.EXE')
    parser.add_argument('--config', default='DESCENT.CFG')
    parser.add_argument('--song', help='Also extract an HMP/HMQ member for the MIDI comparison; does not select it in game')
    parser.add_argument('--hog', default='DESCENT.HOG')
    parser.add_argument('--music-device', choices=('general-midi', 'adlib'), default='general-midi',
                        help='General MIDI capture or the GOG D1 AdLib/FM device (0xa009)')
    parser.add_argument('--mute-effects', action='store_true', help='Set private game effects volume to zero for clean music WAVs')
    parser.add_argument('--opl-capture', action='store_true', help='Prepare for Ctrl+Alt+F7 DRO register capture (requires --music-device adlib)')
    args = parser.parse_args()
    if Path(args.exe).name != args.exe or not re.fullmatch(r'[A-Za-z0-9_.]+', args.exe):
        parser.error('--exe must be a DOS executable basename')
    if Path(args.config).name != args.config:
        parser.error('--config must be a basename')
    if Path(args.hog).name != args.hog or (args.song and (Path(args.song).name != args.song or Path(args.song).suffix.lower() not in ('.hmp', '.hmq'))):
        parser.error('--hog and --song must be basenames; song must be HMP/HMQ')
    prepare(args.source, args.output, args.exe, args.config, args.song, args.hog,
            args.music_device, args.mute_effects, args.opl_capture)


if __name__ == '__main__':
    main()
