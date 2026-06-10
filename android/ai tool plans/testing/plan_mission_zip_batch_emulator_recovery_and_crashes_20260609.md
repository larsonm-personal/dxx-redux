# Mission ZIP Batch Emulator Recovery And Crashes - 2026-06-09

## Goal
Make the mission ZIP batch resilient to emulator crashes and investigate recurring analysis worker crashes, starting with a concrete failed ZIP.

## Plan
- [x] Locate existing emulator recovery helpers and the mission ZIP batch runner failure boundaries.
- [x] Add batch-level emulator restart/reinstall/provision handling when the device disappears or is offline.
- [x] Inspect failed mission JSONs and pick the first reproducible worker crash.
- [x] Reproduce one crash with focused logs/artifacts and identify the native failure cause.
  - Invertaus crashed while loading `darrel.rdl`; the tombstone showed `load_level` opening a debug warning dialog through `nm_messagebox`.
- [x] Fix the first crash class and rerun the focused ZIP.
  - The metadata worker now enables `SysInputDemoNoRender` for D1 as well as D2.
  - The focused ZIP now gets past metadata and the D1 launch selector no longer rejects custom mission names that contain `Descent:`.
- [x] Run scoped formatting/build/tests and record remaining failed ZIPs.
  - The originally identified worker-crash ZIPs now pass focused runs: `Descent- Invertaus 1.2.zip`, `Colossus.zip`, `dd1lvls1.zip`, `Descend Again.zip`, and `Extra_Missions.zip`.

## Notes
- Keep the batch moving after one ZIP fails.
- Preserve concise failure JSON output for failed ZIPs.
- Prefer runner-level recovery over making each automation step handle device loss.
