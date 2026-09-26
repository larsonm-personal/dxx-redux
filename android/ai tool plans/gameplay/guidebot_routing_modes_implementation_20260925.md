# Original and Enhanced Guidebot routing implementation

Implements [the researched scope](guidebot_original_routing_scope_20260925.md)

## Contract

- Enhanced remains the default
- Original follows pinned Redux `9fd90f03513663ce1372c8cfa723b7a73c4219fa`
  goal selection, classic commands, doorway policy, path following, and cadence
- Retain memory safety fixes, modern controls, recall/deploy, and co-op
- Secret and Unexplored require Enhanced
- The host owns the co-op session routing mode; saves and replays retain it
- No wholesale reversal of unrelated engine changes

## Work

- [x] Add native mode state and isolate enhanced live routing hooks
- [x] Restore classic selection, commands, movement, and timing in Original
- [x] Add independent pinned-reference and live integration coverage
- [x] Integrate launcher/native settings, diagnostics, and automation
- [x] Persist mode through saves, both replay starts, and co-op lifecycle
- [x] Validate Original parity and Enhanced regression behavior
- [x] Run scoped quality and Windows D1/D2 tests/builds
- [x] Build Android and run settings/save integration to completion
- [x] Finish co-op routing validation and record remaining suite limitations

## Existing workspace changes

The worktree already contains unrelated profiling, texture diagnostic, launcher,
rendering, and bug-list edits. Preserve these and use narrow edits in shared files

## Evidence and remaining work

Implementation and relevant validation are complete. Broader suite limitations
are recorded below rather than counted as passing checks

- Frozen oracle: `android/tests/guidebot_redux_reference.c`, generated from the
  pinned Redux commit by `update_guidebot_redux_reference.py`. The test-only
  functions have prefixed names and share engine geometry/math with production
- `test_guidebot_original_navigation.ps1`: Counterstrike levels 1 and 11, two
  identical runs each, 3,695 and 7,415 checks per run. Compares keys/default goals,
  every doorway in outbound/return modes, return cadence, classic commands,
  goal/path construction and RNG, steering, and 600 successive moving frames
- The same runner exercises real native saves in both modes and real replay
  startup for both `new_level` and `save_checkpoint`. Recorded mode wins over
  an opposing default and opposing embedded checkpoint mode
- `test_guidebot_live_navigation.ps1`: Enhanced grate detour, moving-player
  return, reactor arrival, endpoint wandering and Maximum hostage cases passed,
  two identical runs each
- `test_input_demo_fixture`: passed mode roundtrip and invalid-mode validation
- Windows D1 and D2 builds passed. D1 replay fixture/recorder/replay tests passed
  (3/3); D2 also passed the native upstream save compatibility suite (4/4)
- Android debug APK built for all three configured ABIs
- `test_guidebot_routing_modes.jsonc` passed all 42 steps on emulator-5556:
  launcher preference handoff, new-game choice, unchanged active mode after a
  default change, Original overlay, Enhanced-only command rejection, Enhanced
  live route, and save/load in both modes against opposing defaults
- Android preference/export and soundfont roundtrip JVM tests passed (19/19).
  The full 1,081-test run exposed the integer preference test-helper omission,
  now fixed, plus an unrelated `TouchLayoutFormatTest` failure: the controller
  preset reaches an unmocked Android `KeyEvent.keyCodeFromString` call. The
  latter belongs to concurrent controller work and was not changed here
- Scoped mixed-language quality passed after normalizing mixed PowerShell line
  endings. Follow-up scoped checks passed; reference regeneration is byte-identical
- Two-emulator co-op verified both host selections against an opposing joiner
  default. Both modes survived the first host migration and Guidebot authority
  transfer. The full migration/rejoin runner stalls in both modes at the same
  existing reconnect handshake, so second-swap coverage is not claimed
- `test_lan.ps1 -GuidebotRoutingMode Original -LevelRestart` passed end to end:
  both peers restored the natural level checkpoint, inventory and running clock,
  then retained Original despite the joiner's Enhanced default

### Co-op reconnect limitation

On the 5556/5558 test pair, the returning former host is assigned temporary
`my_player_num=2` while the migrated master occupies slot 1 and remembers its old
slot 0 identity. The host repeatedly sends 38-byte reconnect challenges; the
returning process sends requests but no proof. The unchanged
`android_net_udp_auth_answer_challenge` rejects a challenge whose slot differs
from `Player_num`, which is consistent with this stall. Original and Enhanced
both reproduce it. This is separate from routing selection; no authentication
checks were relaxed to make the routing test pass

## Storage and session decisions

- Enhanced is the default. Android uses a launcher integer preference and an
  atomic native default handoff; desktop stores the default per pilot in PLX
- New games copy the default to a session mode. Ordinary settings do not change
  a running game. Test automation can switch a single-player session and clears
  derived routing state without clearing unrelated robots' paths
- Native D2 save version 38 appends a validated mode after cadence data. This
  covers desktop and Android, avoiding a duplicate Android-only metadata field
- Older saves use the chosen default. Frozen secret-world restores retain the
  current session mode. Replay metadata overrides both default and save mode
- Co-op mode is a host-selected Netgame field in full game info/sync packets,
  retained independently of Guidebot owner and master-slot changes. D2 protocol
  versions are now Android 30077 and desktop 30024; peers require matching builds
- Android game-info preflight accounts for the new byte and rejects invalid
  modes before parser side effects. Its actual-parser mutation probe now checks
  invalid modes too

## Fidelity boundaries and discoveries

- Preserve the pinned selector's `ConsoleObject->flags` key-check quirk
- Original retains reference closed-wall policy, ordinary velocity assignment,
  return cadence and the classic Next command's timer behavior
- The reference run exposed smoothing differences from truncating the steering
  increment before adding velocity. Original now uses the original full floating
  sum; Enhanced and other robots keep the existing deterministic integer helper
- Retain dynamic path smoothing storage, invalid-segment guards and safe cursor
  retirement for long paths. Do not reproduce reference memory corruption
- Modern controls, recall/deploy, co-op ownership and save normalization remain
  available. Secret and Unexplored commands explicitly require Enhanced
- Shared collision, fixed-point math and the rest of Redux remain current. This
  is tested routing fidelity to the pinned Redux source, not a claim of binary
  or whole-engine identity to the 1996 retail executable

## Reusable integration entry points

- `android/tests/test_guidebot_original_navigation.ps1`
- `android/tests/test_guidebot_live_navigation.ps1`
- `android/game_scripts/test_guidebot_routing_modes.jsonc`
- `android/tests/test_lan.ps1 -GuidebotRoutingMode Original -HostMigration`
  selects Original on the host and Enhanced on the joiner, then verifies the
  session choice before and after migration/rejoin/ownership changes
- `android/tests/test_lan.ps1 -GuidebotRoutingMode Original -LevelRestart`
  verifies session selection through an actual co-op checkpoint restore

The LAN runner accepts `-HostDevice`, `-HostAvd`, `-JoinDevice`, and `-JoinAvd`
to isolate device state from other active work. This validation used 5556/5558
with Nexus5X_Light_2/DxxSdk36 while another task used 5554

Host tests support `-NoBuild -BuildDir temp/guidebot-modes-build` for the isolated
validation build used while other tasks were building the shared worktree

## Follow-up: goal menu visibility

- Android reads the effective native session policy, including saves and co-op,
  through the existing overlay polling loop
- Original hides Secret and Unexplored from the Guide wheel, custom touch
  controls, and controller overflow. Enhanced restores them without modifying
  the saved layout. Secret still follows its existing reveal requirement
- A mode change releases active touch controls and closes open selectors before
  their item lists change. Classic goals, Next, Recall, and Warp remain available
- Native desktop Guidebot menus already contain no Enhanced-only goals
- Regression entry point: `android/tests/test_guidebot_routing_menus.ps1 -Install`
  checks actual UI bindings in Original, Enhanced, and after restoring an
  Original save against an Enhanced default. Focused Kotlin tests cover the
  shared filter, overflow actions, and preservation of configured bindings
- Validation: scoped formatting/lint and all 30 focused Kotlin tests passed
  (`RemainingKeyTouchActionsTest`, `GuidebotLockedWheelTest`). Android APK
  assembly was attempted twice, but the native retention startup guard refused
  while unrelated native builds/replay tests were active. The new device test
  remains unrun until APK assembly can proceed

