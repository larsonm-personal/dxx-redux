#!/usr/bin/env python3
"""Prefer the official archive on disposable GitHub-hosted Noble x64 runners."""

import os
from pathlib import Path
import platform
import re
import subprocess

# actions/runner-images images/ubuntu/scripts/build/configure-apt-sources.sh
# Only priorities change; all endpoints and apt's fallback behavior are retained
ORIGINAL = (
    b"http://azure.archive.ubuntu.com/ubuntu/\tpriority:1\n"
    b"https://archive.ubuntu.com/ubuntu/\tpriority:2\n"
    b"https://security.ubuntu.com/ubuntu/\tpriority:3\n"
)
PREFERRED = (
    ORIGINAL.replace(b"priority:1", b"priority:4")
    .replace(b"priority:2", b"priority:1")
    .replace(b"priority:4", b"priority:2")
)
MIRRORS = Path("/etc/apt/apt-mirrors.txt")
SOURCES = Path("/etc/apt/sources.list.d/ubuntu.sources")


def prefer_archive(mirrors, sources, environ, release, machine):
    """Validate the known image configuration before returning replacement bytes."""
    if (
        environ.get("GITHUB_ACTIONS") != "true"
        or environ.get("RUNNER_ENVIRONMENT") != "github-hosted"
        or environ.get("ImageOS") != "ubuntu24"
        or machine != "x86_64"
        or release.get("ID") != "ubuntu"
        or release.get("VERSION_ID") != "24.04"
    ):
        raise ValueError("Expected a disposable GitHub-hosted Ubuntu Noble x64 runner")
    uris = re.findall(rb"^URIs:[ \t]*(.*)$", sources, re.MULTILINE)
    if not uris or any(uri != b"mirror+file:/etc/apt/apt-mirrors.txt" for uri in uris):
        raise ValueError("Unexpected Ubuntu source URIs; refusing to change mirror priorities")
    if mirrors not in (ORIGINAL, PREFERRED):
        raise ValueError("Unexpected apt mirror list; refusing to change mirror priorities")
    return PREFERRED


def main():
    before = MIRRORS.read_bytes()
    after = prefer_archive(
        before, SOURCES.read_bytes(), os.environ, platform.freedesktop_os_release(), platform.machine()
    )
    if before != after:
        subprocess.run(["sudo", "tee", str(MIRRORS)], input=after, stdout=subprocess.DEVNULL, check=True)
    if MIRRORS.read_bytes() != after:
        raise RuntimeError("Apt mirror priority verification failed")
    print("Apt mirror priorities: archive=1, Azure=2, security=3")


if __name__ == "__main__":
    main()
