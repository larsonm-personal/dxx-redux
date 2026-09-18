# Maximum cooperative exit rejection

- Inspect the September 16 client/host logs and the physical exit validation path
- Identify why trigger 5 on Maximum level 18 fails the host proximity check
- Add targeted diagnostics and a regression-backed correction where evidence supports it
- Run scoped code quality and relevant builds/tests; record any device validation limits

Evidence: client log 221230 arms travel at 22:32:49.220; host log 221246 rejects player 1 with nearby=0 from 22:32:49.401 through 22:33:11.666, while granting player 0 at 22:32:52.667

## Findings

- The affected level is 18, Rosalinde (`psx18.rl2`), with exit trigger 5 on wall 41, segment 36, side 4 according to mission metadata
- The host receives the client's physical request repeatedly, so this is not a missing local trigger or a lost request
- Rejections begin while phase=0 and the generation/epoch match; the host's later exit changes phase to normal wait, but nearby remains zero even after the client adopts the new generation
- `accept_physical` validates the player's current replicated segment against the trigger wall's segment or its immediate child
- `physical_pending` persists until accepted, while normal gameplay and movement continue; the request carries the trigger number but no crossing position
- A delayed request or movement farther down the shaft can therefore lose the proximity evidence, but these logs lack player segment/position data to distinguish this from a stale or incorrect replicated position

## Diagnostic change and validation

- Record the local position, velocity, countdown, and all authored source segment/child pairs once when an exit request is queued
- Extend the existing host rejection record with its view of the player's segment and position
- Keep gameplay and protocol behavior unchanged until a trace identifies the cause; the bug remains open
- Scoped code quality passed
- CMake Android Debug x86_64 builds for both `dxx-redux-d1` and `dxx-redux-d2` passed with no compiler warnings
- No Android devices were connected during this investigation; no on-device reproduction was performed

Next evidence needed: reproduce with these diagnostics and compare the client's `physical exit requested` / `physical exit source` records against the host's first and subsequent `physical exit rejected` positions. Determine whether to preserve crossing evidence across the asynchronous request or repair remote position synchronization

## September 17 code and authored geometry inspection

The user reports only one occurrence in roughly 50 levels, so prioritize a deterministic moving-client regression over waiting for another manual occurrence

Read `psx18.rl2` directly from `max_f.hog` inside the checked-in Maximum ZIP, following the version 7 compiled-mine and wall layouts in the engine. Confirmed 212 segments, 788 vertices, and 65 walls. Wall 41 references trigger 5 at segment 36 side 4, whose child is 34

| Segment | Z extent in world units | Forward child | Exit proximity accepted |
| --- | --- | --- | --- |
| 36 | 235.09 to 245.09 | 34 | Yes |
| 34 | 245.09 to 255.09 | 119 | Yes |
| 119 | 255.09 to 276.65 | 30 | No |

The exit plane is at approximately Z=245.09. The valid post-crossing segment is only 10 units deep. Merely entering segment 119 makes every later request fail until the replicated ship returns to 36 or 34

Concrete failure sequence supported by code:

1. `object_move_one` runs `do_physics_sim` before iterating crossed segments and calling `check_trigger`. A ship can already be beyond the immediate child when the crossing is handled, even without network latency
2. `coop_travel_handle_exit_trigger` queues the trigger and calls `game_flush_inputs`. That function clears input state but does not clear ship velocity or hold position
3. A newly armed client starts with `known=0`. `coop_travel_network_frame` sends HELLO, records `last_request`, and cannot send REQUEST until the host STATE arrives and at least 250 ms have elapsed since that HELLO. Further HELLO retries can extend the wait. The host does not need this handshake for its own exit
4. `coop_travel_exit_side_blocked` closes only the trigger doorway after the crossing. It does not block segment 34 to 119, so it cannot retain the ship in the accepted vicinity
5. The request contains the trigger number, without a crossing position/history. `accept_physical` checks only the latest replicated ship segment. Position packets and requests have independent timing; a perfectly accurate newer position can cause rejection
6. The normal pending state does not pause gameplay. Retrying the trigger every 250 ms cannot recover the lost crossing evidence. The normal-wait phase after the host exits still permits position updates and still applies the same proximity test

The host log arms at 22:32:49.161 and rejects the first request at 22:32:49.401, consistent with the initial handshake delay. These timestamps are from one device; client/host wall clocks should not be directly subtracted. A 10-unit segment can be traversed in 250 ms at an average 40 world units/second; this is a threshold calculation, not a measurement of this player's speed

Existing automation does not cover this: `physical_normal_prepare` teleports to the source segment, zeroes velocity, and sends a position update; `test_coop_normal_exit_client.jsonc` then waits 1500 ms before invoking the trigger directly. The exit-race test covers competing normal/secret decisions, not movement beyond the trigger's child

Assessment: there is a concrete asynchronous crossing-validation flaw, and the authored short exit segment plus client-only handshake strongly fit this incident. The exact client position during the recorded failure remains unproven. No gameplay change was made in this follow-up

Next regression: drive a client physically across 36 -> 34 -> 119 while delaying exit authorization, verify the host has the segment 119 position, and assert that the earlier legitimate crossing still completes. Include a crossing spanning multiple physics segments. The correction should retain validated crossing evidence across authorization/retries, scoped to the current visit, player life, and transition generation, rather than merely expanding the accepted radius or removing the host checks

## Automated reproduction work

Added `test_lan.ps1 -MaximumExitProbe` and paired `test_coop_maximum_exit_{host,client}.jsonc` scripts. This is a diagnostic reproduction expecting the existing rejection, not a passing regression for corrected exit behavior

The fixture uses the checked-in Maximum ZIP and two Android x86_64 emulators. It opens the authored exit door, places the client in segment 36, and advances the real `object_move_one` physics/trigger path across the exit to segment 34 in seven 50 ms steps. Flight temporarily removes drag/thrust acceleration to isolate the crossing distance, then restores those properties and FrameTime. The pending request is delayed for eight seconds using the existing travel test fault injection. The automation pose helper then deliberately moves the client into segment 119, modeling the newer position at request receipt. Both peers must observe the client in segment 119 with no grant. Returning to segment 34 without retriggering must then grant the original request

Fixture development also showed that the post-crossing doorway lock can stop the ship in segment 34 across short physics updates, so continued coasting alone should not be assumed. A single large physics update did not complete the desired crossing in this fixture either. Neither attempt reproduced the original continuous flight path. The narrowed test isolates whether a legitimate crossing is rejected solely because the ship's later position is outside the two accepted segments; it does not prove the original movement trajectory or timing

Run command after installing the freshly built debug APK on both emulators:

```powershell
.\android\tests\test_lan.ps1 -Game d2 -MissionFile max_f -InitialLevel 18 -AllowSecretWarps -NoCoopQol -MaximumExitProbe -SkipBuild
```

Build note: the current Gradle injected-ABI build emits the fresh test-only APK under `app/build/intermediates/apk/debug/`; the older `app/build/outputs/apk/debug/` APK can remain stale. Install the former with `adb install -r -t`

### Result

- September 17 run completed with both automation results PASS and LAN runner exit code 0
- The client's real physical crossing queued trigger 5 once, at segment 34
- After the deliberate move to segment 119, the host rejected that request 25 times with nearby=0, phase=0, matching epoch=2/2 and generation=64454/64454
- Host rejection times: 08:04:13.973 through 08:04:21.307
- Returning the client to segment 34 caused the host to grant the same pending request at 08:04:21.490, without a second crossing notification
- Captured evidence: `temp/maximum_exit_evidence/summary.json`, per-peer native logs, automation logs, and PASS result JSON
- Android Debug x86_64 APK build (both native engines) passed; scoped code quality and `git diff --check` passed

This confirms the host validation failure under controlled delayed delivery and a newer out-of-vicinity position. It does not establish that the original player followed the attempted continuous-flight path. No gameplay correction has been applied, and the bug remains open


## Fix plan

- Capture the actual crossed segment/side at the local physics trigger callback
- Retain that immutable context and player life/restore serial in travel requests, including retries
- Validate the authored exit wall and existing session, nonce, epoch, participant, alive, and transition checks on the host instead of the later replicated position
- Bump Android protocol versions; preserve desktop behavior
- Convert the controlled Maximum probe into a regression requiring a grant without returning to the doorway
- Build both Android engines and run Maximum plus normal/secret arbitration regression tests

Movement remains client authoritative, as before. The crossing callback is the authority for which wall was crossed; this is not cryptographic proof against a modified client

### Implemented behavior

The Android D2 check_trigger hook supplies the authored segment/side before calling check_trigger_sub. The latter remains intercepted for legacy remote broadcasts and host-authorized replay. Only local, non-shot player crossings create pending requests. The host validates the submitted wall against its own level and applies the existing operation/session/liveness checks. A changed life or restore serial invalidates a pending local crossing and is rejected by the host

The request uses previously unused payload offsets 72/76 for segment+1/side+1 and 80/84 for life/restore serial. Packet size remains unchanged. Android D1/D2 protocol versions advance to 30066/30071; desktop protocol versions remain unchanged

### Initial fixed regression

Both peers passed the converted Maximum test. At 08:46:48.281 the host granted trigger 5 for player 1 with crossing=36:4 and current_segment=119. The client reached the postlevel screen without returning to segment 34. Evidence is in temp/maximum_exit_fixed_evidence. This covers delayed delivery and newer position ordering, not a natural-flight reproduction of the original session

### Validation

- Android Debug x86_64: both native engines and APK build passed; final incremental build emitted no warnings
- Windows: run-windows-build.ps1 -Target both passed for D1 and D2
- D2 native CTest: test_upstream_compat, test_coop_gameplay_fence, and test_coop_transition_policy all passed
- Scoped mixed-language formatting/lint and git diff --check passed
- NormalExitRace: both peers passed companion rejection, delayed secret request rejection, and shared advancement to level 9
- SecretExitRace on the final APK: both peers passed rejection of an in-flight normal request, secret loading/arrival checks, and portable-state verification
- One intermediate Android rebuild was blocked by the workspace retention idle check while the Windows build was active; rerunning after it finished passed

Race commands use test_lan.ps1 -Game d2 -InitialLevel 8 -AllowSecretWarps -NoCoopQol -SkipBuild with either -NormalExitRace or -SecretExitRace. Logs: temp/maximum_exit_normal_race.log, temp/maximum_exit_secret_race.log, temp/maximum_exit_final_build.log, temp/maximum_exit_windows_build.log, temp/maximum_exit_native_tests.log

Final Maximum rerun on the completed APK passed at 09:00:42 on both peers (host 5/5 steps, client 12/12). Host granted the recorded crossing while current_segment=119, with zero physical exit rejections. Final evidence replaces the initial fixed-run evidence in temp/maximum_exit_fixed_evidence; pre-fix evidence remains in temp/maximum_exit_evidence. Runner log: temp/maximum_exit_final_test.log
