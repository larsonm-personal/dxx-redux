# Plan: D1/D2 Game Control Save/Load Extraction 2026-07-11

## Goal
- Remove the paired Android save/load and meta-action dispatch bodies from the original D1/D2 `gamecntl.c` files while preserving game-thread ordering and behavior

## Existing work to preserve
- Preserve the completed songs, PhysFS, and HMP diff-minimization tranches
- Preserve all unrelated workspace and mission metadata changes
- Keep desktop game-control behavior unchanged

## Steps
- [x] Reconfirm the live D1/D2 dispatch blocks, includes, flags, and signature differences
- [x] Define the smallest direct API in the existing Android meta-actions helper
- [x] Move the duplicated Android-only behavior and leave mirrored minimal call sites
- [x] Add or extend focused unified integration coverage for save/load dispatch behavior
- [ ] Run scoped formatting, lint, catalog validation, and diff checks
- [ ] Build both Windows targets and all configured Android ABI/game combinations
- [ ] Run focused D1/D2 emulator integration coverage
- [ ] Record exact reduction, deferred behavior, and the next candidate

## Guardrails
- Preserve dispatch priority across autosave, difficulty, game menu, save, load, demo toggle, and rewind
- Preserve pause-window close behavior and request-flag ownership
- Preserve dead-player and competitive-multiplayer restrictions
- Preserve auto-exit, auto-minimize, and autosave result semantics
- Preserve D1/D2 state function signatures through compile-time adapters
- Preserve D2 guide-bot ordering and D1 music-control ordering around the extracted handler
- Do not broaden this tranche into save-format, rewind-policy, or menu behavior changes

## Baseline
- Aggregate D1/D2 diff: 341 files, +49969/-3880 against `upstream/main`
- `d1/main/gamecntl.c`: +455/-9
- `d2/main/gamecntl.c`: +728/-13
- Paired Android dispatch blocks: 155 additions per game
- Expected core engine-file reduction: 310 additions
