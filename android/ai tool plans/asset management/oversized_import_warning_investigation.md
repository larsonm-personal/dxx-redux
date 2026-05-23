# Oversized Import Warning Investigation

## Goal
Explain why the current imported texture pack reports `2 of 894 textures exceed 2048px` when the previous pack did not, and identify whether the cause is pack contents or importer logic.

## Plan
1. Inspect the Android scanner/import warning path and confirm the exact limit and file types counted.
2. Enumerate the current pack entries that exceed the limit and record their dimensions and names.
3. Compare against prior converter behavior and recent pack changes to isolate what introduced the new warning.
4. Apply the narrowest fix if the current pack should not include those entries.
5. Record the result and any follow-up needed.

## Status
- [x] Scanner path inspected
- [x] Oversized entries identified enough for follow-up logging
- [x] Cause isolated
- [x] Fix applied if needed
- [x] Notes updated

## Result
- The launcher warning comes from `DxaTextureScanner`, which only counts `.png` and `.tga` entries and flags them when their rounded-up power-of-two size exceeds the engine cap of `2048`
- The current local rebuilt `d2-hires-512-textures-ktx2.dxa` does not reproduce the reported `2 of 894` signature, which means the on-device pack contents do not currently line up with the local archive state seen in this session
- Rather than guessing which remote entries triggered it, the launcher now records the mod filename, archive byte size, scan summary, and every oversized entry name with raw and rounded dimensions in the Launcher debug log when such a warning is detected on-device
- A focused JVM test now covers the scanner's oversized-entry reporting so future changes keep the diagnostic stable