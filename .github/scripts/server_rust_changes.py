#!/usr/bin/env python3
"""Select server work using PR aggregate changes or a push's before/after diff."""

import json
import os
from pathlib import Path
import re
import subprocess


def git(*args):
    return subprocess.check_output(["git", *args], input="", text=True).strip()


def commit(value):
    if not isinstance(value, str) or not re.fullmatch(r"[0-9a-f]{40}", value):
        raise ValueError("Expected a full commit SHA from the GitHub event")
    if subprocess.run(["git", "cat-file", "-e", f"{value}^{{commit}}"], capture_output=True).returncode:
        # A force-push's previous tip may not be reachable from current refs
        subprocess.run(["git", "fetch", "--no-tags", "--depth=1", "origin", value], check=True)
    return value


def server_changed(event_name, event):
    if event_name == "pull_request":
        base = commit(event["pull_request"]["base"]["sha"])
        head = commit(event["pull_request"]["head"]["sha"])
        base = git("merge-base", base, head)
    elif event_name == "push":
        if event.get("deleted"):
            return False
        head = commit(event["after"])
        before = event["before"]
        # A new branch has no previous tree; all files are additions
        base = git("hash-object", "-t", "tree", "--stdin") if before == "0" * 40 else commit(before)
    else:
        raise ValueError(f"Unsupported event: {event_name}")
    # Disable rename detection so moves both into and out of server/ are changes
    result = subprocess.run(["git", "diff", "--quiet", "--no-renames", base, head, "--", "server/"])
    if result.returncode not in (0, 1):
        raise RuntimeError(f"Cannot determine server changes (git diff exit {result.returncode})")
    return result.returncode == 1


if __name__ == "__main__":
    changed = server_changed(
        os.environ["GITHUB_EVENT_NAME"], json.loads(Path(os.environ["GITHUB_EVENT_PATH"]).read_text())
    )
    value = str(changed).lower()
    with Path(os.environ["GITHUB_OUTPUT"]).open("a") as output:
        output.write(f"changed={value}\n")
    print(f"server/ changed: {value}")
