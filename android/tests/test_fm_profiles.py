"""Installed-app FM integration: selection, preview, seek, restart, D1/D2 gameplay.

Uses the setup automation API, restores the selected profile afterwards, and
runs the shared gameplay controls test with its normal pilot/settings fixtures.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import time
from music_test_preferences import save_preferences, restore_preferences, clear_midi_preferences

PACKAGE = 'com.dxxredux.app'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--adb', default='adb')
    parser.add_argument('--serial', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--games', action='store_true')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    adb = [args.adb, '-s', args.serial]

    def call(*parts, check=True):
        return subprocess.run([*adb, *parts], check=check, capture_output=True, text=True, timeout=60)

    def state():
        call('shell', 'run-as', PACKAGE, 'rm', '-f', 'files/setup_introspect.json')
        deadline = time.monotonic() + 30
        while time.monotonic() < deadline:
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

    def start_setup():
        call('shell', 'am', 'start', '-n', PACKAGE + '/.SetupActivity')
        return state()

    def command(name, *extras):
        response = call('shell', 'am', 'broadcast', '-a', 'com.dxxredux.SETUP_COMMAND', '--es', 'command', name, *extras).stdout
        assert 'result=0' in response, response

    call('shell', 'am', 'force-stop', PACKAGE)
    original = call('shell', 'run-as', PACKAGE, 'cat', 'files/soundfonts/selection.json', check=False)
    original_preferences = save_preferences(call, args.output)
    report = {'checks': [], 'games': {}, 'passed': False}
    try:
        clear_midi_preferences(call, original_preferences, args.output)
        initial = start_setup()
        assert initial['soundfont']['renderer'] == 'ymfm', initial['soundfont']
        report['checks'].append('Fresh game preferences default to AdLib')
        command('music_renderer_select', '--es', 'renderer', 'ymfm')
        call('shell', 'am', 'force-stop', PACKAGE)
        current = start_setup()
        assert current['soundfont']['renderer'] == 'ymfm'
        assert current['soundfont']['selected'] == initial['soundfont']['selected']
        report['checks'].append('FM selection survives restart and retains SF2 selection')
        sources = current['music_preview']['midi_sources']
        source = next(i for i, s in enumerate(sources) if s['id'] == 'd1-builtin')
        track = sources[source]['tracks'].index('game07.hmp')

        def play():
            command('music_midi_play', '--ei', 'source', str(source), '--ei', 'track', str(track))

        play()
        preview = state()['music_preview']['midi']
        assert preview['state'] == 'playing' and preview['renderer'] == 'ymfm', preview
        assert preview['position_ms'] > 0 and preview['duration_ms'] > 20000, preview
        command('music_midi_seek', '--ef', 'fraction', '0.75')
        command('music_midi_pause')
        preview = state()['music_preview']['midi']
        assert preview['state'] == 'paused' and preview['renderer'] == 'ymfm', preview
        assert preview['position_ms'] >= preview['duration_ms'] * 0.7, preview
        command('music_midi_resume')
        assert state()['music_preview']['midi']['state'] == 'playing'
        report['checks'].append('Level 7 preview uses FM and supports seek/pause/resume')
        command('music_renderer_select', '--es', 'renderer', 'sf2')
        assert state()['music_preview']['midi']['state'] == 'stopped'
        play()
        assert state()['music_preview']['midi']['renderer'] == 'sf2'
        command('music_renderer_select', '--es', 'renderer', 'ymfm')
        play()
        assert state()['music_preview']['midi']['renderer'] == 'ymfm'
        command('music_midi_stop')
        report['checks'].append('Switching renderers stops old playback; both can restart')
        for preset in ('ORIGINAL', 'DEFAULTS'):
            command('music_renderer_select', '--es', 'renderer', 'sf2')
            play()
            fonts = state()['soundfont']['fonts']
            command('music_preferences_reset', '--es', 'preset', preset)
            reset = state()
            assert reset['soundfont']['renderer'] == 'ymfm'
            assert reset['soundfont']['selected'] == ''
            assert reset['soundfont']['fonts'] == fonts
            assert reset['music_preview']['midi']['state'] == 'stopped'
            call('shell', 'am', 'force-stop', PACKAGE)
            assert start_setup()['soundfont']['renderer'] == 'ymfm'
            play()
            assert state()['music_preview']['midi']['renderer'] == 'ymfm'
            command('music_midi_stop')
        report['checks'].append('Both preset resets persist AdLib, retain imported fonts and restart FM preview')
        call('logcat', '-c')
        for name in ('game08.hmp', 'descent.hmp', 'game01.hmp'):
            track = sources[source]['tracks'].index(name)
            play()
            preview = state()['music_preview']['midi']
            assert preview['state'] == 'playing' and preview['renderer'] == 'ymfm', (name, preview)
            command('music_midi_stop')
        logs = call('logcat', '-d', '-s', 'DXX-MusicSynth:I', '*:S').stdout
        (args.output / 'hmq-preview-renderer.log').write_text(logs, encoding='utf8')
        for name in ('game08', 'descent', 'game01'):
            assert f'sequence={name}.hmq' in logs, logs
        report['checks'].append('Level 8, title and Level 1 previews resolve HMQ and use FM')
        if args.games:
            environment = dict(os.environ, ANDROID_SERIAL=args.serial)
            for game in ('d1', 'd2'):
                call('logcat', '-c')
                with (args.output / f'{game}-automation.log').open('w', encoding='utf8') as log:
                    subprocess.run(['pwsh', '-NoProfile', '-File', 'android/helpers/run_test.ps1',
                                    '-ScriptName', 'test_music_track_controls_unified.jsonc', '-Game', game],
                                   stdout=log, stderr=subprocess.STDOUT, env=environment, check=True, timeout=360)
                logs = call('logcat', '-d', '-s', 'DXX-MusicSynth:I', '*:S').stdout
                (args.output / f'{game}-renderer.log').write_text(logs, encoding='utf8')
                if game == 'd1':
                    assert 'renderer=ymfm song=game07.hmp melodic=melodic.bnk drums=drum.bnk' in logs, logs
                    assert 'sequence=game08.hmq' in logs, logs
                else:
                    assert 'renderer=ymfm song=game01.hmp melodic=d2melod.bnk drums=d2drums.bnk' in logs, logs
                    assert 'sequence=game01.hmq' in logs, logs
                report['games'][game] = 'passed with confirmed native renderer'
        report['passed'] = True
    finally:
        call('shell', 'am', 'force-stop', PACKAGE)
        restore_preferences(call, original_preferences)
        if original.returncode == 0:
            backup = args.output / 'original-selection.json'
            backup.write_text(original.stdout, encoding='utf8')
            call('push', str(backup), '/data/local/tmp/fm-selection-restore.json')
            call('shell', 'run-as', PACKAGE, 'cp', '/data/local/tmp/fm-selection-restore.json', 'files/soundfonts/selection.json')
            call('shell', 'rm', '-f', '/data/local/tmp/fm-selection-restore.json')
        else:
            call('shell', 'run-as', PACKAGE, 'rm', '-f', 'files/soundfonts/selection.json')
        (args.output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
