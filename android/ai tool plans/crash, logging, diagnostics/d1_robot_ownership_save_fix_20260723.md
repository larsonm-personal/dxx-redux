# D1 robot ownership save fix

## Goal

Determine whether D1 saves persist object-side multiplayer robot ownership
without the matching runtime control-slot table, and apply the D2 writer-side
fix if so.

## Plan

- [x] Inspect D1 AI object serialization and multiplayer restore initialization
- [x] Neutralize saved robot owner and slot fields if the split-state bug exists
- [x] Add a compact Android coop restore verification line
- [x] Compile-check D1 and review the final diff

## Findings and implementation

- D1 had the same split-state bug as D2. `state_object_to_object_rw()` copied
  `REMOTE_OWNER` and `REMOTE_SLOT_NUM`, while `multi_prep_level()` cleared the
  separate `robot_controlled[]` runtime table.
- D1 now writes `REMOTE_OWNER=-1` and `REMOTE_SLOT_NUM=0` into the temporary
  saved AI object. The live robot remains unchanged.
- Android coop restore now logs
  `restore robot ownership payload: owned=N expected=0` after validated objects
  have been linked. New saves should report zero.
- No restore-time migration or save-format change was added.
- `git diff --check` passed.
- The Windows D1 `dxx-redux-d1-headless-metadata` target compiled and linked,
  including the modified `state.c`.
