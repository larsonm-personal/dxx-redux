# Mission ZIP batch failures plan

## Goal
Investigate and fix the four failures from mission ZIP batch `20260610_191752`:

- `castaway_redux.zip`: insufficient app-private free space during durable extraction
- `Descent.zip`: no non-base mission found after import
- `Lunar Series Revamped.zip`: level metadata worker crash
- `U3AAH.zip`: generic failed result

## Work phases
1. [x] Inspect per-ZIP artifacts, imports, generated metadata, resolved scripts, and runner logic.
2. [x] Fix any launcher/import/metadata classification issues that explain deterministic failures.
3. [x] Fix or gracefully handle metadata worker crash paths found from logs or repro.
4. [x] Add focused unit or script-level regression coverage where possible.
5. [x] Run scoped formatting and relevant unit tests, and rerun targeted mission ZIP checks if practical.
