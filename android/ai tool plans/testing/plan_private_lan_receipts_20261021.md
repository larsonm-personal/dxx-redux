# Private LAN receipt diagnostics

- Preserve each master child test's own exit code when a sibling fails; keep aggregate runner failure intact
- Match bounded private LAN logs against fixed failure-message prefixes from the hash-verified controller source
- Retain public source locations, duration and log availability/size without copying private text or paths
- Test mixed child outcomes, missing logs, source mismatch, private text exclusion and bounded reads
- Publish an isolated draft and follow exact-head checks; retain existing workflow trust and private artifact restrictions
