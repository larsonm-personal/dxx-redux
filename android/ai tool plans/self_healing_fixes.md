# Self-healing fixes

Implement the six findings in `self_healing_audit.md`, plus bounded launch preparation where practical. Preserve healthy game sessions and valid user data. Do not edit `android/outstanding_bugs.md`.

## Work and verification

- [x] Shared game-process readiness for Play, Resume, demos, automation, and multiplayer
- [x] LAN supervisor survives failed socket reopen and explicit retry checks transport health
- [x] Matchmaking connection generations fence obsolete callbacks/authentication/probes
- [x] Mission transfer cancellation closes I/O, serializes file ownership, and rejects obsolete grants/results
- [x] Launch failure clears guards, rolls back owned resources and lobby start state, and informs clients
- [x] Proxy setup/forwarding failure reaches the session owner and leaves a clean retry path
- [x] Launch preparation has bounded failure/retry behavior without unsafe overlap of blocking work
- [x] Focused regression tests for failed attempt A, immediate retry B, and late A results
- [x] Scoped formatting, Android JVM tests, native D1/D2 build, and reusable emulator integration tests

Record implementation evidence and validation results below as work completes.

## Progress

- Shared launcher handoff now performs orphan-process readiness for all new engine launches; multiplayer keeps its earlier preflight as well
- LAN watchdog owns a separate scope, survives failed socket binds, and retries visibly; explicit discovery requests validate socket/receiver health
- Download cancellation closes the TCP socket and holds a writer mutex until blocking work retires; request IDs fence delayed/duplicate LAN grants and callbacks
- Matchmaking callbacks/authentication are fenced by WebSocket identity; explicit connect resets the retry budget, and STUN/probes use network generations and serialized probe ownership
- Proxy setup now throws on failed binds; unexpected worker termination closes the remaining resources and notifies the session owner
- Added platform-only device instrumentation for real TCP cancel/resume, stale WebSocket callbacks, and proxy failure/retry; added a LAN rebind fault-injection runner and expanded the orphan runner for ordinary Play

Validation checkpoint:

- Android debug APK and platform-only instrumentation APK built successfully, including native D1 and D2; logs: `android/temp/self-healing-build.txt`
- 10 focused JVM tests passed (6 lobby protocol, 4 mission transfer resume)
- Device instrumentation passed real TCP cancellation/resume, obsolete WebSocket callbacks, and proxy bind/forwarding failure and retry; log: `android/temp/self-healing-device.txt`
- LAN failed-rebind recovery passed for automatic and explicit retry, including supervisor shutdown; log: `android/temp/self-healing-lan.txt`. The initial runner matched the wrong log layout; device logs showed recovery worked, and the corrected assertion passed
- Ordinary Play orphan test exposed another real issue: a cached `gameRunningFlag` could consume a retry without reaching process cleanup. Replaced that launch decision with the current returnable Activity state. The rebuilt ordinary Play and multiplayer orphan tests both passed; logs: `android/temp/self-healing-orphan-game.txt` and `android/temp/self-healing-orphan-multiplayer.txt`
- Host start now stages START until successful `startActivity`, and a common launcher failure handler clears guards and rolls back owned lobby state. Device instrumentation passed failure A, retry B, and obsolete A confirmation/failure, alongside the earlier transfer/WebSocket/proxy tests; log: `android/temp/self-healing-device-launch.txt`

Full build for the newest behavior passed, including both APKs and the 10 focused unit tests; output: `android/temp/self-healing-launch-build.txt`. Scoped formatting passed after splitting an overlong log line; output: `android/temp/self-healing-format-launch.txt`. No tool sessions remain running at this checkpoint. Installed emulator APKs are the injected x86_64 outputs under `android/app/build/intermediates/apk/`, not the stale APK under `outputs/apk/`.

## Final implementation and verification

- All new engine launches use shared orphan-process readiness. Device tests cover Play and multiplayer retry, including preserving the same PID when returning to a healthy Activity. Resume, recorded demos and automation use the same checked handoff; they were reviewed at their call sites rather than separately replayed on-device
- Launcher preparation uses a 120-second deadline and an independent serialized writer. Cancellation interrupts cooperative blocking I/O, drops late results and retains file ownership until non-cooperative work actually returns. Two JVM tests cover both cooperative interruption/retry and a worker that ignores interruption
- Launch ownership is fenced across Activity instances and asynchronous preparation; online rollback is additionally fenced by network generation and proxy identity. The real launcher instrumentation deliberately starts a nonexistent Activity twice and verifies that both failures clear preparation and report a message
- Host START is committed after successful Activity launch. Controller tests cover failure A, retry B, late A completion/failure, and B confirmation; failure messages use the existing lobby system-chat broadcast path
- LAN transport failure/retry tests passed for both automatic and explicit recovery, including supervisor shutdown
- Mission download tests use real TCP sockets and app storage. They cover interrupted read/resume with verified partial bytes preserved, cancellation on entry to verification/finalization, old and duplicate grant rejection, and obsolete automatic retry rejection. The host now tracks and closes accepted transfer sockets on stop
- Matchmaking tests reject old WebSocket callbacks and old-session launch cleanup. A blocking probe test confirms the proxy cannot consume the shared socket until the reader retires
- Proxy tests cover occupied-port failure, subsequent successful binding, unexpected forwarding-socket closure, one failure notification and resource cleanup
- Focused JVM suite passed: 12 tests (6 protocol, 4 transfer resume, 2 launch preparation). Android debug and instrumentation builds passed, including D1 and D2 native libraries. Scoped mixed formatting and `git diff --check` passed; new untracked Kotlin files were also formatted explicitly because the scoped wrapper omitted them

Evidence:

- `android/temp/self-healing-final-build.txt`: build and 12 JVM tests
- `android/temp/self-healing-device-final.txt`: device instrumentation (includes all tests listed above, including grant and transfer phase cancellation assertions)
- `android/temp/self-healing-lan-final.txt`: failed LAN rebind recovery
- `android/temp/self-healing-orphan-game-final.txt`: healthy-game preservation and orphan replacement via Play
- `android/temp/self-healing-orphan-multiplayer-final.txt`: healthy-game preservation and orphan replacement via multiplayer
- `android/temp/self-healing-final-format.txt`, `self-healing-owner-format.txt`, `self-healing-handoff-format.txt`: scoped formatting

## Validation boundaries

These fixes establish retry ownership and visible failure for the six concrete findings; they do not prove recovery from every Android or native hang. In particular, a permanently stuck in-process JNI call cannot safely be force-stopped by coroutine cancellation. Its file lock remains held to avoid concurrent writers, and subsequent preparation attempts also have a deadline. Moving that work to a killable process, and bounding MainActivity playlist preparation, remain separate design work.

Physical Wi-Fi broadcast delivery and a full two-device Internet-matchmaking game were not exercised by the emulator harness. Resume/demo-specific payload behavior was not changed. The verification/finalization tests cancel at phase boundaries; they do not simulate process death halfway through archive publication. Accepted host-socket cleanup was reviewed in code rather than separately fault-injected. The audit document remains the historical pre-fix review; this plan records the resulting implementation.

Final handoff check:

- `android/temp/self-healing-handoff-build.txt`: final APK and instrumentation build passed after adding the last online-session generation check immediately before Activity launch
- `android/temp/self-healing-device-final.txt`: all device recovery instrumentation passed again against the final installed APK
- `android/temp/self-healing-save-compatibility.txt`: incompatible save preserved, host and client warned, Start fresh clears both restrictions/warnings
- `android/temp/self-healing-active-discovery.txt`: additional discovery regression did NOT pass. The host fixture received an automatic QUERY from the client, but no ANNOUNCE reached the app receiver in logcat. Repeating replies and using the observed query return endpoint did not resolve it. Experimental fixture edits were reverted; no production discovery change was inferred from this inconclusive transport result. Physical LAN discovery still needs real-device verification
- Healthy-game return initially failed only because the test expected `mResumedActivity`; this emulator exposes `topResumedActivity`/`ResumedActivity`. The corrected assertion passed for both retry kinds
- No commits were created and `android/outstanding_bugs.md` was not edited
