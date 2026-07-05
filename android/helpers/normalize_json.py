#!/usr/bin/env python3
"""Normalize JSON files using Python's standard JSON parser."""

from __future__ import annotations

import argparse
import json
import pathlib
import sys


def format_json_text(text: str, sort_keys: bool) -> str:
    value = json.loads(text)
    return json.dumps(value, ensure_ascii=False, indent=2, sort_keys=sort_keys) + "\n"


def read_file(path: pathlib.Path) -> str:
    return path.read_text(encoding="utf-8-sig")


def write_file(path: pathlib.Path, text: str) -> None:
    with path.open("w", encoding="utf-8", newline="\n") as out_file:
        out_file.write(text)


def normalize_file(path: pathlib.Path, sort_keys: bool, check: bool) -> int:
    original = read_file(path)
    normalized = format_json_text(original, sort_keys)
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
    parser.add_argument("files", nargs="*", type=pathlib.Path)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if not args.files:
        if args.check:
            print("--check requires at least one file", file=sys.stderr)
            return 2
        sys.stdout.write(format_json_text(sys.stdin.read(), args.sort_keys))
        return 0

    result = 0
    for path in args.files:
        try:
            result |= normalize_file(path, args.sort_keys, args.check)
        except OSError as error:
            print(f"{path}: {error}", file=sys.stderr)
            result = 1
        except json.JSONDecodeError as error:
            print(f"{path}: JSON parse error: {error}", file=sys.stderr)
            result = 1
    return result


if __name__ == "__main__":
    raise SystemExit(main())
