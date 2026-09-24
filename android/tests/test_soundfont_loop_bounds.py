"""Generated SF2 regression for loop bounds and coverage diagnostics; no borrowed samples."""
import argparse
import csv
import io
import math
from pathlib import Path
import struct
import subprocess
import tempfile


def chunk(tag, data):
    return tag + struct.pack('<I', len(data)) + data + b'\0' * (len(data) & 1)


def font(start, end):
    info = chunk(b'LIST', b'INFO' + chunk(b'ifil', struct.pack('<HH', 2, 1)))
    pcm = struct.pack('<128h', *(int(10000 * math.sin(i / 8)) for i in range(128)))
    sdta = chunk(b'LIST', b'sdta' + chunk(b'smpl', pcm))
    tables = {
        b'phdr': struct.pack('<20sHHHIII', b'fixture', 0, 0, 0, 0, 0, 0) + struct.pack('<20sHHHIII', b'EOP', 0, 0, 1, 0, 0, 0),
        b'pbag': struct.pack('<4H', 0, 0, 1, 0), b'pmod': bytes(10), b'pgen': struct.pack('<4H', 41, 0, 0, 0),
        b'inst': struct.pack('<20sH20sH', b'fixture', 0, b'EOI', 1),
        b'ibag': struct.pack('<4H', 0, 0, 3, 0), b'imod': bytes(10), b'igen': struct.pack('<8H', 43, 60 | 60 << 8, 54, 1, 53, 0, 0, 0),
        b'shdr': struct.pack('<20sIIIIIBbHH', b'fixture', 4, 68, start, end, 22050, 60, 0, 0, 1) + bytes(46),
    }
    pdta = chunk(b'LIST', b'pdta' + b''.join(chunk(tag, data) for tag, data in tables.items()))
    return chunk(b'RIFF', b'sfbk' + info + sdta + pdta)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--probe', required=True)
    parser.add_argument('--work', type=Path, required=True)
    args = parser.parse_args()
    args.work.mkdir(parents=True, exist_ok=True)
    cases = [('active', 16, 48, True), ('empty', 16, 16, True), ('single', 16, 17, True),
             ('reversed', 16, 14, False), ('outside-active', 16, 200, False), ('outside-empty', 200, 200, False)]
    with tempfile.TemporaryDirectory(prefix='sf2-loop-', dir=args.work) as directory:
        for name, start, end, accepted in cases:
            path = Path(directory) / f'{name}.sf2'
            path.write_bytes(font(start, end))
            result = subprocess.run([args.probe, str(path)], capture_output=True, text=True)
            assert (result.returncode == 0) == accepted, (name, result.stdout, result.stderr)
        midi = Path(directory) / 'coverage.mid'
        events = b'\0\xc0\0\0\x90\x3c\x64\0\x90\x3d\x64\0\xc0\x03\0\x90\x3c\x64\0\xff\x2f\0'
        midi.write_bytes(b'MThd' + struct.pack('>IHHH', 6, 0, 1, 500) + b'MTrk' + struct.pack('>I', len(events)) + events)
        result = subprocess.run([args.probe, str(Path(directory) / 'active.sf2'), str(midi)],
                                check=True, capture_output=True, text=True)
        uses = list(csv.DictReader(io.StringIO(result.stdout), delimiter='\t'))
        assert len(uses) == 3, uses
        assert sum(u['regions'] == '0' for u in uses) == 1, uses
        assert sum(u['exact_preset'] == '0' for u in uses) == 1, uses
        assert all(float(u['peak']) > 0 for u in uses if u['regions'] != '0'), uses
    print('PASS: valid/empty/single-sample loops accepted; reversed/out-of-bounds loops rejected')
    print('PASS: coverage distinguishes audible notes, unmapped keys and missing-preset fallback')


if __name__ == '__main__':
    main()
