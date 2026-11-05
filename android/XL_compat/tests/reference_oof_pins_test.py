"""Exercise exact OOF source pins using platform-independent binary fixtures."""

import argparse
from pathlib import Path
import subprocess


PATHS = ("models/oofread.cpp", "render/oofrender.cpp", "3d/oof2model.cpp")


def write_exact(path, payload):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(payload)
    if path.read_bytes() != payload:
        raise ValueError("OOF fixture bytes changed during binary write")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cmake", required=True)
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    validator = Path(__file__).with_name("reference_oof_pins.cmake")

    def validate(expected, root=args.output):
        result = subprocess.run(
            [args.cmake, "-DXL_REFERENCE_GEOMETRY_ROOT=" + str(root), "-P", str(validator)],
            capture_output=True,
            text=True,
            check=False,
        )
        if expected and result.returncode != 0:
            raise ValueError("Equivalent source rejected: " + result.stderr)
        if not expected and (result.returncode == 0 or "OOF reference changed:" not in result.stderr):
            raise ValueError("Changed source was not rejected by the hash guard: " + result.stderr)

    validate(True, args.reference)

    # Record actual CMake text-writer behavior, including Windows CRCRLF expansion
    args.output.mkdir(parents=True, exist_ok=True)
    probe = args.output / "text-writer.cmake"
    write_exact(probe, b'file(WRITE "${OUTPUT}/text-writer.bin" "first\\r\\nsecond\\r\\n")\n')
    subprocess.run([args.cmake, "-DOUTPUT=" + str(args.output), "-P", str(probe)], check=True)
    observed = (args.output / "text-writer.bin").read_bytes()
    print("CMake text-writer bytes: " + observed.hex(), flush=True)

    originals = {name: (args.reference / name).read_bytes() for name in PATHS}
    for ending in (b"\n", b"\r\n"):
        payloads = {name: data.replace(b"\r\n", b"\n").replace(b"\n", ending) for name, data in originals.items()}
        for name, payload in payloads.items():
            write_exact(args.output / name, payload)
        validate(True)
        for name, original in payloads.items():
            first = original.index(ending)
            opposite = b"\r\n" if ending == b"\n" else b"\n"
            changes = {
                "edit": original + b"\nint substantive_extra_source = 1;\n",
                "wrong-source": b"int unrelated_source = 1;\n",
                "truncated": original[:-1],
                "mixed": original[:first] + opposite + original[first + len(ending) :],
                "bom": b"\xef\xbb\xbf" + original,
            }
            if ending == b"\r\n":
                changes["windows-text-expanded"] = original.replace(b"\n", b"\r\n")
            for change, payload in changes.items():
                write_exact(args.output / name, payload)
                validate(False)
                print(f"Rejected {name}: {change}", flush=True)
            write_exact(args.output / name, original)
        validate(True)
    for name, original in originals.items():
        if (args.reference / name).read_bytes() != original:
            raise ValueError("Original OOF source changed during fixture controls")
    print("OOF pins: 4 equivalent-source passes and 33 changed-source rejections")


if __name__ == "__main__":
    main()
