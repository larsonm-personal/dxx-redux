# D1-in-D2 primary weapon switching

1. [x] Capture the connected phone's game, input, overlay, and audio state
2. [x] Reproduce missing selection audio, blocked primary fire, and stale touch labels with targeted diagnostics
3. [x] Fix confirmed causes while checking the corresponding D1 paths
4. [x] Run scoped formatting, relevant builds, and regression checks

Initial state: physical device JYPR42510121028 runs com.dxxredux.app.nsdtest, D1 level 1 in D2, with lasers and Vulcan owned

Do not edit android/outstanding_bugs.md

## Findings and changes

- D1 laser availability aliases 5-12 overlapped D2 upgraded weapon slots. Re-selecting lasers through the wheel selected index 5; a held primary-fire probe consumed no energy, while index 0 fired before and after it
- D1 direct selection now bypasses D2's normal/super toggle, and direct laser aliases normalize to the actual laser slot
- PC shareware defines selection audio at logical sound 155 and cheater audio at 156. Normalize these at import to the engine's selection/cheater IDs, preserving both full and low-memory maps
- The overlay only fetched weapon state when drawing. Its touch-release redraw could precede the native selection and leave the old label until another UI event. Polling now invalidates when the native weapon snapshot changes
- Added native sound mapping and displayed weapon labels to introspection

## Validation

- Scoped mixed-language formatting/lint passed
- Windows D2 CMake build passed; Android debug APK built for all configured ABIs and both engines
- Extended host weapon regression passed with authentic phone shareware data, registered D1 source switching, and ordinary D2 asset controls
- Physical phone: 10 actual touch-wheel gestures, including repeated Laser and Vulcan selections; every engine selection and displayed label matched, and every subsequent held-fire probe consumed the correct resource
- Isolated emulator shareware smoke: 44 steps passed, including repeated touch-wheel laser selection, label assertion, firing, audio, save/load and memory rewind
- Both automation catalog validators passed
- Evidence: temp/weapon-switch, temp/host-test.log, temp/profile-test.log and temp/d1-launch-runtime-emulator-5582-20261008-135854
- Updated APK installed on the physical phone; the saved mid-level checkpoint from before the fixed-build firing tests was restored afterward

## Follow-up: complete sound list and ordinary D2 lasers

1. [x] Audit every shareware sound declaration and low-memory target against the imported bank and engine IDs
2. [x] Add coverage for the complete source sound list and ordinary D2 laser/Vulcan reselects without super lasers
3. [x] Run scoped formatting, host build and relevant integration tests; record any remaining intentional silence

- The phone's PC shareware table declares 82 active logical sounds referencing all 70 packaged samples; every explicit sound reference in an active asset definition has a declaration
- The three existing engine aliases cover primary/secondary selection (153/154 -> shareware 155) and cheating (200 -> shareware 156). Other shared D1/D2 sound IDs agree
- Seven declarations are explicitly excluded from shareware: fusion/plasma (24/25), registered missiles (131/132), registered boss sounds (186-188). They are not missing imported samples
- Fusion warmup (34) and invulnerability expiry (163) have no declaration or sample in this shareware release, matching native D1's silence. D2-only sound IDs likewise have no authored shareware counterpart
- Added a source-driven host check for every active declaration's exact sample, nonempty audio and normal/low-memory resolution; verifies all packaged samples are reachable and all three aliases match. Repeats after registered/shareware source switches
- Ordinary D2 never owns super-laser inventory slot 5, including after picking up level 5/6 upgrades; super lasers raise laser_level while using slot 0. Added firing coverage for levels 1-6, quad/non-quad, repeated Laser/Vulcan selections and stale remembered super selections
- Scoped formatting passed; final incremental Windows D2 build passed without warnings. Authentic shareware sound/source-switch tests and the complete D1/D2 asset compatibility suite passed, including 60 D2 selection-and-fire checks per profile load
- Follow-up evidence: temp/weapon-switch/followup-format.log, followup-build-final.log, followup-sounds-test.log and followup-profile-test.log. No further runtime changes or device installation were needed
