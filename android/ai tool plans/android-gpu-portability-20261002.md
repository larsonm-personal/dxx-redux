# Android GPU portability

Status: implementation and emulator verification complete; physical GPU qualification remains

## Scope

Guard optional anisotropy calls, select MSAA samples from the intersection of actual color/depth support, reject unknown window formats, and report capabilities to the launcher and live overlay. Preserve the five-second confirmation and complete accepted-settings rollback. Never silently persist reduced settings or treat an unobserved color mode as unsupported

The launcher shows explanations directly below MSAA/AF, including unknown support before a game has initialized that requested color mode. Native capability reports are atomic, per requested color mode, versioned and tied to the Android build fingerprint. A fresh current-context query is authoritative; cached reports only guide launcher controls

## Work

- [x] Shared native capability query and exact format/sample policy
- [x] Extension guards and allocation/dimension validation
- [x] Launcher support details and unavailable overlay controls
- [x] Host/JVM and both-engine integration tests, including forced unsupported capabilities
- [x] Android/Windows builds and scoped mixed-language quality checks
- [x] Run existing MSAA, resolution recovery and context-loss checks; document physical coverage limits

## Further device coverage

Run the same tests on physical Mali, Adreno and Shield devices when available, preserving personal app data. Capture renderer/driver, requested and actual color/sample formats and pixel probes. Add representative transparent walls, distant geometry, HUD, rear-view and missile-camera checks to hardware release validation. Sustained performance and thermal tests remain device qualification work, distinct from the five-second settings confirmation

## Implementation and evidence

- Shared `android_gpu_capabilities.cpp` checks exact tokens in the GLES extension string and queries actual window component sizes/type/encoding, per-format samples and renderbuffer dimensions. The sample policy chooses the smallest common count at least as large as requested; actual allocation remains checked. Unknown formats and incompatible attachments reject trials through accepted-settings recovery
- Anisotropy is queried/applied only when supported. Launcher reports are advisory, atomic, per color mode and Android build fingerprint; missing/stale/malformed reports remain unknown. The live overlay labels unsupported controls unavailable and omits them from navigation and touch handling
- Normal EGL replacement now forgets resource names using the same path as lost-context recovery, before allocating new shim resources. Inspection found timing-query names previously survived ordinary resolution replacement. The mode-restore fixture now checks retained scene GL errors
- Both Windows engines build; the new native format/sample tests pass in both builds. Full Android build and JVM suite passed (`android/temp/gpu-portability-build.txt`), followed by focused JVM checks and the Android instrumentation build. Scoped mixed-language quality passes after shortening the unknown-support label
- Initial negative integration verified full MSAA rollback; its first fixture failure compared a floating JSON value as an integer string. Subsequent runs verified MSAA/AF rejection and unavailable overlay navigation but separate confirmation key commands exceeded five seconds on the slow emulator. Fixtures now dispatch select/accept together through the real controller route; the product deadline is unchanged
- An independent temporary EGL probe confirms emulator-5554 advertises no anisotropic extension via either indexed or legacy extension enumeration. Positive AF scenarios require a device advertising AF; unsupported AF is a real-device capability condition on this emulator as well as an injected negative case
- Forced unsupported-capability integration passed in both engines (D1: 56 steps; D2: 58). It verifies complete accepted-settings rollback for MSAA and AF, unavailable live-overlay controls through controller navigation, and successful MSAA rendering after restoring real capabilities
- Resolution rollback exposed a separate recovery limit: the old three-second watchdog killed an EGL/shader rebuild that was making progress. Mode changes now have ten seconds to restore; live-setting rollback keeps three seconds. The user's five-second confirmation deadline is unchanged. Focused JVM tests cover resolution, aspect, color-depth and live-setting timeout selection
- Capability queries avoid repeated indexed extension calls, and cold contexts with MSAA off no longer enable per-frame MSAA tracing. Final Android builds include both engines and all configured ABIs; the four capability/recovery JVM tests and existing video-overlay layout tests pass
- Focused launcher instrumentation passes with actual production controls and Android accessibility semantics: unsupported support leaves only Off enabled, partial support disables only unavailable levels, and unknown support leaves all choices available with explanatory text
- D2 mode-restore testing exposed the existing one-second overlay preparation deadline expiring before Android's first popup draw (`deadline_ms=0`, automatic rollback). Preparation now allows a bounded five seconds; the separate five-second user countdown still begins only after the first Android UI draw. The next run confirmed normal five-second timeout recovery, but the native fixture missed the active challenge while the first scene blocked its thread. The fixture now explicitly tests launcher Cancel in D1 and timeout in D2, retaining UI evidence that `Cancel (5)` was drawn and checking rendered pixels after recovery
- Resolution/color-mode recovery passes in D1 (70 steps, `android/temp/gpu-portability-verified-test_graphics_mode_restore.txt`) and D2 (68 steps, `android/temp/gpu-portability-mode-d2-timeout.txt`). Coverage includes launcher recovery, nested-menu changes, edit-back suppression, zero scene GL errors, and owned known-color probes after context reconstruction
- Verbose per-draw MSAA state tracing pushed an emulator frame past the real five-second confirmation deadline during context testing, despite successful framebuffer creation and resolve with zero GL errors. Context/MSAA functional fixtures now leave that optional tracing off; explicit pixel probes, framebuffer checks and GL-error assertions remain enabled. The product confirmation duration is unchanged
- Context testing passed the first D1 forced-loss recovery and pixel probe, then exposed host timing assumptions: a launcher round trip exceeded the fixture's 30-second wait, and a marker emitted after the candidate frame arrived too late to press Home during the challenge. Lifecycle waits now allow 90 seconds; background markers are emitted before the candidate frame and the host waits for the durable live-challenge phase without an unconditional one-second delay. These are harness changes, not product deadline changes
- MSAA render/menu tests passed RGB565 in both games (D1 96 steps, D2 126), and RGBA8888 in D2 (126). D1 RGBA8888 passed its initial 2x/4x probes but missed a later acceptance deadline because a separate wait/input sequence rendered another slow frame. Controller automation now supports native `expect` preconditions and dispatches input in the same frame they succeed. The MSAA and context fixtures also require the introspected `candidate_ready` flag before accepting. D1 RGBA8888 then passed all 92 steps, completing both games and both color modes (`android/temp/gpu-portability-msaa-matrix.txt`, `android/temp/gpu-portability-msaa-d1-rgba-final.txt`)
- Final context-loss integration passed all 54 steps in each game (`android/temp/gpu-portability-context-handoff.txt`). Each run rejects live trials during two Home/resume cycles, retires and recreates actual EGL contexts, retains accepted MSAA settings, and verifies zero scene GL errors plus successful state-preserving pixel probes
- Final Android build passed (`android/temp/gpu-portability-input-precondition-build-retry.txt`); scoped code quality and `git diff --check` pass. The test emulator needed one harness-managed restart after launcher responsiveness degraded; the successful reruns use the same product deadlines and assertions. No physical-device qualification was performed or personal device data changed
