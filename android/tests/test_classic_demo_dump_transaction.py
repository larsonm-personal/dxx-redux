import argparse
import hashlib
import json
import os
import shutil
import struct
import subprocess
import tempfile
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
TEMP_ROOT = REPO_ROOT / "temp"


def file_hash(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run_dump(executable: Path, data_dir: Path, source: Path, output: Path) -> int:
    result = subprocess.run(
        [
            str(executable),
            "-hogdir",
            str(data_dir),
            "-nomovies",
            "-nomusic",
            "-nosound",
            "-classicdemo-dump-json",
            str(source),
            str(output),
        ],
        cwd=REPO_ROOT,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=False,
        timeout=10,
    )
    return result.returncode


def initial_wall_count_offset(demo: bytes) -> int:
    if len(demo) < 68 or demo[:3] not in (b"\x01\x0f\x03", b"\x01\x10\x03"):
        raise AssertionError("fixture is not a supported D2 classic demo")
    game_mode = struct.unpack_from("<i", demo, 7)[0]
    if game_mode != 0:
        raise AssertionError("wall-boundary fixture must be a single-player demo")
    mission_length = demo[56]
    event_offset = 57 + mission_length + 8
    if len(demo) < event_offset + 7 or demo[event_offset] != 28:
        raise AssertionError("fixture does not begin with a new-level event")
    return event_offset + 3


def require_malformed_preserves_output(
    executable: Path,
    data_dir: Path,
    source: Path,
    output: Path,
    prior: bytes,
) -> None:
    output.write_bytes(prior)
    if run_dump(executable, data_dir, source, output) == 0:
        raise AssertionError(f"malformed demo unexpectedly converted: {source.name}")
    if output.read_bytes() != prior:
        raise AssertionError(f"malformed demo changed prior output: {source.name}")
    if list(output.parent.glob(f"{output.name}.tmp.*")):
        raise AssertionError(f"malformed demo leaked temporary output: {source.name}")


def require_rejected_without_change(
    executable: Path, data_dir: Path, source: Path, output: Path, expected_hash: str
) -> None:
    if run_dump(executable, data_dir, source, output) == 0:
        raise AssertionError(f"alias unexpectedly accepted: {source} -> {output}")
    if file_hash(source.resolve()) != expected_hash:
        raise AssertionError(f"source changed after rejection: {source}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--data-dir", required=True, type=Path)
    parser.add_argument("--demo", required=True, type=Path)
    args = parser.parse_args()

    executable = args.executable.resolve()
    data_dir = args.data_dir.resolve()
    demo = args.demo.resolve()
    for path in (executable, data_dir, demo):
        if not path.exists():
            raise FileNotFoundError(path)

    TEMP_ROOT.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="classic_demo_dump_", dir=TEMP_ROOT) as directory:
        case_dir = Path(directory)
        source = case_dir / "source.dem"
        shutil.copyfile(demo, source)
        source_hash = file_hash(source)

        require_rejected_without_change(
            executable, data_dir, source, source, source_hash
        )
        relative_source = Path(os.path.relpath(source, REPO_ROOT))
        require_rejected_without_change(
            executable, data_dir, relative_source, source, source_hash
        )
        hard_link = case_dir / "hard-link.dem"
        os.link(source, hard_link)
        require_rejected_without_change(
            executable, data_dir, source, hard_link, source_hash
        )

        malformed = case_dir / "malformed.dem"
        malformed.write_bytes(b"not a classic demo")
        prior_output = case_dir / "prior.jsonl"
        prior_output.write_bytes(b"preserve me")
        prior_hash = file_hash(prior_output)
        if run_dump(executable, data_dir, malformed, prior_output) == 0:
            raise AssertionError("malformed input unexpectedly converted")
        if file_hash(prior_output) != prior_hash:
            raise AssertionError("malformed input changed the prior output")

        demo_bytes = source.read_bytes()
        wall_offset = initial_wall_count_offset(demo_bytes)
        for wall_count in (-1, 255, 2_147_483_647):
            wall_demo = case_dir / f"wall-count-{wall_count}.dem"
            mutated = bytearray(demo_bytes)
            struct.pack_into("<i", mutated, wall_offset, wall_count)
            wall_demo.write_bytes(mutated)
            require_malformed_preserves_output(
                executable, data_dir, wall_demo, prior_output, b"preserve wall count"
            )
        for count_bytes in range(4):
            truncated = case_dir / f"wall-count-truncated-{count_bytes}.dem"
            truncated.write_bytes(demo_bytes[: wall_offset + count_bytes])
            require_malformed_preserves_output(
                executable, data_dir, truncated, prior_output, b"preserve count truncation"
            )
        one_wall = bytearray(demo_bytes[: wall_offset + 4])
        struct.pack_into("<i", one_wall, wall_offset, 1)
        for record_bytes in range(7):
            truncated = case_dir / f"wall-record-truncated-{record_bytes}.dem"
            truncated.write_bytes(one_wall + bytes(record_bytes))
            require_malformed_preserves_output(
                executable, data_dir, truncated, prior_output, b"preserve record truncation"
            )

        output = case_dir / "result.jsonl"
        if run_dump(executable, data_dir, source, output) != 0:
            raise AssertionError("valid distinct conversion failed")
        records = [json.loads(line) for line in output.read_text(encoding="utf-8").splitlines()]
        if len(records) < 2 or records[0].get("type") != "header":
            raise AssertionError("conversion output is missing its header")
        if records[-1].get("type") != "result":
            raise AssertionError("conversion output is missing its result")
        if list(case_dir.glob("*.tmp.*")):
            raise AssertionError("classic demo conversion leaked a temporary output")

    print("classic demo dump transaction tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
