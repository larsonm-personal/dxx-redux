# Co-op secret warps and briefings: player testing handoff

Updated 2026-09-14. Implementation is ready for player testing and iteration.
The user explicitly asked to stop expanding edge-case coverage after finishing
the cross-world save/load check. Start next session with player feedback;
do not automatically resume every unchecked item in the longer plans.

## Client departure during travel (2026-09-16)

Remaining players now continue after a non-host client disconnects during secret
travel. Captured rosters stay stable; live transfer/acknowledgment waits shrink.
Host loss still ends the session. See [implementation and test notes](coop_travel_disconnects.md)
for the five tested phases, commands, evidence, and remaining validation limits.

## Faster secret travel (2026-09-16)

Player feedback supersedes the original seven-second warning requirement:
secret entry, return, and advancement after a destroyed base should start loading
as soon as every participant acknowledges the prepared campaign. Freeze,
checkpoint/transfer, destination application, and release barriers remain intact.
Normal-exit evacuation and briefing deadlines are unchanged.

Implementation: remove the production warning delay in `coop_transition_policy.c`.
The retained frozen-state/fault-injection scenarios explicitly request
`coop_travel_test=warning_delay`; that diagnostic setting resets when travel is
rearmed. Ordinary play has no fixed warning countdown. Policy coverage checks
immediate loading after the last preparation ACK and continued blocking until
all load/release ACKs arrive.

Removing the delay exposed packet ordering at the preparation/load boundary:
the world-transfer BEGIN can arrive before the host's LOADING snapshot. Once
`prepared_ready` is true, allow buffering in CAPTURING; applying still requires
LOADING. Otherwise the client rejects BEGIN and the host times out after 60s,
then rolls everyone back. The secret-advance LAN case exercises this path.

Test setup note: the provisioning helper selects `build/outputs/apk/debug`,
which can be stale when Gradle writes `build/intermediates/apk/debug`. Install
the freshly built APK explicitly with `adb install -r -t` on both emulators.
The first cold-launch attempt also exceeded the test's 60-second initial-sync
limit while the client was still in launcher preflight.

Validation completed 2026-09-16:
- Both Windows builds and both transition-policy test executables passed
- Android x86_64 debug assembly passed (ARM64 was not rebuilt in this check)
- Two-emulator `test_lan.ps1 -Game d2 -InitialLevel 8 -SecretAdvance
  -AllowSecretWarps -NoCoopQol -TimeoutSeconds 180 -SkipBuild` passed at 17:29:57
- The host staged the next mine at 17:29:48.449 and entered LOADING at
  17:29:48.488: 39 ms, with no seven-second warning phase
- This exercised Counterstrike's equivalent destroyed-base progression path;
  the exact Descent Maximum secret-4 to level-16 route remains a player retest

Evidence: `temp/coop-fast-warp-lan-resumed.log`,
`temp/coop-fast-warp-emulator-5554-passed-native.txt`,
`temp/coop-fast-warp-emulator-5556-passed-native.txt`,
`temp/coop-fast-warp-windows-verified.log`, and
`temp/coop-fast-warp-android-verified.log`.

## First player feedback (2026-09-14)

- Movie input correction: fullscreen movie taps must not dismiss playback.
  The upper-right Skip dismisses only that movie and leaves subsequent robot
  briefing pages available. Page taps keep their existing advance behavior;
  host force-launch still closes the full presentation. Validate with the
  paused-force LAN case, including an ordinary movie tap before pausing and
  explicit movie Skip followed by the still-active briefing pages.
  This case passed on two emulators at 12:29:17 on 2026-09-14 using
  `test_lan.ps1 -Game d2 -Briefings -BriefingCase paused_force -NoCoopQol
  -TimeoutSeconds 180 -SkipBuild`. The host's ordinary tap left the actual
  Counterstrike movie running; movie Skip preserved briefing pages and did not
  enable Launch now. Subsequent briefing Skip and host force-launch still
  released both players, including a client paused in its movie. Android
  ARM64/x86_64 assembly and both Windows builds passed. Evidence:
  [test log](../../../temp/coop-movie-only-live.log),
  [Android build](../../../temp/coop-movie-only-build.log),
  [Windows builds](../../../temp/coop-movie-only-windows.log).
  The same movie handler is used in single-player, but a separate single-player
  device run was not performed. Temporary emulator movie libraries were removed.
- New host preferences now default both briefings and secret warps to on.
  Existing explicit choices remain authoritative. Creating a game saves both
  choices through the same preference path as the other host switches; config
  export uses the same initial defaults.
- Investigated `Downloads/debuglog_20260914_120539.txt` (client) and
  `Downloads/debuglog_20260914_120540.txt` (host), supplied as nested paths but
  found under those flattened filenames. The client started synchronization at
  12:06:43.031, received Android Back (`akey=4`, SDL Escape) down/up at
  12:06:44.917, and cancelled at 12:06:44.940. The host completed synchronization
  at 12:06:45.946 with one player. `net_udp_sync_poll` returns cancellation on
  Escape, matching this sequence. These logs do not establish an options-off
  startup defect or explain what generated the Back event. No network behavior
  was changed based on this report. If it recurs without Back input, retain that
  reproduction and logs for a focused follow-up.

## Where we stopped

- D2 has an independent **Allow secret area warps** server option beside co-op
  QoL. Secret entry/return shows yellow status text while preparing/loading
  and moves the team together as soon as everyone is ready.
- The host resolves competing exits. A normal-exit winner blocks the secret
  exit but does **not** evacuate or freeze the team: the first exiter waits,
  while the others escape or die under the live reactor countdown.
- Settled normal/secret saves include campaign and dormant-world state.
  Save/load and rewind are blocked during unsafe intermediate phases.
- Optional D1/D2 briefings show each player's page N/M and a separate end/Skip
  signal. Different totals are fine; content hashes/agreement are intentionally
  removed and should not be reintroduced.
- Briefings have a visible two-minute maximum, shortened to at most 20 seconds
  when the host finishes or skips, without extending the original deadline.
  A separate **Launch now** overlay button requires a fresh tap and closes
  everyone's remaining presentation. Players enter the mine together.

## What to test first

- Enable secret warps, enter and return, and check the loading status and team arrival.
  Also check that disabling the option prevents secret entry.
- Try competing normal/secret exits, particularly after destroying the main
  reactor. Confirm normal-exit waiting retains the ordinary evacuation behavior.
- Save/load in both worlds, restart the app and resume, then use the return
  teleporter. Try rewind within a settled visit and check that transition-time
  state changes are rejected cleanly.
- Enable briefings and try reading, skipping, and paused videos. Check page
  counts, the two-minute/20-second timers, and deliberate use of **Launch now**.

For a bug report, retain the mission/level, host/client roles, enabled options,
action order, and any relevant save or native logs. Use the smallest reproduction
to guide the next fix rather than reopening the whole acceptance matrix.

## Deferred work

These are remaining validation areas, not claims that the corresponding
features are entirely missing:

- Larger teams and ordinary observers. Observing-host controls already have
  coverage; that does not establish every observer/roster combination.
- Remaining reconnect, host-migration, and interrupted-loading combinations.
  Several loss/recovery cases already passed; consult the detailed evidence
  before duplicating them.
- Device rotation, overlay recreation and broader multi-touch coverage.
- A full custom-mission and single-player regression sweep.

Prioritize actual player-reported problems next week. Broader stress testing
can be selected later if useful; it is not a prerequisite for trying the feature.

## Final verification

The last gap was loading a normal save while in a secret mine, then loading
the secret save from the normal mine. The two-emulator D2 run passed at
09:47:58 on 2026-09-14. It saved normal level 8 in slot 1 and secret -2 in
slot 0, restored secret-in-place, loaded both directions, checked both players'
distinct saved scores/ammunition and campaign checksums, then returned through
the real teleporter to base 8. No production fix was needed for this final case.
Android x86_64 assembly, both Windows builds, and scoped formatting passed.

Reusable command from the repository root, with the current APK installed:

```powershell
.\android\tests\test_lan.ps1 -Game d2 -InitialLevel 8 -SecretCrossRestore -AllowSecretWarps -NoCoopQol -TimeoutSeconds 180 -SkipBuild
```

## Expanded material and evidence

- [Secret teleporters: design, implementation history, recovery and acceptance cases](coop_secret_teleporters.md)
- [Briefings: timing, UI, synchronization and deferred lifecycle cases](coop_briefings_and_launch.md)
- [Reusable LAN integration runner](../../tests/test_lan.ps1)
- [Final cross-world test result](../../../temp/coop-cross-restore-live.log),
  [host native log](../../../temp/coop-cross-restore-host-logcat.log), and
  [client native log](../../../temp/coop-cross-restore-client-logcat.log)
- [Android build](../../../temp/coop-cross-restore-build.log) and
  [Windows builds](../../../temp/coop-cross-restore-windows.log)

The longer plans contain historical failures, superseded designs and stale
unchecked audit items. Their top-of-file handoff notes and this document define
the stopping point. Logs under `temp/` may be removed by cleanup; the result
summary above and reusable test remain the durable handoff.
