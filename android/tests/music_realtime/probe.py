"""Measure installed-app MIDI/FM startup and sustained output from native logs.

Uses the same native preview as the music editor. Requires imported D1/D2 data
and a debug APK. Restores game preferences; does not change imported assets.
"""
import argparse
import json
import math
from pathlib import Path
import re
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from music_test_preferences import save_preferences, restore_preferences

PACKAGE = 'com.dxxredux.app'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--adb', default='adb')
    parser.add_argument('--serial', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--seconds', type=int, default=45)
    parser.add_argument('--renderer', choices=['both', 'ymfm', 'sf2'], default='both')
    parser.add_argument('--source', default='d1-builtin')
    parser.add_argument('--measure-only', action='store_true', help='Record a failing baseline without a nonzero exit')
    parser.add_argument('--max-render-ratio', type=float,
                        help='Optional elapsed render seconds per produced audio second budget (includes scheduling delays)')
    args = parser.parse_args()
    if args.seconds < 35:
        parser.error('--seconds must be at least 35 to reach game01\'s dense section')
    if args.max_render_ratio is not None and (
            not math.isfinite(args.max_render_ratio) or args.max_render_ratio <= 0):
        parser.error('--max-render-ratio must be finite and positive')
    args.output.mkdir(parents=True, exist_ok=True)

    def call(*parts, check=True):
        return subprocess.run([args.adb, '-s', args.serial, *parts],
                              check=check, capture_output=True, text=True, timeout=60)

    def command(name, *extras):
        result = call('shell', 'am', 'broadcast', '-a', 'com.dxxredux.SETUP_COMMAND',
                      '--es', 'command', name, *extras)
        assert 'result=0' in result.stdout, result.stdout

    def state():
        for _ in range(20):
            call('shell', 'run-as', PACKAGE, 'rm', '-f', 'files/setup_introspect.json')
            call('shell', 'am', 'broadcast', '-a', 'com.dxxredux.SETUP_INTROSPECT')
            snapshot = call('shell', 'run-as', PACKAGE, 'cat', 'files/setup_introspect.json', check=False)
            if snapshot.returncode == 0:
                value = json.loads(snapshot.stdout)
                if value.get('music_preview', {}).get('midi_catalog_complete'):
                    return value
            time.sleep(.5)
        raise AssertionError('MIDI catalog did not become available')

    call('shell', 'am', 'force-stop', PACKAGE)
    original = save_preferences(call, args.output)
    original_timing = call('shell', 'getprop', 'debug.dxx.music_timing').stdout.strip()
    report = []
    try:
        call('shell', 'setprop', 'debug.dxx.music_timing', '1')
        call('shell', 'am', 'start', '-n', PACKAGE + '/.SetupActivity')
        sources = state()['music_preview']['midi_sources']
        index, source = next((i, s) for i, s in enumerate(sources) if s['id'] == args.source)
        renderers = ('ymfm', 'sf2') if args.renderer == 'both' else (args.renderer,)
        for renderer in renderers:
            command('music_renderer_select', '--es', 'renderer', renderer)
            command('music_soundfont_select')
            command('music_effects_select', '--ez', 'reverb', 'true', '--ez', 'chorus', 'true')
            songs = ('descent.hmp', 'briefing.hmp', 'game01.hmp') if renderer == 'ymfm' else ('game01.hmp',)
            for song in songs:
                call('logcat', '-c')
                before = time.monotonic()
                command('music_midi_play', '--ei', 'source', str(index), '--ei', 'track', str(source['tracks'].index(song)))
                reply_ms = (time.monotonic() - before) * 1000
                print(f'{renderer} {song}: running sustained playback', flush=True)
                time.sleep(args.seconds if song == 'game01.hmp' else 5)
                logs = call('logcat', '-d', '-v', 'epoch', '-s', 'DXX-MidiPreview:I', 'DXX-MusicSynth:I', 'DXX-Soundfont:I', 'DXX-DLOG:D', '*:S').stdout
                (args.output / f'{renderer}-{song}.log').write_text(logs, encoding='utf8')
                samples = []
                for line in logs.splitlines():
                    if 'Progress: ' not in line:
                        continue
                    sample = {'wall_time': float(line.split()[0])}
                    for key, value in re.findall(r'(\w+)=([\w.]+)', line.split('Progress: ', 1)[1]):
                        sample[key] = value if key == 'renderer' else float(value)
                    samples.append(sample)
                result = {'renderer': renderer, 'song': song, 'command_reply_ms': reply_ms, 'samples': samples}
                failures = []
                if len(samples) >= 2:
                    first, last = samples[0], samples[-1]
                    expected_renderer = 'fluidsynth' if renderer == 'sf2' else 'ymfm'
                    if any(s['renderer'] != expected_renderer for s in samples):
                        failures.append('The selected renderer was not used')
                    seconds = last['wall_time'] - first['wall_time']
                    result['consumed_frames_per_second'] = (last['consumed_frames'] - first['consumed_frames']) / seconds
                    result['underruns_after_first_report'] = last['underruns'] - first['underruns']
                    result['forward_progress'] = all(b['consumed_frames'] > a['consumed_frames'] for a, b in zip(samples, samples[1:]))
                    result['first_audio_ms'] = last['first_audio_ms']
                    result['max_render_ms'] = max(s['max_render_ms'] for s in samples)
                    audio_seconds = (last['rendered_frames'] - first['rendered_frames']) / 48000
                    result['render_wall_seconds_per_audio_second'] = (
                        sum(s['render_ms'] for s in samples[1:]) / 1000 / audio_seconds
                        if audio_seconds > 0 else None)
                    if args.max_render_ratio is not None and (
                            result['render_wall_seconds_per_audio_second'] is None or
                            result['render_wall_seconds_per_audio_second'] > args.max_render_ratio):
                        failures.append('Elapsed rendering exceeded the requested headroom budget')
                    if not result['forward_progress']:
                        failures.append('Audio consumption stopped making forward progress')
                    if not 47000 <= result['consumed_frames_per_second'] <= 49000:
                        failures.append('Audio consumption failed to sustain 48 kHz')
                    if result['underruns_after_first_report'] > 2:
                        failures.append('Sustained playback starved the audio queue')
                    if not 0 < result['first_audio_ms'] < 1500:
                        failures.append('First nonzero audio exceeded 1.5 seconds (including authored silence)')
                    if seconds < (args.seconds - 3 if song == 'game01.hmp' else 2):
                        failures.append('Progress reports ended before the requested test duration')
                else:
                    failures.append('Missing native progress diagnostics')
                result['failures'] = failures
                report.append(result)
                command('music_midi_stop')
    finally:
        call('shell', 'am', 'force-stop', PACKAGE)
        restore_preferences(call, original)
        call('shell', 'setprop', 'debug.dxx.music_timing', original_timing or '0')
        (args.output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    print(json.dumps([{k: v for k, v in entry.items() if k != 'samples'} for entry in report], indent=2))
    if not args.measure_only and any(entry['failures'] for entry in report):
        raise SystemExit(1)


if __name__ == '__main__':
    main()
