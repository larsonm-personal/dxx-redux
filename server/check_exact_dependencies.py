#!/usr/bin/env python3
"""Fail before/after upgrades if a direct dependency can drift (Python 3.11+)."""

import pathlib
import re
import tomllib

manifest = tomllib.loads(pathlib.Path(__file__).with_name("Cargo.toml").read_text())


def check(table):
    for section, entries in table.items():
        if section in ("dependencies", "dev-dependencies", "build-dependencies"):
            for name, spec in entries.items():
                version = spec if isinstance(spec, str) else spec.get("version")
                if version is None and isinstance(spec, dict) and "path" in spec:
                    continue
                if not isinstance(version, str) or not re.fullmatch(
                    r"=\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?(?:\+[0-9A-Za-z.-]+)?",
                    version,
                ):
                    raise SystemExit(f"{section}.{name} must have an exact =version pin")
        elif isinstance(entries, dict):
            check(entries)


check(manifest)
print("All versioned direct dependencies have exact pins")
