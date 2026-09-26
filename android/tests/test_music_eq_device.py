"""Installed-app EQ integration: selection, restart, D1/D2 preview, seek and FM bypass."""
import argparse
import json
from pathlib import Path
import subprocess
import shlex
import time

from music_test_preferences import save_preferences, restore_preferences, clear_midi_preferences

PACKAGE = 'com.dxxredux.app'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--adb', default='adb')
    parser.add_argument('--serial', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--ui-only', action='store_true', help='Exercise profile popups and capture their graphs without playback tests')
    parser.add_argument('--game-launch', action='store_true', help='Also verify native D1/D2 startup; requires launch-ready base data')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)

    def call(*parts, check=True):
        return subprocess.run([args.adb, '-s', args.serial, *parts], capture_output=True,
                              text=True, check=check, timeout=60)

    def command(name, *extras):
        result = call('shell', 'am', 'broadcast', '-a', 'com.dxxredux.SETUP_COMMAND', '--es', 'command', name, *extras)
        assert 'result=0' in result.stdout, result.stdout

    def state():
        call('shell', 'run-as', PACKAGE, 'rm', '-f', 'files/setup_introspect.json')
        call('shell', 'am', 'broadcast', '-a', 'com.dxxredux.SETUP_INTROSPECT')
        for _ in range(40):
            result = call('shell', 'run-as', PACKAGE, 'cat', 'files/setup_introspect.json', check=False)
            if result.returncode == 0:
                value = json.loads(result.stdout)
                if value.get('music_preview', {}).get('midi_catalog_complete'):
                    return value
            time.sleep(.25)
        raise AssertionError('Setup introspection timed out')

    def start():
        call('shell', 'run-as', PACKAGE, 'rm', '-f', 'files/setup_introspect.json')
        call('shell', 'am', 'start', '-n', PACKAGE + '/.SetupActivity')
        for _ in range(20):
            result = call('shell', 'am', 'broadcast', '-a', 'com.dxxredux.SETUP_INTROSPECT', '--ez', 'lightweight', 'true')
            result = call('shell', 'run-as', PACKAGE, 'cat', 'files/setup_introspect.json', check=False)
            if result.returncode == 0:
                return
            time.sleep(.5)
        raise AssertionError('Launcher did not start')

    def buttons():
        call('shell', 'run-as', PACKAGE, 'rm', '-f', 'files/setup_buttons.json')
        call('shell', 'am', 'broadcast', '-a', 'com.dxxredux.SETUP_INTROSPECT', '--ez', 'buttons_only', 'true')
        for _ in range(20):
            result = call('shell', 'run-as', PACKAGE, 'cat', 'files/setup_buttons.json', check=False)
            if result.returncode == 0:
                return json.loads(result.stdout)['buttons']
            time.sleep(.2)
        raise AssertionError('Button introspection timed out')

    def tap(text):
        call('shell', 'am', 'broadcast', '-a', 'com.dxxredux.MP_COMMAND', '--es', 'command', 'tap_button', '--es', 'text', shlex.quote(text))
        time.sleep(1)

    call('shell', 'am', 'force-stop', PACKAGE)
    original = save_preferences(call, args.output)
    results = []
    try:
        clear_midi_preferences(call, original, args.output)
        start()
        command('music_renderer_select', '--es', 'renderer', 'sf2')
        command('music_soundfont_select')
        assert state()['soundfont']['eq'] == 'measured-sc55-balanced'
        tap('Edit')
        tap('MIDI')
        eq_button = next(b['text'] for b in buttons() if b['text'].startswith('eq '))
        assert eq_button != 'eq opl3'
        tap(eq_button)
        assert any(b['text'] == 'Flat (no EQ)' for b in buttons())
        screenshot = subprocess.run([args.adb, '-s', args.serial, 'exec-out', 'screencap', '-p'], capture_output=True, check=True)
        (args.output / 'measured-popup.png').write_bytes(screenshot.stdout)
        tap('Balanced (1 octave)')
        tap('Broad (2 octaves)')
        assert state()['soundfont']['eq'] == 'measured-sc55-broad'
        tap('Flat (no EQ)')
        assert state()['soundfont']['eq'] == 'flat'
        tap('Close')
        command('music_renderer_select', '--es', 'renderer', 'ymfm')
        tap('eq opl3')
        assert not any('Measured EQ' in b['text'] for b in buttons())
        screenshot = subprocess.run([args.adb, '-s', args.serial, 'exec-out', 'screencap', '-p'], capture_output=True, check=True)
        (args.output / 'opl3-popup.png').write_bytes(screenshot.stdout)
        tap('Close')
        command('music_renderer_select', '--es', 'renderer', 'sf2')
        assert state()['soundfont']['eq'] == 'flat'
        results.append(dict(check='profile popup controls, default measured EQ, smoothing and profile isolation', passed=True))
        if args.ui_only:
            print('Profile EQ popup integration passed', flush=True)
            return
        sources = state()['music_preview']['midi_sources']
        for source_id in ['d1-builtin', 'd2-builtin']:
            source_index, source = next((i, s) for i, s in enumerate(sources) if s['id'] == source_id)
            for preset, native in [('measured-sc55-detail', 1), ('measured-sc55-balanced', 2), ('measured-sc55-broad', 3), ('flat', 0)]:
                command('music_eq_select', '--es', 'preset', preset)
                command('music_midi_play', '--ei', 'source', str(source_index), '--ei', 'track', str(source['tracks'].index('game01.hmp')))
                time.sleep(1)
                snapshot = state()
                assert snapshot['soundfont']['eq'] == preset
                assert snapshot['soundfont']['eq_effective_native'] == native, snapshot['soundfont']
                assert snapshot['music_preview']['midi']['state'] == 'playing', snapshot['music_preview']['midi']
                command('music_midi_seek', '--ef', 'fraction', '.5')
                command('music_midi_pause')
                assert state()['music_preview']['midi']['state'] == 'paused'
                command('music_midi_resume')
                assert state()['soundfont']['eq_effective_native'] == native
                command('music_midi_stop')
                results.append(dict(source=source_id, preset=preset, effective=native, passed=True))
                print(source_id, preset, 'passed', flush=True)
        command('music_eq_select', '--es', 'preset', 'measured-sc55-balanced')
        call('shell', 'am', 'force-stop', PACKAGE)
        start()
        assert state()['soundfont']['eq'] == 'measured-sc55-balanced'
        command('music_renderer_select', '--es', 'renderer', 'ymfm')
        source_index, source = next((i, s) for i, s in enumerate(sources) if s['id'] == 'd1-builtin')
        command('music_midi_play', '--ei', 'source', str(source_index), '--ei', 'track', str(source['tracks'].index('game01.hmp')))
        snapshot = state()
        assert snapshot['music_preview']['midi']['renderer'] == 'ymfm'
        assert snapshot['soundfont']['eq'] == 'flat'
        assert snapshot['soundfont']['eq_effective_native'] == 0
        results.append(dict(check='restart persistence and FM bypass', passed=True))
        command('music_midi_stop')
        command('music_renderer_select', '--es', 'renderer', 'sf2')
        assert state()['soundfont']['eq'] == 'measured-sc55-balanced'
        results.append(dict(check='OPL3 and bundled profile isolation', passed=True))
        if args.game_launch:
            command('music_midi_stop')
            command('music_renderer_select', '--es', 'renderer', 'sf2')
            for game in ['d1', 'd2']:
                assert state()[game]['ready'], f'{game} data is not launch-ready'
                call('shell', 'run-as', PACKAGE, 'rm', '-f', 'files/introspect.json')
                command('launch', '--es', 'game', game)
                audio = {}
                for _ in range(60):
                    call('shell', 'am', 'broadcast', '-a', 'com.dxxredux.INTROSPECT')
                    time.sleep(1)
                    response = call('shell', 'run-as', PACKAGE, 'cat', 'files/introspect.json', check=False)
                    if response.returncode == 0:
                        audio = json.loads(response.stdout).get('audio', {})
                        if audio.get('tsf_equalizer') == 2 and audio.get('tsf_cb_count', 0) > 0:
                            break
                assert audio.get('tsf_equalizer') == 2 and audio.get('tsf_cb_count', 0) > 0, audio
                results.append(dict(check=f'{game} native game startup', audio=audio, passed=True))
                print(game, 'native EQ playback passed', flush=True)
                call('shell', 'am', 'force-stop', PACKAGE)
                start()
    finally:
        call('shell', 'am', 'force-stop', PACKAGE)
        restore_preferences(call, original)
        (args.output / 'report.json').write_text(json.dumps(results, indent=2) + '\n', encoding='utf8')
    print('Installed-app EQ integration passed', flush=True)


if __name__ == '__main__':
    main()
