# D1/D2 high-coupling cleanup campaign

## Goal

Process the remaining high-value D1/D2 diff-minimization candidates with the fixture and integration coverage required for their higher coupling. Defer the input-demo direct-command policy until every non-demo-breaking tranche is complete and validated so demos are re-recorded only once from a clean final state.

## Existing work to preserve

- Preserve the completed low-risk diff-minimization campaign and its shared helpers
- Preserve unrelated route-planner, guidebot, mission metadata, and outstanding-bug work
- Keep desktop D1/D2 and Android D1/D2 behavior supported throughout
- Keep D1/D2 file formats and game-specific policy in engine-owned C code

## Ordered tranches

### H01. Playsave transactional repair and format fixtures

- [x] Refresh the live D1/D2 launcher-bridge layout audit against current code
- [x] Add byte-accurate D1/D2 PLR and PLX fixtures covering supported versions and layout variants
- [x] Add short-file, malformed-section, oversized-PLX, and injected-write-failure coverage
- [x] Introduce atomic shared I/O and bounded PLX rewrite primitives
- [x] Correct layout/version handling through compact per-game descriptors kept next to canonical engine format knowledge
- [x] Extract common launcher bridge mechanisms and minimize inherited `playsave.c`/header churn
- [x] Build and run host and all-ABI Android coverage; JNI callers compile against the repaired bridge

H01 evidence:

- D1 fixtures cover versions 4-8, shareware and registered v7/v8 layouts, and both 8-byte D1X variants
- D2 fixtures cover versions 17-24 structural boundaries and byte-swapped v24 headers
- Atomic failure fixtures cover write, sync, and replace failures without original-file mutation
- PLX fixtures preserve a 65,537-byte payload, unknown keys, outer `[end]` placement, and unterminated target-section repair
- Windows D1/D2 builds and all six focused executables pass
- Android native builds pass for arm64-v8a, armeabi-v7a, and x86_64

### H02. Kconfig launcher control-layout descriptors

- [x] Capture D1/D2 launcher-setting mappings in table-driven fixtures before movement
- [x] Define compact game-specific override descriptors
- [x] Move pair/default construction into shared code without changing indices or defaults
- [x] Run controller comparison, keyboard defaults, and launcher roundtrip coverage

H02 evidence:

- Shared descriptors encode the 50-slot D1 and 56-slot D2 joystick layouts, invert slots, common Android defaults, and game-specific high-slot mappings
- Descriptor fixtures distinguish D1/D2 logical joystick counts from their full persisted settings spans, including D2 keyboard slot 56
- Suffixed JNI methods load both engine libraries, emit game-sized arrays, and route D1 patch/reset calls through D1's canonical Kconfig and playsave code
- Windows D1/D2 builds and both focused descriptor executables pass
- `ControllerConfigSerializationTest` and all-ABI Android native builds pass
- The controller comparison runner now creates a real pilot, writes a distinct axis fixture, resets and patches it through the selected engine JNI, masks the live config override, and compares persisted joystick plus full keyboard arrays after relaunch
- Emulator D1 and D2 runs each pass 37 steps: D1 reports one D1 reset and patch with a 50-byte keyboard readback, while D2 reports one D2 reset and patch with a 60-byte keyboard readback

### H03. Host migration and disconnect coverage

- [x] Reconcile existing host-migration plans and current implementation
- [x] Add deterministic host-loss/disconnect coverage before extraction
- [x] Separate common election/reset/ownership mechanics from D2-only ownership transfer
- [x] Validate two-emulator host loss, reconnect, object ownership, and guidebot authority

H03 implementation evidence:

- Shared host-migration policy fixtures cover ordinary disconnect, local and remote election, lowest-slot determinism, ineligible slots, no-survivor fallback, stale host state, and object-owner reset
- Shared runtime code now owns election, rewind reset, object ownership reset, powerup recount, migration metadata output, and Kotlin notification
- D2 retains its Guide-Bot ownership handoff after the common transition; the focused D2 escort-owner policy fixture passes
- D1 and D2 `multi.c` shed 54 and 56 inherited additions respectively, reducing combined inherited additions by 110
- Windows D1/D2 builds, both host-migration policy executables, and the combined all-ABI Android build pass
- D2 two-emulator acceptance passes initial PDATA, both host elections, both port-42425 rejoins, synchronized-object parity, Guide-Bot generation/ownership parity, and post-rejoin PDATA
- D1 two-emulator acceptance passes the corresponding two elections and rejoins with fresh process/introspection gates, synchronized-object parity, owner reset, and sustained PDATA
- Migrated-master transport paths now address the elected slot rather than slot 0; D1's Android full-game-info packet carries the elected master slot like D2

### H04. Private newmenu rendering state

- [x] Re-audit newmenu/listbox residuals after the shared scaled-render callback work
- [x] Extract only callbacks that are smaller than the duplicated bodies
- [x] Keep private layout, selection, and hit-testing policy local where abstraction would increase inherited churn
- [x] Run menu scale, readability, touch, pause, and listbox coverage

H04 implementation evidence:

- The shared menu-scale owner now performs the duplicated offscreen source/direct-render transaction while each game retains only compact private-state callbacks
- D1 and D2 `newmenu.c` each shed 53 inherited additions; combined inherited churn fell by 104 changed lines
- Remaining private callbacks are 8, 39, and 31 lines, at the campaign stopping scale
- The unified runner creates a real pilot before relaunching and covers scaled pilot listboxes, main and options newmenus, and in-game pause newmenus
- Windows D1/D2 and all configured Android ABIs pass; the installed combined build completes the unified emulator run for both D1 and D2

### H05. D2 classic-demo serialization

- [x] Snapshot representative classic-demo JSON output fixtures
- [x] Separate serialization from private parser state through a compact immutable record/snapshot boundary
- [x] Preserve parsing, mount, error, and output ordering semantics
- [x] Validate exact normalized JSON output and failure cases

H05 implementation evidence:

- A pure C serializer now owns stable JSON formatting for immutable header, frame, player, object, robot-damage, and result records without access to private parser or engine globals
- D2 `newdemo.c` retains PhysFS mounting and private parser state while a D2 engine adapter constructs records, shedding 165 inherited additions
- Exact fixtures cover escaping, controls, both wiggle sources, physics, robot AI, damage, results, injected sink failure, and invalid spans
- Real v15 and v16 dumps complete with stable record counts; repeated 5,163-frame v15 output is byte-identical
- Missing and corrupt demo failures return cleanly without invoking headless menu rendering
- Windows D2, the focused CTest, and the combined all-ABI Android build pass

### H06. Input-demo direct-command policy and demo refresh

- [ ] Begin only after H01-H05 are complete and fully validated
- [ ] Define shared command iteration and explicit D1/D2 policy adapters
- [ ] Accept input-demo format/behavior compatibility breaks where they simplify the correct engine boundary
- [ ] Run deterministic record/replay and final-state coverage
- [ ] Re-record affected regression demos once from the final clean implementation
- [ ] Regenerate and verify associated state and RNG traces

## Validation gates for every tranche

- Run scoped code quality and `git diff --check`
- Build all configured Android ABIs
- Build Windows D1 and D2
- Run focused host fixtures before emulator tests
- Run emulator tests sequentially with cleared logcat and file-backed output
- Record exact before/after inherited-file metrics and update the candidate catalog

## Current status

- [x] Campaign order established with demo-breaking work last
- [x] H01 playsave transactional repair and format fixtures complete
- [x] H02 Kconfig launcher control-layout descriptors complete
- [x] H03 host-migration implementation and host fixtures complete
- [x] H03 D1/D2 two-emulator acceptance complete
- [x] H04-H05 implementation, combined Android build, and focused fixture validation complete
- [x] H04 D1/D2 emulator acceptance complete
- [x] H02 D1/D2 real-pilot launcher JNI roundtrip complete
- [x] H03 two-emulator acceptance complete
