"""Check the real audio experiments, including a known-broken upstream control."""
import json
from pathlib import Path
import sys


def verify(path):
    report = json.loads(path.read_text())
    assert all(dep['license'] in ('MIT', 'BSD-3-Clause') for dep in report['dependencies'].values())
    assert report['selected_tracks'] == [0, 1, 2, 3, 4, 5, 8], 'Wrong Level 7 arrangement'
    assert report['noteoff_effect']['live-opl2']['identical_to_no_noteoffs'], 'Pinned upstream negative control changed'
    assert not report['noteoff_effect']['file-opl2']['identical_to_no_noteoffs'], 'File player ignored note-off'
    assert not report['noteoff_effect']['live-fixed']['identical_to_no_noteoffs'], 'Experimental fix ignored note-off'
    assert report['fixed_live_matches_file'], 'External scheduler differs from file playback'
    assert report['noteoff_equivalence']['file-opl2']['identical'], 'MIDI note-off encodings differ'
    for name in ('fm-opl2', 'fm-opl3', 'fm-live-fixed', 'tone-ymfm', 'tone-emu8950'):
        clip = report['renders'][name]
        assert clip['peak'] > 0, f'{name}: silence'
        assert clip['clipped_samples'] == 0, f'{name}: PCM rails reached'
    print(f'PASS: {path} (FM selection, note-off negative control/fix, file/live PCM equality, both chip cores)')


if __name__ == '__main__':
    for filename in sys.argv[1:]:
        verify(Path(filename))
