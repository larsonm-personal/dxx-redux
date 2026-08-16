#!/usr/bin/env python3
"""Normalize JSON files using Python's standard JSON parser."""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import sys
import tempfile


MISSION_METADATA_FLOAT_FIELDS = {
    "distance",
    "mine_volume",
    "mine_volume_normalized",
    "travel_distance",
}
MISSION_METADATA_POSITION_FIELDS = {"activation_pos", "aim_pos", "label_pos"}
MISSION_METADATA_EXCLUDED_FIELDS = {"replacements", "replacement_groups"}


def reject_json_constant(token: str) -> object:
    raise ValueError(f"non-finite JSON number is not allowed: {token}")


def unique_json_object(pairs: list[tuple[str, object]]) -> dict[str, object]:
    result: dict[str, object] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON object member: {key!r}")
        result[key] = value
    return result


def canonicalize_mission_metadata(value: object, parent_key: str = "") -> object:
    """Restore the numeric types defined by the checked-in metadata schema.

    Android's org.json writer renders integral doubles such as 514.0 as 514,
    while nlohmann/json in the host generator preserves the decimal point.
    JSON considers those values equivalent, but generated fixtures must also be
    byte-stable across both generation routes.
    """
    if isinstance(value, list):
        normalized = [canonicalize_mission_metadata(item, parent_key) for item in value]
        if parent_key == "" and all(
            isinstance(item, dict) and isinstance(item.get("mission_filename"), str)
            for item in normalized
        ):
            normalized.sort(
                key=lambda item: (item["mission_filename"].casefold(), item["mission_filename"])
            )
            if all("target_index" in item for item in normalized):
                for index, item in enumerate(normalized):
                    item["target_index"] = index
        return normalized
    if not isinstance(value, dict):
        return value

    result: dict[str, object] = {}
    for key, item in value.items():
        if key in MISSION_METADATA_EXCLUDED_FIELDS:
            continue
        normalized = canonicalize_mission_metadata(item, key)
        is_number = isinstance(normalized, (int, float)) and not isinstance(normalized, bool)
        if is_number and (
            key in MISSION_METADATA_FLOAT_FIELDS
            or (parent_key in MISSION_METADATA_POSITION_FIELDS and key in {"x", "y", "z"})
        ):
            normalized = float(normalized)
        result[key] = normalized
    return result


def format_json_text(text: str, sort_keys: bool, mission_metadata: bool = False) -> str:
    value = json.loads(
        text,
        parse_constant=reject_json_constant,
        object_pairs_hook=unique_json_object,
    )
    if mission_metadata:
        if not isinstance(value, (dict, list)):
            raise ValueError("mission metadata root must be an object or array")
        value = canonicalize_mission_metadata(value)
    return (
        json.dumps(
            value,
            ensure_ascii=False,
            indent=2,
            sort_keys=sort_keys,
            allow_nan=False,
        )
        + "\n"
    )


def read_file(path: pathlib.Path) -> str:
    return path.read_text(encoding="utf-8-sig")


def write_file(path: pathlib.Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary_path: pathlib.Path | None = None
    try:
        with tempfile.NamedTemporaryFile(
            "w", encoding="utf-8", newline="\n", dir=path.parent, delete=False
        ) as out_file:
            temporary_path = pathlib.Path(out_file.name)
            out_file.write(text)
        os.replace(temporary_path, path)
    finally:
        if temporary_path is not None:
            temporary_path.unlink(missing_ok=True)


def normalize_file(path: pathlib.Path, sort_keys: bool, check: bool, mission_metadata: bool) -> int:
    original = read_file(path)
    normalized = format_json_text(original, sort_keys, mission_metadata)
    if original == normalized:
        return 0
    if check:
        print(f"{path}: not normalized", file=sys.stderr)
        return 1
    write_file(path, normalized)
    return 0


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Normalize JSON formatting")
    parser.add_argument("--check", action="store_true", help="fail if any file is not normalized")
    parser.add_argument("--sort-keys", action="store_true", help="sort object keys alphabetically")
    parser.add_argument(
        "--mission-metadata",
        action="store_true",
        help="canonicalize schema-defined floating-point mission metadata fields",
    )
    parser.add_argument("files", nargs="*", type=pathlib.Path)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if not args.files:
        if args.check:
            print("--check requires at least one file", file=sys.stderr)
            return 2
        try:
            sys.stdout.write(
                format_json_text(sys.stdin.read(), args.sort_keys, args.mission_metadata)
            )
            return 0
        except (json.JSONDecodeError, ValueError) as error:
            print(f"stdin: JSON validation error: {error}", file=sys.stderr)
            return 1

    result = 0
    for path in args.files:
        try:
            result |= normalize_file(path, args.sort_keys, args.check, args.mission_metadata)
        except OSError as error:
            print(f"{path}: {error}", file=sys.stderr)
            result = 1
        except json.JSONDecodeError as error:
            print(f"{path}: JSON parse error: {error}", file=sys.stderr)
            result = 1
        except ValueError as error:
            print(f"{path}: JSON validation error: {error}", file=sys.stderr)
            result = 1
    return result


if __name__ == "__main__":
    raise SystemExit(main())
