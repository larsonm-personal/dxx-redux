# Co-op Guide-Bot cage investigation

- [x] Trace the cage-release and control-wheel spawn paths
- [x] Trace Guide-Bot control availability and multiplayer ownership/state synchronization
- [x] Identify the likely failure mode and supporting evidence
- [x] Decide whether diagnostics or a fix are in scope
- [x] Determine validation required for this investigation
- [x] Record conclusions and completed validation here

## Findings

- The touch wheel's `Locked` state reads `Buddy_allowed_to_talk` through
  `nativeIsBuddyReleased()`; it does not test object existence or motion
- `escort_spawn_at_player()` explicitly sets `Buddy_allowed_to_talk = 1` and
  sends the co-op ownership request/state, so control-wheel deployment is reliable
- Destroying the cage wall does not directly set release state. The state changes
  only when `ok_for_buddy_to_talk()` happens to run later
- `do_escort_frame()` does not poll `ok_for_buddy_to_talk()` during normal
  companion updates. Therefore the companion can leave its opened cage while
  `Buddy_allowed_to_talk == 0` and `Escort_owner_player == -1`
- In co-op, the host is allowed to simulate an unowned companion while joiners
  return before companion AI. Normal multiplayer robot claiming/position traffic
  can consequently keep the bot moving even though Guide-Bot ownership was never
  assigned and the wheel remains locked
- Ownership state packets would repair the release flag on every peer, but the
  host sends the initial ownership packet only from `ok_for_buddy_to_talk()`
- Existing co-op Guide-Bot automation used the direct spawn command and did not
  exercise physical cage destruction, leaving this split untested
- The fix now notifies Guide-Bot state when either an immediate or animated
  blastable wall finishes destruction. In co-op, only the host evaluates release
  and assigns the initial owner
- Per-frame release polling was rejected because it incorrectly unlocks a
  naturally free Guide-Bot at level start
- The focused automation begins with the Guide-Bot locked, destroys its adjacent
  blastable walls through the debug adapter, and verifies that it becomes released

## Implementation

- [x] Recognize physical cage release from the wall-destruction event
- [x] Add focused regression coverage for cage release and co-op ownership
- [x] Run scoped formatting and the focused Android automation test
- [x] Run the required Android and Windows build/test verification
- [x] Record final validation and limitations

## Validation

- Scoped `run-code-quality.ps1 -Fix` passed for the Android automation change
- `test_guidebot_cage_release.json5` passed on the Android emulator
- Android `:app:assembleDebug` passed for all configured ABIs
- Windows D2 build passed through `run-windows-build.ps1 -Target d2`
- D2 native CTest passed all 26 tests
- The paired co-op ownership test could not reach gameplay because the second
  managed AVD timed out during SetupActivity startup. The host script now covers
  the cage-release path, but end-to-end two-peer validation remains outstanding
