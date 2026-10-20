#!/usr/bin/env python3
"""Export bounded public LAN diagnostics via check annotations when blob downloads fail.

Only use with the public nightly-results-android-dual artifact. Native logs are
filtered, most screenshots stay in the original artifact, and source hashes preserve
provenance. No network access or credentials are read by this helper.
"""

import base64
import gzip
import hashlib
import json
import re
import sys
from pathlib import Path

root = Path(sys.argv[1])
selected = {}
pattern = re.compile(
    r"LAN-TIMING|loadprof_v=1|mp preparation|Launcher game launch|preflight|sync_poll|auto.host|NETEXIT|SCRIPT_RESULT|Condition met|Exception|FAIL|BLOCKED|LAN final peer evidence|Timed out|Error:|DXX-MP|LobbyService|DXX-Launcher|DXX-RouteMetadata|MainActivity|DXX-Automate",
    re.I,
)
for path in sorted(root.rglob("*")):
    if not path.is_file() or path.is_symlink() or path.suffix not in (".txt", ".log", ".json", ".jsonl"):
        continue
    if path.stat().st_size > 16 * 1024 * 1024:
        continue
    raw = path.read_bytes()
    text = raw.decode("utf-8-sig", errors="replace")
    relative = str(path.relative_to(root))
    if path.name == "events.jsonl":
        events = []
        for line in text.splitlines():
            event = json.loads(line)
            data = event.get("data")
            if event["event"] == "observation" and isinstance(data, dict):
                state = data.get("state") or {}
                data["state"] = {
                    k: state.get(k)
                    for k in (
                        "in_game",
                        "current_level_num",
                        "screen_mode",
                        "time_paused",
                        "postlevel_active",
                        "coop_briefing",
                        "multiplayer",
                        "menu",
                    )
                }
            events.append(event)
        text = "\n".join(json.dumps(e, separators=(",", ":")) for e in events)
    elif (
        (path.suffix == ".json" and len(raw) <= 512 * 1024)
        or (relative.startswith("master/") and len(raw) <= 512 * 1024)
        or "automation_" in path.name
        or path.name.endswith(("-properties.txt", "-routes.txt", "-neighbors.txt"))
        or path.name == "emulator-version.log"
    ):
        pass
    else:
        text = "\n".join(line for line in text.splitlines() if pattern.search(line))
    if text:
        selected[relative] = {"source_sha256": hashlib.sha256(raw).hexdigest(), "text": text}
# Preserve a few small original UI snapshots without inflating check annotations
image_bytes = 0
for path in sorted(root.rglob("*-1-*-*.png")):
    if path.is_symlink() or not path.name.endswith(("-briefing1.png", "-scores.png")):
        continue
    if path.stat().st_size > 64 * 1024 or image_bytes + path.stat().st_size > 128 * 1024:
        continue
    raw = path.read_bytes()
    image_bytes += len(raw)
    selected[str(path.relative_to(root))] = {
        "source_sha256": hashlib.sha256(raw).hexdigest(),
        "base64": base64.b64encode(raw).decode(),
    }
blob = gzip.compress(json.dumps(selected, separators=(",", ":")).encode(), mtime=0)
if len(blob) > 256 * 1024:
    raise SystemExit("Refusing annotation export over 256 KiB compressed")
encoded = base64.b64encode(blob).decode()
print(f"Exported {len(selected)} public text files/excerpts; gzip sha256={hashlib.sha256(blob).hexdigest()}")
for index, offset in enumerate(range(0, len(encoded), 4000)):
    print(f"::notice title=LAN_EVIDENCE_{index:04d}::{encoded[offset : offset + 4000]}")
