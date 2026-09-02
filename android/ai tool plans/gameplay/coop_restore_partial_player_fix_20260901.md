# Co-op restore partial player reconstruction fix plan

## Goal

Eliminate the D1 and D2 co-op restore stack-garbage bug that can corrupt the
selected weapon and crash on the next shot. Preserve exact per-player rewind
state where it is available, reject invalid saved references safely, and audit
the neighboring restore paths that use the same partial-record pattern.

This is a planning pass only. No production or test code is changed here.

## Confirmed cause and scope

- [x] Reconstruct the crash sequence from the tombstone and exported debug log
- [x] Inventory `player` versus legacy `player_rw` fields in D1 and D2
- [x] Trace every `state_player_rw_to_player` destination and subsequent copy
- [x] Classify omitted fields as saved gameplay state, live session state, or
      derived runtime state
- [x] Evaluate fix strategies for compatibility, exact rewind semantics, and
      D1/D2 parity
- [x] Identify validation, diagnostics, and regression coverage needed
- [x] Write a phased implementation and verification plan

The immediate crash is deterministic undefined behavior, even though its value
depends on stack contents:

1. Both co-op restore implementations allocate `restore_players[MAX_PLAYERS]`
   without initialization.
2. `state_player_rw_to_player()` expands each legacy `player_rw` into a modern
   `player`, but only assigns fields present in the legacy disk record.
3. Android's `coop_restore_player_game_state()` copies the whole partially
   initialized `player` into the live player and then restores only the
   network-session fields.
4. D2 therefore copies an indeterminate `secondary_weapon`. Firing a missile
   later reaches `d2/main/laser.c` with a value outside the weapon array and
   fails the `weapon < MAX_SECONDARY_WEAPONS` assertion. D1 has the same unsafe
   construction and firing pattern.

The energy field is present in `player_rw` and is assigned by the converter.
There is no matching uninitialized-energy defect in this path. Treat the
reported zero energy as normal gameplay unless a separate reproducible trace
shows otherwise.

## Field ownership decision

| State | D1 | D2 | Restore policy |
| --- | --- | --- | --- |
| Legacy inventory, ammo, energy, shields, score, flags, statistics | In `player_rw` | In `player_rw` | Restore from the base save |
| Selected primary and secondary weapon | Omitted | Omitted | Save per player in Android co-op metadata, validate, then restore |
| Afterburner charge | Not applicable | Omitted | Save per player in Android co-op metadata, clamp or reject invalid values, then restore |
| `KillGoalCount` | Omitted | Present | Add to shared co-op metadata for D1 parity, or explicitly reset if product semantics do not require exact rewind |
| Callsign, address, connection, object slot, packet counters | Live session | Live session | Preserve from the current network session |
| Shield delta, timestamp, hour rollover, certainty | Omitted transient display state | Omitted transient display state | Initialize to neutral values, do not serialize |
| Object transform and physics | Saved object array | Saved object array | Copy only from a validated player object |

The Android metadata already snapshots every active player's inventory from the
host's cached `Players[]`. Ship-status updates include selected weapons in both
games and D2 afterburner charge, so the host has the values needed to make a
co-op rewind exact without changing the base D1/D2 save ABI.

## Rejected shortcuts

- Only zeroing `restore_players` prevents the crash but resets all restored
  selections to weapon zero and loses D2 afterburner state.
- Preserving the current live selection avoids garbage but is not a rewind. It
  retains state acquired after the snapshot.
- A firing-site assertion alone is too late. It also misses negative indices,
  which pass the current upper-bound-only assertion and can index before an
  array.
- Extending `player_rw` would change the upstream save format. The Android
  co-op trailer is the appropriate disposable extension point.

## Phase 1: deterministic legacy reconstruction

- [ ] In both `d1/main/state.c` and `d2/main/state.c`, make
      `state_player_rw_to_player()` produce a fully initialized destination
      before assigning legacy fields. Document that omitted modern fields get
      neutral defaults.
- [ ] Confirm every converter caller remains correct with this contract:
  - normal local restore reads selected weapons separately;
  - D2 reads afterburner separately;
  - D2 secret-level death restore no longer copies indeterminate fields from
    `dummy_player`;
  - co-op restore receives safe defaults before Android metadata is applied.
- [ ] Initialize co-op `restore_objects` storage in both games and add an
      explicit valid-object mask. Do not use zero-filled object contents as an
      implicit validity signal.
- [ ] Before evaluating `Objects[restore_players[i].objnum]`, validate the
      object index against the loaded object bounds and require `OBJ_PLAYER`.
- [ ] Pass object validity into the shared remapper. A saved player with no
      valid player object must be treated as unavailable and spawned fresh,
      with a concise co-op debug-log entry.
- [ ] Make the desktop co-op remap branches use the same validated source
      conditions so this undefined behavior is not Android-only.

## Phase 2: exact per-player co-op snapshot fields

- [ ] Bump the disposable Android co-op metadata and footer version from v5 to
      v6. Replace the format directly, with no compatibility reader or migration.
- [ ] Extend `coop_player_record` with fixed-layout fields shared by D1 and D2:
  - selected primary weapon;
  - selected secondary weapon;
  - D2 afterburner charge, stored as zero by D1;
  - D1 `KillGoalCount` if exact same-level rewind semantics are confirmed.
- [ ] Populate the fields in `coop_snapshot_player()` for active and remembered
      players.
- [ ] Apply these fields only after identity-to-saved-slot mapping succeeds.
      Keep session identity preservation separate from saved gameplay state.
- [ ] Centralize restore-boundary validation:
  - require `0 <= primary_weapon < MAX_PRIMARY_WEAPONS`;
  - require `0 <= secondary_weapon < MAX_SECONDARY_WEAPONS`;
  - check selection consistency against ownership and ammo, preserving a legal
    empty selection if the engine permits it and otherwise choosing a
    deterministic owned fallback;
  - constrain D2 afterburner charge to its legal range;
  - log the saved value and fallback whenever sanitization occurs.
- [ ] Apply local selection without producing normal selection HUD messages,
      rearm delays, or multiplayer sends. Update any local derived selection
      bookkeeping required by each game.
- [ ] Reuse the validated application path for absent-player rejoin and level
      restart so those routes cannot reintroduce invalid selections.

## Phase 3: defense at unsafe consumers and packet boundaries

- [ ] Add a D1/D2 runtime guard before secondary weapon array access in
      `do_missile_firing()`. Check both lower and upper bounds, log Android
      context, choose a safe fallback or abort the shot, and retain an assertion
      for debug builds.
- [ ] Add the equivalent validation before restore-time calls to
      `select_weapon()`, since that function indexes weapon tables before it can
      repair a bad value.
- [ ] Audit primary-fire, gauges, autoselect, bomb selection, and HUD consumers
      for direct indexing by selected weapon. The restore boundary remains the
      primary guarantee, but externally sourced values should not reach these
      sites unchecked.
- [ ] Validate D1/D2 ship-status receive fields before assigning remote selected
      weapons or afterburner state. Include packet length, player index,
      authenticated sender-to-player mapping, weapon ranges, and fixed-point
      range checks. This prevents malformed network state from being captured
      into a later host snapshot.

## Phase 4: adjacent restore deficiencies

- [ ] Replace ignored reads of `player_rw` and the co-op player/object blocks
      with exact-read checks in both games. Do not byte-swap or convert a short
      read.
- [ ] On a failed or inconsistent co-op block, fail the restore cleanly or mark
      the affected player unavailable before mutating live player state.
- [ ] Audit other stack or heap destinations passed to partial disk-record
      converters. The confirmed D2 `dummy_player` case is fixed by Phase 1;
      record any additional converter with an incomplete-output contract.
- [ ] Keep a broader transactional save-reader rewrite separate. The current
      reader mutates world state while parsing, so full rollback after a late
      failure is valuable but is larger than this crash fix.

## Phase 5: co-op runtime-state ownership audit

The rewind buffer is authored by the host. World simulation state should come
from that buffer, but several saved globals describe the local console rather
than the shared world. Applying the host's copy to every client can be safe yet
semantically wrong.

- [ ] Classify D1/D2 saved globals as world-authoritative, per-player, or
      per-console. Start with fire cooldowns and counts, fusion charge, D2 omega
      charge, super-weapon preferences, missile-gun parity, spreadfire toggle,
      helix orientation, and mine-drop counters.
- [ ] For per-player state already reported to the host, add it to the v6 player
      record and restore by stable player identity.
- [ ] For true per-console state not known by the host, choose explicitly among:
  - extend the co-op protocol so the host caches it for future snapshots;
  - reset it to a documented neutral value on clients;
  - preserve live client state only where exact rewind semantics do not matter.
- [ ] Do not duplicate world RNG, object allocator, or other authoritative
      simulation state into per-player metadata.

This phase should be a separate implementation tranche after the stack-garbage
fix, because it changes multiplayer rewind semantics rather than memory safety.

## Phase 6: regression coverage

- [ ] Extend `android/tests/test_coop_player_session.c` with poisoned-source and
      poisoned-destination cases. Verify gameplay fields are deterministic and
      all live session fields remain intact.
- [ ] Add shared metadata tests for D1 and D2 profiles covering:
  - valid boundary weapon indices;
  - negative and `MAX_*` weapon indices;
  - unowned or empty selected secondary weapons;
  - D2 afterburner minimum, maximum, and invalid values;
  - D1 `KillGoalCount`, if included;
  - metadata checksum and v6 size/version rejection.
- [ ] Add restore-object tests for negative, too-large, non-player, duplicate,
      and valid object references.
- [ ] Add short-read tests proving no partially read `player_rw` is committed.
- [ ] Extend introspection only as needed to expose selected weapons,
      afterburner, remote player snapshots, and restore sanitization counters.
- [ ] Add a maintained two-emulator D1/D2 co-op rewind scenario:
  1. give host and client distinct selected primary and secondary weapons;
  2. set distinct inventories and D2 afterburner charge;
  3. snapshot, alter the state, and rewind;
  4. assert both players match the snapshot by stable identity;
  5. fire primary and secondary weapons on host and client;
  6. advance or restart the level and assert the session remains connected.
- [ ] Add a focused native regression that fills reconstruction storage with a
      nonzero poison pattern, converts a `player_rw`, and proves every omitted
      field receives its documented neutral value.

## Phase 7: verification and handoff

- [ ] Run scoped formatting and linting on all changed files with
      `android/run-code-quality.ps1 -Fix -Paths ...`.
- [ ] Run the focused native co-op helper and metadata tests for both D1 and D2.
- [ ] Build the Android debug APK with JDK 21 and run the two-emulator scenario.
- [ ] Run `run-windows-build.ps1 -Target both` to protect desktop D1/D2 builds.
- [ ] Run the relevant CTest suites and `git diff --check`.
- [ ] Review the final diff for minimal mirrored D1/D2 changes, Android-only
      metadata changes, preserved handmade comments, and no unrelated edits.

## Recommended tranche order

1. Implement Phases 1 and 3's firing/restore guards first. This closes the
   memory-safety hole even if metadata work is delayed.
2. Implement Phase 2 in the same release so rewind restores exact per-player
   selection and D2 afterburner state rather than merely choosing safe defaults.
3. Implement Phase 4 exact-read and reference validation while the restore code
   is already under test.
4. Complete Phase 6 and Phase 7 before merging.
5. Treat Phase 5 as a follow-up multiplayer determinism tranche with its own
   captures and tests.

## Expected files in the core implementation

- `d1/main/state.c`
- `d2/main/state.c`
- `d1/main/laser.c`
- `d2/main/laser.c`
- `android/app/src/main/cpp/shared/coop/coop_save.h`
- `android/app/src/main/cpp/shared/coop/coop_save.c`
- `android/app/src/main/cpp/shared/coop/coop_player_session.h`
- focused files under `android/tests/`
- one maintained multiplayer automation script under `android/game_scripts/`

Additional D1/D2 multiplayer files should be added only if the ship-status
validation audit confirms the checks are not already centralized elsewhere.

## Constraints

- Preserve the base D1/D2 save ABI and desktop compatibility.
- Keep new co-op behavior shared under `android/` where practical.
- Mirror the few required safety hooks in D1 and D2.
- Treat the Android co-op metadata as disposable before release.
- Do not broaden this fix into the full save parser transaction rewrite.
