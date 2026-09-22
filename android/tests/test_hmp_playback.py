"""Exercise the compiled Android converter with synthetic HMP and SMF fixtures."""
import argparse
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

from midi_diff import compare, note_volumes, read_midi

EXPORTER = None


def hmi_delta(n):
    result = bytearray()
    while n >= 128:
        result.append(n & 127)
        n >>= 7
    result.append(n | 128)
    return result


def make_fixture(ended_track=False):
    timing = bytes.fromhex('80 ff 2f 00')
    control = bytes(hmi_delta(10)) + bytes.fromhex('b0 6d 80 80 b0 6e ff')
    control_offset = len(control)
    control += bytes(hmi_delta(100)) + bytes.fromhex('b0 6f 80 8a ff 2f 00')
    melody = bytes(hmi_delta(10)) + bytes.fromhex('b0 6c 00')
    melody_offset = len(melody)
    melody += bytes.fromhex('8a 94 3c 64 81 94 3c 00 89 b4 07 7f 8a 94 3e 64 81 94 3e 00')
    melody += bytes(hmi_delta(79)) + bytes.fromhex('ff 2f 00')
    data = bytearray(0x308)
    data[:8] = b'HMIMIDIP'
    tracks = [timing, control, melody]
    if ended_track:
        tracks.append(bytes.fromhex('8a b0 6c 00 8a bb 40 00 80 ff 2f 00'))
    struct.pack_into('<III', data, 0x30, len(tracks), 120, 120)
    for i, track in enumerate(tracks):
        data += struct.pack('<III', i, 12 + len(track), 4 if i == 2 else 0) + track
    branch_offset = len(data)
    struct.pack_into('<I', data, 32, branch_offset)
    data += bytes([0] + [1] * (len(tracks) - 1))
    offsets = [control_offset, melody_offset] + ([4] if ended_track else [])
    for offset in offsets:
        data += struct.pack('<IBBBBIIII', offset, 128, 0, 0, 0, 0, 0, 0, 0)
    return data, branch_offset


def smf(events, end=1000):
    def delta(n):
        parts = [n & 127]
        while (n := n >> 7):
            parts.insert(0, (n & 127) | 128)
        return bytes(parts)
    payload, last = bytearray(), 0
    for time, message in events:
        payload += delta(time - last) + bytes(message)
        last = time
    payload += delta(end - last) + bytes.fromhex('ff 2f 00')
    return b'MThd' + struct.pack('>IHHH', 6, 0, 1, 500) + b'MTrk' + struct.pack('>I', len(payload)) + payload


class PlaybackTests(unittest.TestCase):
    def setUp(self):
        if EXPORTER is None:
            self.skipTest('Run via CTest or pass --exporter to exercise the compiled converter')
        root = Path(__file__).resolve().parents[2] / 'temp'
        root.mkdir(exist_ok=True)
        self.directory = tempfile.TemporaryDirectory(dir=root, prefix='hmp-parity-test-')
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def export(self, data, mode='repeat'):
        source, output = self.root / 'test.hmp', self.root / 'test.mid'
        source.write_bytes(data)
        result = subprocess.run([str(EXPORTER), str(source), str(output), mode], capture_output=True, text=True)
        return result, output

    def test_startup_and_branch_repeat(self):
        data, _ = make_fixture()
        result, output = self.export(data)
        self.assertEqual(result.returncode, 0, result.stderr)
        info = json.loads(result.stdout)
        self.assertEqual(info['branch_loop'], 1)
        self.assertAlmostEqual(info['repeat_ms'], 109 * 1000 / 120, places=5)
        events, duration = read_midi(str(output) + '.tml.mid')
        notes = note_volumes(events, 0, duration)[4]
        self.assertEqual(notes, [(60, 100, 0, 127), (62, 100, 125, 127),
                                 (60, 100, 125, 127), (62, 100, 125, 127)])
        self.assertFalse(any(e.status & 240 == 176 and 108 <= e.data[0] <= 111 for e in events))
        self.assertEqual([e.data[0] for e in events if e.status == 196], [0])
        result, output = self.export(data, 'once')
        self.assertEqual(result.returncode, 0)
        events, duration = read_midi(output)
        self.assertEqual(len(note_volumes(events, 0, duration)[4]), 2)

    def test_malformed_branch_records_rejected(self):
        original, branch = make_fixture()
        for offset, replacement in ((branch + 3, struct.pack('<I', 0xffffffff)),
                                     (branch + 3 + 7, b'\x01'),
                                     (branch + 3 + 7, b'\x02' + struct.pack('<I', 0xffffffff)),
                                     (0x38, b'\0\0\0\0')):
            bad = original.copy()
            bad[offset:offset + len(replacement)] = replacement
            result, _ = self.export(bad)
            self.assertNotEqual(result.returncode, 0)
        for length in (0, 0x307, branch, len(original) - 1):
            result, _ = self.export(original[:length])
            self.assertNotEqual(result.returncode, 0)

    def test_ended_track_not_reactivated_and_unsupported_loops_reported(self):
        data, _ = make_fixture(ended_track=True)
        result, output = self.export(data)
        self.assertEqual(result.returncode, 0, result.stderr)
        events, _ = read_midi(output)
        self.assertFalse(any(e.status == 203 for e in events))
        data, _ = make_fixture()
        struct.pack_into('<I', data, 32, 0)
        result, _ = self.export(data)
        self.assertEqual(result.returncode, 0)
        self.assertEqual(json.loads(result.stdout)['unsupported_branches'], 1)
        self.assertEqual(json.loads(result.stdout)['branch_loop'], 0)
        data, _ = make_fixture()
        at = data.index(bytes.fromhex('b0 6e ff'))
        data[at + 2] = 2
        result, _ = self.export(data)
        self.assertEqual(result.returncode, 0)
        self.assertEqual(json.loads(result.stdout)['unsupported_branches'], 1)

    def test_diff_detects_notes_timing_controllers_and_effective_volume(self):
        a, b = self.root / 'a.mid', self.root / 'b.mid'
        events = [(0, (180, 7, 0)), (100, (148, 60, 100)), (200, (180, 7, 125)), (300, (148, 62, 100))]
        a.write_bytes(smf(events))
        b.write_bytes(smf(events))
        self.assertTrue(compare(a, b)['passed'])
        for modified in (events[:-1], [(t + (30 if i == 1 else 0), m) for i, (t, m) in enumerate(events)],
                         [(0, (180, 7, 127)), *events[1:]],
                         [*events[:3], (300, (148, 63, 100))]):
            b.write_bytes(smf(modified))
            self.assertFalse(compare(a, b)['passed'])
        # Previous-song state matters even when it lies outside the message window
        a.write_bytes(smf([(0, (180, 7, 0)), (100, (148, 60, 100))]))
        b.write_bytes(smf([(100, (148, 60, 100))]))
        result = compare(a, b, reference_start=100, candidate_start=100, duration=500)
        self.assertFalse(result['channels'][4]['note_volume_state_matches'])

    def test_shared_context_is_explicit_and_cannot_hide_pan_reset(self):
        a, b = self.root / 'a.mid', self.root / 'b.mid'
        a.write_bytes(smf([(0, (180, 10, 47)), (100, (148, 60, 100))]))
        b.write_bytes(smf([(100, (148, 60, 100))]))
        args = dict(reference_start=100, candidate_start=100, duration=500)
        self.assertFalse(compare(a, b, **args)['passed'])
        self.assertTrue(compare(a, b, **args, shared_initial_state=True)['passed'])
        b.write_bytes(smf([(0, (180, 10, 64)), (100, (148, 60, 100))]))
        self.assertFalse(compare(a, b, **args, shared_initial_state=True)['passed'])

    def test_unfinalized_capture_rejected(self):
        p = self.root / 'unfinished.mid'
        data = bytearray(smf([(0, (148, 60, 100))]))
        data[18:22] = b'\0\0\0\0'
        p.write_bytes(data)
        with self.assertRaises(ValueError):
            read_midi(p)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--exporter', type=Path, required=True)
    args, remaining = parser.parse_known_args()
    EXPORTER = args.exporter.resolve()
    unittest.main(argv=[__file__, *remaining])
