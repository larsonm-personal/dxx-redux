# Mid-level join regression investigation

## Evidence

- Client log: `debuglog_20261002_230645.txt`, host log: `debuglog_20261002_230704.txt`, build 23830 / 71739b5b
- Initial client wait ends at 23:07:54.865 immediately after controller key 97, before the host starts the level at 23:07:56.188
- Host restores level 5 alone; restore diagnostics show a connected empty slot outside the live one-player roster
- First subsequent join times out awaiting approval; next attempt is accepted at 23:09:38.443, prompts again at 23:09:40.455, and is accepted again at 23:09:42.449 but ignored because object transfer is busy
- Recovery freeze messages time out and saves fail after that acceptance, consistent with a recovery barrier waiting for a nonexistent peer

## Plan

- [x] Trace live roster restoration, recovery freeze membership, and pending join retries in both engines
- [x] Add targeted diagnostics and regression coverage for the evidenced failure
- [x] Fix the cause without weakening real peer synchronization or transition isolation
- [x] Run scoped formatting, relevant CMake builds/tests, and emulator integration
- [x] Record findings and validation limits

## Cause and changes

`net_udp_remove_player` reduced `N_players` after a lobby cancellation without
disconnecting the vacated `Players` slot. Lobby selection also cleared unused
names without clearing connection flags. The solo host consequently saved and
restored an empty connected slot. Recovery freeze scanned `MAX_PLAYERS`, so it
waited for that nonexistent player to acknowledge before starting object sync.
The freeze also blocked subsequent autosaves and rewind captures.

The transition join implementation keeps the new identity unpublished until
successful SYNC. During the blocked transfer, request retries therefore reached
the approval prompt again. Repeated approvals called welcome while it was busy.

- Both engines disconnect vacated and deselected lobby slots
- Recovery freeze takes participants only from `N_players`
- An active object/extras transfer defers approval prompts until it completes
- Recovery diagnostics record the actual acknowledgement mask and roster size
- Introspection exposes pending approval so the integration test approves once

The native recovery regression fails before the fix in both games and passes
afterward. Existing late-pickup coverage still verifies that a real connected
peer must acknowledge and that its partial collection is preserved.

The first join ends immediately after controller key 97 (B), before host start;
there is no sync rejection in that attempt. The next attempt times out before
the host's approval flag is set. The final attempt is accepted twice and stalls
in the recovery barrier described above.

## Validation

- Windows CMake builds complete for D1 and D2
- Recovery, player-session, reconnect-authentication, and initial-sync-retry
  CTests pass for both games (eight tests total)
- Scoped mixed-language formatting/lint passes
- Android x86_64 debug APK build passes with existing upstream warnings only
- Two-emulator saved-late-join coverage now includes death spew, lobby
  cancellation, solo restore, a single approval, gear recovery, and save readiness
- D2 paired-emulator regression passes: cancellation removes slot 1, solo restore
  reports one live player, recovery freeze waits on mask `0x0`, and one approval
  restores plasma, six homing missiles, and laser level 2
- D2 evidence: `temp/midlevel_join_regression/d2/`
- D1 paired-emulator regression also passes with one approval, restored gear,
  and save readiness; evidence: `temp/midlevel_join_regression/d1/`
- D2 interrupted-transfer integration passes: partial level-1 object transfer
  is cancelled, the client follows evacuation, flyout, scores and briefing,
  and the same attempt joins level 2 with one retry and 13 object packets
- Transition evidence: `temp/midlevel_join_regression/d2-transition/`
- Final scoped formatting/lint and `git diff --check` pass

Validation used an x86_64 debug APK on two emulators. The supplied phone logs
were analyzed, but the fix has not been installed on those phones. The first
emulator test run exposed a fixture parameter error (the death step was
filtered out); the corrected scenario passed to completion in both engines.
