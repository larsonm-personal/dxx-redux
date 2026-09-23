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

PACKAGE = 'com.dxxredux.app'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--adb', default='adb')
    parser.add_argument('--serial', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--games', action='store_true', help='Also run existing music controls in both games (Windows/pwsh)')
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

    call('shell', 'am', 'force-stop', PACKAGE)
    original = call('shell', 'run-as', PACKAGE, 'cat', 'files/soundfonts/selection.json', check=False)
    existing = json.loads(original.stdout) if original.returncode == 0 else {'fonts': []}
    # Change only a preset display name to give the fixture a distinct content hash
    data = bytearray(Path('android/app/src/main/assets/gm.sf2').read_bytes())
    header = data.index(b'phdr') + 8
    data[header:header + 20] = b'Profile test piano\0\0'
    fixture = args.output / 'profile-test.sf2'
    fixture.write_bytes(data)
    identity = hashlib.sha256(data).hexdigest()
    invalid = args.output / 'invalid.sf2'
    invalid.write_bytes(b'not a soundfont')
    report = {'fixture_sha256': identity, 'checks': [], 'games': {}}
    try:
        start_setup()
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
        command('music_midi_play', '--ei', 'source', '0', '--ei', 'track', '2')
        time.sleep(2)
        preview = state()['music_preview']['midi']
        assert preview['state'] == 'playing' and preview['position_ms'] > 500, preview
        command('music_soundfont_select')
        switched = state()
        assert switched['soundfont']['selected'] == ''
        assert switched['music_preview']['midi']['state'] == 'stopped'
        report['checks'].append('switch to bundled stops the previous preview safely')
        command('music_soundfont_select', '--es', 'id', identity)
        command('music_midi_play', '--ei', 'source', '0', '--ei', 'track', '2')
        time.sleep(1)
        assert state()['music_preview']['midi']['state'] == 'playing'
        report['checks'].append('custom font can be selected and previewed again')
        command('music_midi_stop')
        if args.games:
            environment = dict(os.environ, ANDROID_SERIAL=args.serial)
            for game in ('d1', 'd2'):
                call('logcat', '-c')
                with (args.output / f'{game}-automation.log').open('w', encoding='utf8') as log:
                    subprocess.run(['pwsh', '-NoProfile', '-File', 'android/helpers/run_test.ps1',
                                    '-ScriptName', 'test_music_track_controls_unified.jsonc', '-Game', game],
                                   stdout=log, stderr=subprocess.STDOUT, env=environment, check=True, timeout=360)
                logs = call('logcat', '-d', '-s', 'DXX-Soundfont:I', '*:S').stdout
                (args.output / f'{game}-soundfont.log').write_text(logs, encoding='utf8')
                assert f'/{identity}.sf2' in logs and 'Loaded path=' in logs, logs
                report['games'][game] = 'music-control automation passed; native loader selected custom asset'
        report['passed'] = True
    finally:
        call('logcat', '-d', '-s', 'DXX-Soundfont:I', '*:S')
        call('shell', 'am', 'force-stop', PACKAGE)
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
