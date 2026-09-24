"""Audit every GM HMP/MIDI member of named HOGs with the shipping converter and TSF.

Build soundfont_coverage and hmp_midi_export from the host extract CMake project.
Pass --hog d1=path/to/descent.hog --hog d2=path/to/descent2.hog (repeatable).
The generated report includes note/velocity mappings, fallback selections and
isolated audibility checks. It does not certify SC-55 timbral/effects accuracy.
"""
import argparse
import csv
import hashlib
import io
import json
from pathlib import Path
import subprocess

from fm_feasibility.experiment import hog_members


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bin', type=Path, default=Path('android/build/host-extract-tests/Release'))
    parser.add_argument('--sf2', type=Path, required=True)
    parser.add_argument('--hog', action='append', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    report = {'soundfont': str(args.sf2.resolve()), 'sha256': digest(args.sf2),
              'method': 'Shipping GM conversion including first pass and repeat; actual TSF preset/key/velocity selection; isolated float rendering up to 2 seconds with neutral controllers',
              'numbering': 'Channels 1-16; programs and MIDI keys 0-127',
              'scope': [], 'songs': [], 'totals': {}}
    columns = ['channel', 'requested_bank', 'program', 'preset', 'key', 'velocity', 'note_count',
               'first_ms', 'exact_preset', 'actual_bank', 'actual_program', 'regions']
    for item in args.hog:
        label, filename = item.split('=', 1)
        assert label and all(c.isalnum() or c in '-_' for c in label), label
        path = Path(filename)
        members = hog_members(path)
        tracks = sorted(name for name in members if name.endswith(('.hmp', '.mid')))
        report['scope'].append({'label': label, 'hog': str(path.resolve()), 'sha256': digest(path), 'tracks': tracks})
        folder = args.output / label
        folder.mkdir(exist_ok=True)
        for song in tracks:
            assert Path(song).name == song and '\\' not in song, song
            source = folder / song
            source.write_bytes(members[song])
            midi = source
            if song.endswith('.hmp'):
                midi = folder / (song + '.mid')
                conversion = subprocess.run([str(args.bin / 'hmp_midi_export.exe'), str(source), str(midi), 'repeat'],
                                            check=True, capture_output=True, text=True)
                (folder / (song + '.conversion.log')).write_text(conversion.stdout + conversion.stderr)
            result = subprocess.run([str(args.bin / 'soundfont_coverage.exe'), str(args.sf2), str(midi)],
                                    check=True, capture_output=True, text=True)
            (folder / (song + '.tsv')).write_text(result.stdout, encoding='utf8')
            uses = list(csv.DictReader(io.StringIO(result.stdout), delimiter='\t'))
            for use in uses:
                for column in columns:
                    use[column] = int(use[column])
                use['peak'] = float(use['peak'])
            missing = [use for use in uses if not use['regions'] or use['peak'] <= 1e-12]
            fallback = [use for use in uses if not use['exact_preset']]
            entry = {'game': label, 'song': song, 'source_sha256': digest(source), 'midi_sha256': digest(midi),
                     'note_ons': sum(use['note_count'] for use in uses), 'unique_uses': len(uses),
                     'missing': missing, 'fallback': fallback,
                     'melodic_programs': sorted({use['program'] for use in uses if use['channel'] != 10}),
                     'drum_programs': sorted({use['program'] for use in uses if use['channel'] == 10}),
                     'drum_keys': sorted({use['key'] for use in uses if use['channel'] == 10})}
            report['songs'].append(entry)
            print(f'{label}/{song}: {entry["note_ons"]} notes, {len(missing)} uncovered uses, {len(fallback)} fallback uses', flush=True)
    report['totals'] = {
        'songs': len(report['songs']),
        'note_ons': sum(s['note_ons'] for s in report['songs']),
        'songs_with_missing': sum(bool(s['missing']) for s in report['songs']),
        'songs_with_fallback': sum(bool(s['fallback']) for s in report['songs']),
        'missing_note_ons': sum(u['note_count'] for s in report['songs'] for u in s['missing']),
        'fallback_note_ons': sum(u['note_count'] for s in report['songs'] for u in s['fallback']),
    }
    (args.output / 'coverage.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    lines = ['# SoundFont song coverage', '', f'SF2 SHA-256: `{report["sha256"]}`', '',
             report['method'], '', report['numbering'], '',
             '| Game | Song | Note-ons | Uncovered uses | Fallback uses |',
             '| --- | --- | ---: | ---: | ---: |']
    for song in report['songs']:
        lines.append(f'| {song["game"]} | {song["song"]} | {song["note_ons"]} | {len(song["missing"])} | {len(song["fallback"])} |')
    lines += ['', 'Counts include the initial pass and exported repeat. A use is a unique channel/bank/program/resolved preset/key/velocity combination.',
              'Instrument availability is independent of deliberate song volume/mutes. Tests do not compare timbre, effects, mixing balance or polyphony.',
              'See coverage.json for complete missing/fallback details and the per-song TSVs for every tested use.', '']
    (args.output / 'coverage.md').write_text('\n'.join(lines), encoding='utf8')
    print(json.dumps(report['totals'], indent=2))


if __name__ == '__main__':
    main()
