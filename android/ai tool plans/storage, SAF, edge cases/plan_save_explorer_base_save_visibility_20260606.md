# Save Explorer Base Save Visibility

## Goal
Investigate and fix base-game save slots appearing as missing or invalid metadata after launching a mission pack such as `obsidian.zip`.

## Plan
- [completed] Trace save explorer slot discovery, save-set path construction, and Android save metadata validation.
- [completed] Identify whether the base-game saves are hidden by a mission identity mismatch, truncated metadata, or lack of metadata.
- [completed] Apply the smallest fix, preferring launcher/native save explorer code over duplicated game-core changes unless metadata writing is wrong.
- [completed] Add or update focused tests where the current test structure supports this.
- [completed] Run scoped formatting and relevant tests/build checks.
