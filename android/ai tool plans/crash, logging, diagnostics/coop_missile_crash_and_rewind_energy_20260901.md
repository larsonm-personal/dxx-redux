# Co-op missile crash and rewind energy investigation

## Scope

Diagnose the reported Android co-op crash and the possibly related rewind-to-zero-energy behavior without changing production code.

## Plan

- [x] Locate and inventory the supplied xCrash artifact
- [x] Identify the crashing thread, signal, native frames, and relevant preceding logs
- [x] Trace the implicated crash path through the current source and assess whether missile firing is causal
- [x] Trace rewind state restoration and energy synchronization in co-op
- [x] Correlate findings, record confidence and missing evidence, and recommend focused follow-up diagnostics or fixes

## Constraints

- Preserve all existing user worktree changes
- Make no production code changes during this research pass

## Findings

- Build `21493` at commit `224e7200` aborted on the game thread at
  `d2/main/laser.c:2733` because the host's selected secondary weapon was at
  least `MAX_SECONDARY_WEAPONS` (`10`)
- Virtual joystick button `101` is mixer button `100` plus the secondary-fire
  binding `1`; its down event occurs three milliseconds before the assertion,
  proving that secondary fire exposed the invalid state
- Co-op rewind serializes and restores the complete player record, including
  energy and selected secondary weapon, without validating those scalar values
- The tombstone contains repeated invalid multiplayer robot control-slot
  diagnostics before the abort, which is additional evidence of inconsistent
  restored multiplayer state
- This build also applied a client restore from inside the UDP packet handler;
  commit `68f71bae`, made after the crash, moved that application to a frame
  boundary because restore tears down and rebuilds the current level
- The tombstone's logcat tail does not include the rewind. It shows an energy
  pickup raising the host to 92 energy 216 milliseconds before the crash, so
  the earlier reported zero-energy state cannot be reconstructed from this
  artifact
- Rewind has no intentional zero-energy rule. It restores the raw energy from
  the selected snapshot, so zero can mean either that the chosen snapshot
  already held zero or that the co-op restore selected or reconstructed bad
  player state
- The referenced private debug log
  `debuglog_20260831_222856.txt` is named in the open-file list but is not
  embedded in the xCrash file. That export is the missing evidence needed to
  distinguish the energy branches and correlate the rewind with the invalid
  selected weapon

## Exported debug-log follow-up plan

- [x] Locate and inventory the exported `debuglog_20260831_222856.txt`
- [x] Reconstruct the rewind capture, transfer, restore, and post-restore timeline
- [x] Track logged energy, selected weapon, player remapping, and multiplayer diagnostics
- [x] Refine the root-cause assessment and record remaining evidence gaps

## Exported debug-log findings

- The host completed two co-op restores from `Players/__rewind__.d2sg`, at
  `22:30:16.653` and `22:30:42.907`, before advancing normally from Castaway
  level 2 to level 3 at `22:32:11.997`
- Guide-Bot game time shows the first rewind moved from about 35 seconds to 20
  seconds and the second from about 43 seconds to 25 seconds
- The crash then occurred on level 3 at `22:33:21.956`, about 2 minutes 39
  seconds after the second rewind
- The direct crash root is deterministic in the co-op restore implementation:
  `restore_players` is an uninitialized stack array; `state_player_rw_to_player`
  populates only fields represented by the legacy `player_rw` save record; and
  `coop_restore_player_game_state` then copies the entire partially initialized
  `player` over the live player
- `player_rw` does not contain `primary_weapon`, `secondary_weapon`,
  `afterburner_charge`, or several newer runtime fields. In particular, each
  rewind replaces the valid selected secondary weapon with indeterminate stack
  data. The later secondary-fire assertion is the expected consequence when
  that byte happens to be 10 or greater
- Energy is present in `player_rw` and is explicitly populated during restore,
  so the same uninitialized-field defect does not directly explain zero energy
- Only Guide-Bot, Texture, and Profiling categories were present in the export.
  It contains no per-player capture/remap energy values, so it cannot prove
  whether the selected rewind snapshot already contained zero energy or the
  wrong saved player record was selected
