"""Run the native Obsidian 13 secret regression against supplied installed assets."""
import argparse
import json
from pathlib import Path
import subprocess


def validate(report):
    level = report["levels"][0]
    assert level["level_num"] == 13
    assert level["scanner_enabled"] and level["secret_areas_complete"]
    assert level["secret_count"] == 6, level["secret_count"]
    liquids = {tuple(secret["segments"]): secret for secret in level["secrets"] if secret["liquid_only"]}
    assert set(liquids) == {(154,), (282,)}, set(liquids)
    for segments, entrance, items in (((154,), (144, 3), {21, 30}), ((282,), (203, 3), {23})):
        secret = liquids[segments]
        assert {(entry["seg"], entry["side"]) for entry in secret["entrances"]} == {entrance}
        assert {item["id"] for item in secret["items"]} == items
    identities = [secret["identity"] for secret in level["secrets"]]
    assert len(set(identities)) == 6 and all(int(identity) for identity in identities)
    # Transparent water at 58/71 and 334/347 must never become liquid secrets
    assert not any(segment in {58, 71, 334, 347} for members in liquids for segment in members)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", required=True, type=Path)
    parser.add_argument("--hogdir", required=True, type=Path)
    parser.add_argument("--mission-dir", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    result = subprocess.run([
        str(args.exe.resolve()), "-hogdir", str(args.hogdir.resolve()),
        "-extra-dir", str(args.mission_dir.resolve()), "-mission", "obsidian", "-level", "13",
        "-secretarea-json-out", str(args.output.resolve()),
    ], capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, (result.returncode, result.stdout, result.stderr)
    validate(json.loads(args.output.read_text(encoding="utf-8")))
    print("Obsidian 13: six secrets, both opaque reward pockets, transparent water excluded")


if __name__ == "__main__":
    main()
