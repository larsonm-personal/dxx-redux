#!/usr/bin/env python3
"""Export bounded public LAN diagnostics via check annotations when blob downloads fail.

Only use with the public nightly-results-android-dual artifact. Native logs are
filtered, screenshots stay in the original artifact, and source hashes preserve
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
pattern = re.compile(r"LAN-TIMING|loadprof_v=1|mp preparation|Launcher game launch|preflight|sync_poll|auto.host|NETEXIT|SCRIPT_RESULT|Condition met|Exception|FAIL|BLOCKED|LAN final peer evidence|Timed out|Error:", re.I)
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
                data["state"] = {k: state.get(k) for k in ("in_game", "current_level_num", "screen_mode", "time_paused", "postlevel_active", "coop_briefing", "multiplayer", "menu")}
            events.append(event)
        text = "\n".join(json.dumps(e, separators=(",", ":")) for e in events)
    elif path.name in ("experiment.json", "results.json", "lifecycle.json") or "automation_" in path.name or path.name.endswith("summary.json"):
        pass
    else:
        text = "\n".join(line for line in text.splitlines() if pattern.search(line))
    if text:
        selected[relative] = {"source_sha256": hashlib.sha256(raw).hexdigest(), "text": text}
blob = gzip.compress(json.dumps(selected, separators=(",", ":")).encode(), mtime=0)
if len(blob) > 256 * 1024:
    raise SystemExit("Refusing annotation export over 256 KiB compressed")
encoded = base64.b64encode(blob).decode()
print(f"Exported {len(selected)} public text files/excerpts; gzip sha256={hashlib.sha256(blob).hexdigest()}")
for index, offset in enumerate(range(0, len(encoded), 4000)):
    print(f"::notice title=LAN_EVIDENCE_{index:04d}::{encoded[offset:offset+4000]}")
