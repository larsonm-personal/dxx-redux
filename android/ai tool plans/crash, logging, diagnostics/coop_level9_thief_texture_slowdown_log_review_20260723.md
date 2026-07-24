# Coop level 9 thief, texture, and slowdown log review

## Goal

Analyze the supplied D2 level 9 cooperative log for:

- thief robot position and passive/stuck behavior desynchronization
- incorrect door textures drawn over moving lava during a slowdown
- the recurring slowdown near the reactor

## Plan

- [x] Read repository instructions and establish the log timeline
- [x] Extract and classify thief ownership, AI, texture, and profiling records
- [x] Correlate the three reported incidents with source behavior
- [x] Document supported conclusions, likely causes, and missing evidence

## Findings

- The level 9 session runs from approximately 20:23:37 through 20:53:21.
- The thief starts as object 173. Its first fresh-level control claim is in AI
  wait mode 17, but it changes to attack mode 15 about one second later and is
  restored in attack mode after the automatic cooperative save restore.
- From 20:49:14.651 through 20:50:44.165, the host applies 52 accepted thief
  position corrections sent by player 1. The corrections move the host copy by
  4.74 to 8.73 world units, averaging 6.22, through segments 77, 78, 79, 85,
  117, and 128. This directly confirms material position divergence while
  player 1 owns the thief.
- Android gives the guidebot special priority in the robot position send loop,
  but does not give the thief equivalent priority. Thief ownership migration
  plus round-robin robot updates is the leading source-level explanation for
  stale remote thief placement in a robot-heavy cooperative game. The log does
  not contain enough continuous thief state to prove the exact source of its
  early visible passivity.
- A direct headless load of the same stock D2 data confirms that the four
  texture-910 references are authored in the stock level, not introduced by
  the cooperative save. They are paired sides of two portals: 491:4 to 493:5
  and 495:4 to 496:5.
- All four sides have `WALL_OPEN`, no animation clip, and no normal trigger or
  reactor trigger targeting them. `WALL_OPEN` returns `WID_NO_WALL`, so these
  side textures are unused and never submitted to the face renderer. The
  `invalid_tmaps` diagnostic is therefore a false positive because it scans all
  side fields without checking renderability. These four references do not
  explain the visible door texture over lava.
- The reactor object is in segment 492, adjacent to the first invalid-reference
  pair. That explains why the misleading diagnostic points near the reactor,
  but does not make the unused side textures renderable.
- Logged hidden-door textures use valid indices. The log does not contain a
  face tap/snapshot at the lava incident, so it does not identify the actual
  incorrectly drawn face or its texture-cache route.
- The file contains no PROFILING record, prof_v=2 record, capture start, capture
  end, slow frame, or FPS sample. Consequently it does not record the execution
  cost or exact timing of either slowdown.
- Build 98026dd2 contains both the absolute below-8-FPS trigger and the severe
  frame trigger. Automatic capture defaults off and is enabled only from the
  stored Advanced Settings preference. The exported log does not record its
  armed state. Given the reported severity and total absence of capture output,
  the leading explanation is that automatic capture was not armed in the game
  process.
- The periodic cooperative metadata heartbeat has one unusual 7.641-second gap
  from 20:47:56.232 to 20:48:03.873 instead of approximately five seconds.
  This is weak evidence of a short stall, but does not identify its subsystem
  or establish that it is either reported slowdown.

## Corrected thief analysis

- The opening scene does not show robot-heavy traffic. Object 173, the thief,
  is the only robot producing logged flare traffic until the guidebot first
  appears at 20:29:12. The earlier round-robin starvation explanation is not
  supported and is withdrawn.
- Cooperative save serialization writes every AI flag, including
  `REMOTE_OWNER` and `REMOTE_SLOT_NUM`. During restore, `StartNewLevelSub()` and
  `multi_prep_level()` clear the separate `robot_controlled[]`,
  `robot_send_pending[]`, and `robot_fired[]` runtime slot tables. The saved
  object records are then read back and restore their owner and slot flags, but
  there is no general robot-control-table rebuild afterward.
- The guidebot has a dedicated `multi_restore_companion_robot_control()` repair
  path. The log shows that path running for object 166. No equivalent repair or
  sanitization exists for the thief.
- This leaves the restored thief able to say that player 0 owns saved slot N
  while `robot_controlled[N]` is empty or later belongs to a different robot.
  Thief position sends only set `robot_send_pending[N]`; the network frame loop
  sends a robot position only when `robot_controlled[N]` names a robot. Flare
  logging occurs when a flare is queued, not when the packet is actually sent.
- A pending flare in a mismatched slot can also make
  `multi_can_move_robot()` reject further local thief AI frames until that
  shared slot happens to be cleared. This matches both the passive/stuck thief
  and the two players retaining different positions despite the thief being
  the only active on-screen robot.
- At 20:49:01 the host makes a fresh thief claim in slot 2, which repairs its
  local slot association. Ownership then moves to player 1, producing the 52
  large host-side corrections, and returns to the host at 20:50:45. This later
  behavior is consistent with valid claims re-establishing runtime slot state,
  rather than with robot-count pressure.

## Recommended thief restore repair

- Add one multiplayer restore helper that resets the complete transient robot
  control state after all saved objects and AI state have been read:
  - clear every `robot_controlled[]` entry and all pending fire/position data
  - reset the slot timers and agitation values
  - set every restored robot's `REMOTE_OWNER` to `-1` and
    `REMOTE_SLOT_NUM` to `0`
- Invoke that helper near the end of `state_restore_all_sub()`, immediately
  before `escort_rebuild_runtime_state_after_restore()`. This placement handles
  existing saves and leaves the saved AI mode, path, position, and thief state
  intact.
- Keep `escort_rebuild_runtime_state_after_restore()` after the reset. It will
  reapply the guidebot's deliberately saved coop owner and rebuild its local
  runtime slot through `multi_restore_companion_robot_control()`.
- Let the thief and ordinary robots use the existing claim protocol after
  restore. The first eligible player to activate the thief will run
  `thief_prepare_for_local_control()`, create a real runtime slot association,
  and force an initial position update.
- Add a defensive invariant in `multi_can_move_robot()`: if an object says the
  local player owns slot N but `robot_controlled[N]` is not that object, log the
  mismatch, discard the stale owner/slot, and reacquire through the normal claim
  path. This prevents any future restore or packet-ordering path from recreating
  the permanent stuck state.
- Save-format changes are not required. Sanitizing on read fixes all existing
  saves. Future coop saves may additionally write owner `-1` and slot `0`, but
  the restore-side reset remains necessary for compatibility and defense.

## Tighter design when existing saves are out of scope

- Treat `REMOTE_OWNER` and `REMOTE_SLOT_NUM` as runtime-only fields at the save
  serialization boundary. Keep their bytes in `ai_static_rw` for file-layout
  stability, but always write the neutral values `-1` and `0`.
- Implement this directly in `state_object_to_object_rw()` after copying the AI
  flags into the temporary save object. Do not mutate the live object.
- Do not add a restore-time ownership sweep. `StartNewLevelSub()` and
  `multi_prep_level()` already clear the runtime `robot_controlled[]`, pending
  packet, and timer tables. A newly written save will now load every ordinary
  robot with matching neutral object ownership.
- Continue restoring guidebot ownership exclusively from the explicit coop
  metadata. `escort_rebuild_runtime_state_after_restore()` remains the sole
  deliberate exception and reconstructs both the guidebot object owner and its
  runtime control slot together.
- The thief and all ordinary robots enter the normal claim protocol on first
  activation. There is no saved owner to reconcile and no special thief restore
  path.
- Add a debug assertion or compact coop log after restore that no non-companion
  robot has a saved owner. This detects future serialization regressions without
  adding runtime repair behavior.
- No save-format bump is technically required because the structure and byte
  count do not change. Existing saves can retain the old bug under this scoped
  requirement; all saves written after the change are clean.

## Implemented thief save fix

- `d2/main/state.c` now overwrites `REMOTE_OWNER` with `-1` and
  `REMOTE_SLOT_NUM` with `0` in the temporary `object_rw` representation after
  copying persistent AI flags. The live robot object is unchanged.
- D2 Android coop restore now logs
  `restore robot ownership payload: owned=N thief_owned=N expected=0`
  immediately after reading objects. A save written by the fixed build should
  report zero before explicit guidebot ownership reconstruction.
- No restore-time repair, legacy-save migration, save layout change, or special
  thief restore path was added.
- Lightweight validation:
  - `git diff --check` passed.
  - The Windows D2 `dxx-redux-d2-headless-metadata` target compiled and linked,
    including the modified `state.c`.
