# D1/D2 startup pilot and resume extraction plan

## Goal

Move the duplicated command-line lookup, Android pilot selection, and startup save-restore orchestration out of the upstream-original D1/D2 `inferno.c` files while keeping D2 classic-demo dump parsing local.

## Baseline

- `d1/main/inferno.c`: 154 additions, 11 deletions versus `upstream/main`
- `d2/main/inferno.c`: 213 additions, 11 deletions versus `upstream/main`
- The command lookup and pilot/resume functions are identical except for the `d1`/`d2` diagnostic tag
- Startup resume is cross-platform; Android additionally prepares the pilot, logs callsign transitions, and flushes inputs after success

## Work

- [ ] Add an unconditional `startup_resume_shared.c/.h` source pair
- [ ] Move command lookup and startup resume orchestration into the shared source
- [ ] Move Android `-pilot` application into the shared source
- [ ] Pass only the D1/D2 game tag to preserve diagnostic text
- [ ] Keep D2 classic-demo argument validation and JSON dump policy local
- [ ] Replace local bodies and update call sites in both games
- [ ] Wire the source into Windows and Android D1/D2 targets
- [ ] Run scoped code quality
- [ ] Build all Android ABIs
- [ ] Build Windows D1 and D2
- [ ] Run resume tests when the shared emulator is free
- [ ] Record exact inherited-file reduction and update campaign documents

## Risk controls

- Preserve argument scanning order and case-insensitive matching
- Preserve missing-path, pilot-preparation, restore, failure, and input-flush ordering
- Keep the same engine `state_restore_all_path` call and return value
- Do not move or reinterpret `-classicdemo-dump-json`
