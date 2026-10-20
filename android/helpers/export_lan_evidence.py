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
pattern = re.compile(
    r"LAN-TIMING|loadprof_v=1|mp preparation|Launcher game launch|preflight|sync_poll|auto.host|auto_net:|select_players|initial sync retry:|NETEXIT|SCRIPT_RESULT|Condition met|Exception|FAIL|BLOCKED|LAN final peer evidence|Timed out|Error:|DXX-MP|LobbyService|DXX-Launcher|DXX-RouteMetadata|MainActivity|DXX-Automate",
    re.I,
)
for path in sorted(root.rglob("*")):
    if not path.is_file() or path.is_symlink() or path.suffix not in (".txt", ".log", ".json", ".jsonl", ".jsonc"):
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
        or path.name.startswith("test_lan_latency_")
        or path.name.endswith(("-properties.txt", "-routes.txt", "-neighbors.txt"))
        or path.name == "emulator-version.log"
    ):
        pass
    else:
        lines = []
        startup = bool(re.fullmatch(r"d[12]-\d+-emulator-\d+-logcat.txt", path.name))
        for line in text.splitlines():
            # Preserve native startup diagnostics, including packet/retry events,
            # up to briefing readiness. This exports saved evidence, not a rerun
            if pattern.search(line) or (
                startup and re.search(r"\sDXX(?:-DLOG|-Init|-Lifecycle|-Redux)?\s*:|SDL", line)
            ):
                lines.append(line)
            if startup and "[LAN-TIMING]" in line and "phase=6 armed=1" in line:
                break
        text = "\n".join(lines)
    if text:
        selected[relative] = {"source_sha256": hashlib.sha256(raw).hexdigest(), "text": text}
blob = gzip.compress(json.dumps(selected, separators=(",", ":")).encode(), mtime=0)
if len(blob) > 128 * 1024:
    raise SystemExit("Refusing annotation export over 128 KiB compressed")
encoded = base64.b64encode(blob).decode()
print(f"Exported {len(selected)} public text files/excerpts; gzip sha256={hashlib.sha256(blob).hexdigest()}")
group = int(sys.argv[2]) if len(sys.argv) > 2 else 0
for index, offset in enumerate(range(0, len(encoded), 4000)):
    if index // 10 == group:
        print(f"::notice title=LAN_EVIDENCE_{index:04d}::{encoded[offset : offset + 4000]}")
