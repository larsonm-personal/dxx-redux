"""Installed debug-app integration: import, reject, select, preview and optionally D1/D2.

Requires an emulator with game data already imported. Uses the setup automation
API and restores the original soundfont manifest after the test. Game tests use
the existing music-control runner and its normal pilot/settings fixtures.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time
from music_test_preferences import save_preferences, restore_preferences

PACKAGE = 'com.dxxredux.app'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--adb', default='adb')
    parser.add_argument('--serial', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--games', action='store_true', help='Also run existing music controls in both games (Windows/pwsh)')
    parser.add_argument('--library', action='store_true', help='Also verify saved download Info and confirmed deletion in the UI')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    adb = [args.adb, '-s', args.serial]

    def call(*parts, check=True):
        return subprocess.run([*adb, *parts], check=check, capture_output=True, text=True, timeout=60)

    def start_setup():
        call('shell', 'am', 'start', '-n', PACKAGE + '/.SetupActivity')
        return state()

    def state():
        call('shell', 'run-as', PACKAGE, 'rm', '-f', 'files/setup_introspect.json')
        deadline = time.monotonic() + 30
        while time.monotonic() < deadline:
            # Cold startup may not have registered its receiver for the first request
            call('shell', 'am', 'broadcast', '-a', 'com.dxxredux.SETUP_INTROSPECT')
            data = call('shell', 'run-as', PACKAGE, 'cat', 'files/setup_introspect.json', check=False)
            if data.returncode == 0:
                try:
                    value = json.loads(data.stdout)
                    if 'soundfont' in value:
                        return value
                except json.JSONDecodeError:
                    pass
            time.sleep(0.5)
        raise AssertionError('No complete launcher state')

    def command(name, *extras, failure=False):
        response = call('shell', 'am', 'broadcast', '-a', 'com.dxxredux.SETUP_COMMAND', '--es', 'command', name, *extras).stdout
        expected = 'result=1' if failure else 'result=0'
        assert expected in response, response

    def stage(path, name):
        call('push', str(path), '/data/local/tmp/' + name)
        call('shell', 'run-as', PACKAGE, 'cp', '/data/local/tmp/' + name, 'files/' + name)
        call('shell', 'rm', '-f', '/data/local/tmp/' + name)

    if args.library:
        assert int(call('shell', 'getprop', 'ro.build.version.sdk').stdout.strip()) >= 29, 'Dialog automation requires an API 29+ emulator'
    call('shell', 'am', 'force-stop', PACKAGE)
    original = call('shell', 'run-as', PACKAGE, 'cat', 'files/soundfonts/selection.json', check=False)
    original_preferences = save_preferences(call, args.output)
    existing = json.loads(original.stdout) if original.returncode == 0 else {'fonts': []}
    # Change only a preset display name to give the fixture a distinct content hash
    data = bytearray(Path('android/app/src/main/assets/gm.sf2').read_bytes())
    header = data.index(b'phdr') + 8
    data[header:header + 20] = (b'Library test piano' if args.library else b'Profile test piano').ljust(20, b'\0')
    fixture = args.output / 'profile-test.sf2'
    fixture.write_bytes(data)
    identity = hashlib.sha256(data).hexdigest()
    if args.library:
        assert not any(f['id'] == identity for f in existing['fonts']), 'Deletion fixture already exists; refusing to delete a preexisting asset'
    invalid = args.output / 'invalid.sf2'
    invalid.write_bytes(b'not a soundfont')
    report = {'fixture_sha256': identity, 'checks': [], 'games': {}}
    try:
        start_setup()
        command('music_renderer_select', '--es', 'renderer', 'sf2')
        command('music_effects_select', '--ez', 'reverb', 'false', '--ez', 'chorus', 'true')
        assert state()['soundfont']['reverb'] is False and state()['soundfont']['chorus'] is True
        stage(fixture, 'soundfont-profile-test.sf2')
        stage(invalid, 'soundfont-invalid-test.sf2')
        base = '/data/user/0/' + PACKAGE + '/files/'
        command('music_soundfont_import', '--es', 'path', base + 'soundfont-profile-test.sf2')
        assert state()['soundfont']['selected'] == identity
        report['checks'].append('valid import activates and persists')
        command('music_soundfont_import', '--es', 'path', base + 'soundfont-invalid-test.sf2', failure=True)
        assert state()['soundfont']['selected'] == identity
        report['checks'].append('invalid import preserves active selection')
        call('shell', 'am', 'force-stop', PACKAGE)
        start_setup()
        assert state()['soundfont']['selected'] == identity
        report['checks'].append('selection survives process restart')
        assert state()['soundfont']['reverb'] is False and state()['soundfont']['chorus'] is True
        report['checks'].append('independent effect preferences survive process restart')
        for preset in ('ORIGINAL', 'DEFAULTS'):
            command('music_preferences_reset', '--es', 'preset', preset)
            reset = state()['soundfont']
            assert reset['renderer'] == 'ymfm' and reset['selected'] == '' and reset['reverb'] and reset['chorus'], reset
            command('music_effects_select', '--ez', 'reverb', 'false', '--ez', 'chorus', 'false')
        report['checks'].append('both resets restore AdLib, bundled font and wet MIDI defaults')
        command('music_renderer_select', '--es', 'renderer', 'sf2')
        command('music_soundfont_select', '--es', 'id', identity)
        command('music_effects_select', '--ez', 'reverb', 'true', '--ez', 'chorus', 'true')
        command('music_midi_play', '--ei', 'source', '0', '--ei', 'track', '2')
        time.sleep(2)
        preview = state()['music_preview']['midi']
        assert preview['state'] == 'playing' and preview['position_ms'] > 500 and preview['renderer'] == 'sf2', preview
        command('music_soundfont_select')
        switched = state()
        assert switched['soundfont']['selected'] == ''
        assert switched['music_preview']['midi']['state'] == 'stopped'
        report['checks'].append('switch to bundled stops the previous preview safely')
        command('music_midi_play', '--ei', 'source', '0', '--ei', 'track', '2')
        time.sleep(1)
        bundled_preview = state()['music_preview']['midi']
        assert bundled_preview['state'] == 'playing' and bundled_preview['position_ms'] > 500, bundled_preview
        report['checks'].append('bundled font loads and MIDI preview advances')
        command('music_soundfont_select', '--es', 'id', identity)
        command('music_midi_play', '--ei', 'source', '0', '--ei', 'track', '2')
        time.sleep(1)
        assert state()['music_preview']['midi']['state'] == 'playing'
        report['checks'].append('custom font can be selected and previewed again')
        command('music_midi_stop')
        if args.games:
            # The gameplay fixture clears game preferences, including MIDI choices.
            # Select this test's profile after reset, before preview or game launch.
            script = Path('android/game_scripts/test_music_track_controls_unified.jsonc').read_text(encoding='utf8')
            reset = '{"action": "reset_state"},'
            assert script.count(reset) == 1, 'Expected one gameplay fixture reset'
            profile_steps = [
                {'action': 'setup_command', 'command': 'music_renderer_select', 'args': {'renderer': 'sf2'}},
                {'action': 'setup_command', 'command': 'music_soundfont_select', 'args': {'id': identity}},
                {'action': 'assert', 'expect': {'soundfont.renderer': 'sf2', 'soundfont.selected': identity}},
            ]
            game_script = args.output / 'test_soundfont_music_controls.jsonc'
            game_script.write_text(script.replace(reset, reset + '\n' + '\n'.join(
                json.dumps(step) + ',' for step in profile_steps)), encoding='utf8')
            environment = dict(os.environ, ANDROID_SERIAL=args.serial)
            for game in ('d1', 'd2'):
                call('logcat', '-c')
                with (args.output / f'{game}-automation.log').open('w', encoding='utf8') as log:
                    subprocess.run(['pwsh', '-NoProfile', '-File', 'android/helpers/run_test.ps1',
                                    '-ScriptName', str(game_script.resolve()), '-Game', game],
                                   stdout=log, stderr=subprocess.STDOUT, env=environment, check=True, timeout=360)
                logs = call('logcat', '-d', '-s', 'DXX-Soundfont:I', '*:S').stdout
                (args.output / f'{game}-soundfont.log').write_text(logs, encoding='utf8')
                assert f'/{identity}.sf2' in logs and 'Loaded path=' in logs, logs
                assert 'renderer=fluidsynth' in logs, logs
                report['games'][game] = 'music-control automation passed; native loader selected custom asset'
        if args.library:
            # Simulate a completed download's stored metadata absent from the production
            # catalog. Downloader-to-manifest persistence is covered on JVM.
            call('shell', 'am', 'force-stop', PACKAGE)
            metadata = {
                'name': 'Library metadata fixture',
                'url': 'https://example.invalid/font.sf2',
                'description': 'Saved metadata fixture',
                'websiteUrl': 'https://example.invalid/original',
                'license': 'Test fixture attribution (not a redistribution license)',
            }
            manifest = args.output / 'library-selection.json'
            manifest.write_text(json.dumps({'fonts': [{
                'id': identity, 'name': metadata['name'], 'download': metadata,
            }]}, indent=2) + '\n', encoding='utf8')
            stage(manifest, 'soundfont-library-selection.json')
            call('shell', 'run-as', PACKAGE, 'mv', 'files/soundfont-library-selection.json', 'files/soundfonts/selection.json')
            start_setup()
            command('music_renderer_select', '--es', 'renderer', 'sf2')
            command('music_soundfont_select', '--es', 'id', identity)
            assert state()['soundfont']['fonts'][0]['download'] == metadata
            report['checks'].append('download metadata survives restart independently of catalog')
            steps = [
                {'_info': {'games': ['d1']}},
                {'action': 'enter_launcher'},
                # The shared runner resets preferences before executing the script
                {'action': 'setup_command', 'command': 'music_renderer_select', 'args': {'renderer': 'sf2'}},
                {'action': 'setup_command', 'command': 'music_soundfont_select', 'args': {'id': identity}},
                {'action': 'assert', 'expect': {'soundfont.selected': identity, 'soundfont.renderer': 'sf2'}},
                {'action': 'setup_command', 'command': 'music_midi_play', 'args': {'source': 0, 'track': 2}},
                {'action': 'assert', 'expect': {'music_preview.midi.state': 'playing'}},
                {'action': 'tap_button', 'text': 'Game Preferences'},
                {'action': 'tap_button', 'text': 'Manage soundfonts'},
                {'action': 'tap_button', 'text': 'Info', 'exact': True},
                {'action': 'assert_button', 'text': metadata['websiteUrl'], 'enabled': True, 'timeout_ms': 10000},
                {'action': 'tap_button', 'text': 'Close', 'exact': True},
                {'action': 'tap_button', 'text': 'Delete', 'exact': True},
                {'action': 'tap_button', 'text': 'Cancel', 'exact': True},
                {'action': 'assert', 'expect': {'soundfont.selected': identity}},
                {'action': 'tap_button', 'text': 'Delete', 'exact': True},
                {'action': 'tap_button', 'text': 'Delete', 'exact': True, 'post_delay_ms': 1000},
                {'action': 'assert', 'expect': {'soundfont.selected': '', 'soundfont.renderer': 'sf2', 'music_preview.midi.state': 'stopped'}},
                {'action': 'tap_button', 'text': 'Close', 'exact': True},
                {'action': 'tap_button', 'text': '< Back'},
            ]
            library_script = args.output / 'test_soundfont_library.jsonc'
            library_script.write_text(json.dumps(steps, indent=2) + '\n', encoding='utf8')
            environment = dict(os.environ, ANDROID_SERIAL=args.serial)
            with (args.output / 'library-automation.log').open('w', encoding='utf8') as log:
                subprocess.run(['pwsh', '-NoProfile', '-File', 'android/helpers/run_test.ps1',
                                '-ScriptName', str(library_script.resolve()), '-Game', 'd1'],
                               stdout=log, stderr=subprocess.STDOUT, env=environment, check=True, timeout=360)
            # run_test stops the app on completion; reopening also verifies persistence
            restored = start_setup()['soundfont']
            assert restored['selected'] == '' and restored['renderer'] == 'sf2' and restored['fonts'] == []
            assert call('shell', 'run-as', PACKAGE, 'test', '-e', f'files/soundfonts/{identity}.sf2', check=False).returncode != 0
            report['checks'].append('Info source link available; cancel retains font; confirmed active deletion removes file and persists bundled fallback')
        report['passed'] = True
    finally:
        call('logcat', '-d', '-s', 'DXX-Soundfont:I', '*:S')
        call('shell', 'am', 'force-stop', PACKAGE)
        restore_preferences(call, original_preferences)
        if original.returncode == 0:
            backup = args.output / 'original-selection.json'
            backup.write_text(original.stdout, encoding='utf8')
            stage(backup, 'soundfont-selection-restore.json')
            call('shell', 'run-as', PACKAGE, 'mv', 'files/soundfont-selection-restore.json', 'files/soundfonts/selection.json')
        else:
            call('shell', 'run-as', PACKAGE, 'rm', '-f', 'files/soundfonts/selection.json')
        if not any(f['id'] == identity for f in existing['fonts']):
            call('shell', 'run-as', PACKAGE, 'rm', '-f', f'files/soundfonts/{identity}.sf2')
        call('shell', 'run-as', PACKAGE, 'rm', '-f', 'files/soundfont-profile-test.sf2', 'files/soundfont-invalid-test.sf2')
        (args.output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
