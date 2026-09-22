"""Run a hash-verified DOS capture against the compiled production converter."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
from midi_diff import compare


def run(exporter, fixture, reference, output):
    spec = json.loads(fixture.read_text(encoding='utf8'))
    output.mkdir(parents=True, exist_ok=True)
    for key in ('hmp', 'capture'):
        path = reference / spec[key]
        if hashlib.sha256(path.read_bytes()).hexdigest() != spec[key + '_sha256']:
            raise ValueError(f'{key} hash does not match reference manifest')
    reports = {}
    modes = ['repeat', 'legacy']
    if spec.get('device_selection_negative_control'):
        modes.append('all-devices')
    for mode in modes:
        target = output / f'{mode}.mid'
        source = reference / spec['hmp']
        if mode == 'all-devices':
            data = bytearray(source.read_bytes())
            data[0x90:0x2fc] = bytes(0x2fc - 0x90)
            source = output / 'negative-control-all-devices.hmp'
            source.write_bytes(data)
        playback = mode != 'legacy'
        result = subprocess.run([str(exporter), str(source), str(target), 'repeat' if playback else 'legacy'],
                                capture_output=True, text=True, check=True)
        reports[mode] = compare(reference / spec['capture'], Path(str(target) + '.tml.mid'),
                                reference_start=spec['reference_start_ms'],
                                duration=spec['duration_ms'] if playback else spec['legacy_duration_ms'],
                                tolerance=spec['tolerance_ms'], candidate_initial_state=playback,
                                shared_initial_state=playback)
        reports[mode]['converter'] = json.loads(result.stdout)
        (output / f'{mode}-diff.json').write_text(json.dumps(reports[mode], indent=2) + '\n', encoding='utf8')
    passed = reports['repeat']['passed'] and not reports['legacy']['passed']
    if 'all-devices' in reports:
        passed &= not reports['all-devices']['passed']
    summary = {'passed': passed, 'production_matches': reports['repeat']['passed'],
               'legacy_negative_control_fails': not reports['legacy']['passed'],
               'matched_channel_messages': sum(c['reference_events'] for c in reports['repeat']['channels']),
               'max_time_error_ms': max(c['max_time_error_ms'] for c in reports['repeat']['channels'])}
    if 'all-devices' in reports:
        summary['device_selection_negative_control_fails'] = not reports['all-devices']['passed']
    (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf8')
    print(json.dumps(summary, indent=2))
    return passed


if __name__ == '__main__':
    repo = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exporter', type=Path, required=True)
    parser.add_argument('--fixture', type=Path, default=Path(__file__).parent / 'fixtures/dos-midi/descent14-game02.json')
    parser.add_argument('--reference', type=Path, default=repo / 'game_data/music/dos-references/descent14-game02')
    parser.add_argument('--output', type=Path, default=repo / 'temp/midi-parity/reference')
    args = parser.parse_args()
    raise SystemExit(0 if run(args.exporter.resolve(), args.fixture, args.reference, args.output) else 1)
