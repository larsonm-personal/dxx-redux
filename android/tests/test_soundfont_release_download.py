"""Download a catalog release through the Android UI, inspect Info and preview MIDI.

Requires an API 29+ debug emulator with D2 data. Restores the original soundfont
manifest and MIDI preferences. Supply the name, hash and URLs to test another
enabled catalog entry. Uses the normal launcher runner's game-state reset.
"""

import argparse
import json
import os
from pathlib import Path
import re
import subprocess

from music_test_preferences import save_preferences, restore_preferences

PACKAGE = 'com.dxxredux.app'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--adb', default='adb')
    parser.add_argument('--serial', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--name', default='GeneralUser GS 2.0.3 beta')
    parser.add_argument('--sha256', default='9575028c7a1f589f5770fccc8cff2734566af40cd26ed836944e9a5152688cfe')
    parser.add_argument('--url', default='https://github.com/krtw00/codetta/releases/download/soundfont-bundle/GeneralUser-GS.sf2')
    parser.add_argument('--website', default='https://github.com/mrbumpy409/GeneralUser-GS')
    args = parser.parse_args()
    assert re.fullmatch('[0-9a-f]{64}', args.sha256), 'Expected a lowercase SHA-256'
    args.output.mkdir(parents=True, exist_ok=True)

    def call(*parts, check=True):
        return subprocess.run([args.adb, '-s', args.serial, *parts], check=check,
                              capture_output=True, text=True, timeout=60)

    def publish_manifest(contents):
        path = args.output / 'selection-to-publish.json'
        path.write_text(contents, encoding='utf8')
        remote = '/data/local/tmp/soundfont-release-selection.json'
        call('push', str(path), remote)
        call('shell', 'run-as', PACKAGE, 'mkdir', '-p', 'files/soundfonts')
        call('shell', 'run-as', PACKAGE, 'cp', remote, 'files/soundfonts/selection.json')
        call('shell', 'rm', '-f', remote)

    assert int(call('shell', 'getprop', 'ro.build.version.sdk').stdout.strip()) >= 29
    call('shell', 'am', 'force-stop', PACKAGE)
    original = call('shell', 'run-as', PACKAGE, 'cat', 'files/soundfonts/selection.json', check=False)
    original_fonts = json.loads(original.stdout)['fonts'] if original.returncode == 0 else []
    original_preferences = save_preferences(call, args.output)
    report = {'name': args.name, 'sha256': args.sha256, 'url': args.url, 'passed': False}
    try:
        # Isolate the visible list so Info targets this bank; existing files stay intact
        publish_manifest('{"fonts": []}\n')
        steps = [
            {'_info': {'games': ['d2']}},
            {'action': 'enter_launcher'},
            {'action': 'tap_button', 'text': 'Game Preferences'},
            {'action': 'tap_button', 'text': 'Download soundfonts'},
            {'action': 'tap_button', 'text': args.name},
            {'action': 'assert_button', 'text': args.website, 'enabled': True},
            {'action': 'tap_button', 'text': 'Cancel', 'exact': True},
            {'action': 'assert', 'expect': {'soundfont.selected': '', 'soundfont.renderer': 'ymfm'}},
            {'action': 'tap_button', 'text': 'Download soundfonts'},
            {'action': 'tap_button', 'text': args.name},
            {'action': 'tap_button', 'text': 'Download', 'exact': True},
            {'action': 'wait_for', 'field': 'soundfont.selected', 'value': args.sha256, 'timeout_ms': 180000},
            {'action': 'assert', 'expect': {
                'soundfont.renderer': 'ymfm',
                'soundfont.fonts[0].download.url': args.url,
                'soundfont.fonts[0].download.websiteUrl': args.website,
                'soundfont.fonts[0].download.name': args.name,
            }},
            {'action': 'tap_button', 'text': 'Manage soundfonts'},
            {'action': 'tap_button', 'text': 'Info', 'exact': True},
            {'action': 'assert_button', 'text': args.website, 'enabled': True},
            {'action': 'tap_button', 'text': 'Close', 'exact': True},
            {'action': 'tap_button', 'text': 'Close', 'exact': True},
            {'action': 'tap_button', 'text': '< Back'},
            {'action': 'setup_command', 'command': 'music_renderer_select', 'args': {'renderer': 'sf2'}},
            {'action': 'setup_command', 'command': 'music_midi_play', 'args': {'source': 0, 'track': 2}, 'post_delay_ms': 2000},
            {'action': 'assert', 'expect': {
                'soundfont.selected': args.sha256,
                'music_preview.midi.state': 'playing',
                'music_preview.midi.renderer': 'sf2',
                'music_preview.midi.position_ms': {'gt': 500},
            }},
            {'action': 'setup_command', 'command': 'music_midi_stop'},
        ]
        script = args.output / 'test_soundfont_release_download.jsonc'
        script.write_text(json.dumps(steps, indent=2) + '\n', encoding='utf8')
        call('logcat', '-c')
        with (args.output / 'automation.log').open('w', encoding='utf8') as log:
            subprocess.run(['pwsh', '-NoProfile', '-File', 'android/helpers/run_test.ps1',
                            '-ScriptName', str(script.resolve()), '-Game', 'd2'],
                           stdout=log, stderr=subprocess.STDOUT,
                           env=dict(os.environ, ANDROID_SERIAL=args.serial), check=True, timeout=600)
        manifest = json.loads(call('shell', 'run-as', PACKAGE, 'cat', 'files/soundfonts/selection.json').stdout)
        font = manifest['fonts'][0]
        assert font['id'] == args.sha256 and font['download']['url'] == args.url
        assert font['download']['license'] and font['download']['description']
        digest = call('shell', 'run-as', PACKAGE, 'sha256sum', f'files/soundfonts/{args.sha256}.sf2').stdout.split()[0]
        assert digest == args.sha256
        report.update(passed=True, manifest=manifest)
    finally:
        logs = call('logcat', '-d', '-s', 'DXX-Soundfont:I', 'DXX-MidiPreview:I', '*:S').stdout
        (args.output / 'native.log').write_text(logs, encoding='utf8')
        call('shell', 'am', 'force-stop', PACKAGE)
        restore_preferences(call, original_preferences)
        if original.returncode == 0:
            publish_manifest(original.stdout)
        else:
            call('shell', 'run-as', PACKAGE, 'rm', '-f', 'files/soundfonts/selection.json')
        if not any(font['id'] == args.sha256 for font in original_fonts):
            call('shell', 'run-as', PACKAGE, 'rm', '-f', f'files/soundfonts/{args.sha256}.sf2')
        (args.output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    print(json.dumps({key: value for key, value in report.items() if key != 'manifest'}, indent=2))


if __name__ == '__main__':
    main()
