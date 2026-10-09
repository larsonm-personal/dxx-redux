# D1 DOS 1.4 shareware in the D2 engine

Status: implementation in progress; shared definitions and D2 adapter implemented, Android gameplay and persistence qualification underway

## Outcome and scope

Support structurally compatible PC shareware sources in D1-in-D2, using only their own installed assets. DOS 1.4 is the first acceptance target, not an edition whitelist. Qualify both the uncompressed installer PIG (5092871 bytes) and the compressed DOS-installed PIG (2509799 bytes) when authentic fixtures are available. A successful import, a launcher readiness result and a successful game session are separate assertions.

Deliver through six sequential parts below. Parts 2 and 3 are the largest and most coupled. Finish the fixture/dependency inventory before committing to their final interface or an elapsed-time estimate.

Include available earlier PC shareware and Test Flight fixtures in qualification. Accept compatible structures and complete dependencies; retain exclusions only for demonstrated missing capabilities. Original Mac and registered PC 1.0 formats require separate investigation if they differ from the implemented format. Multiplayer, arbitrary third-party shareware modifications and full D1/D2 engine consolidation are outside this first milestone. Existing registered D1, native D1 and ordinary D2 behavior must retain their current support.

## Evidence and architectural decision

- `d2/main/d1_in_d2/d1_in_d2_assets.c`: `d1_in_d2_source_edition_error` rejects all known PC shareware PIG sizes; `d1_in_d2_read_assets` expects embedded registered-PIG definitions
- `d1/main/piggy.c`: shareware starts the bitmap/sound directory at offset zero and selects `PIGGY_PC_SHAREWARE`
- `d1/main/bmread.c`: `gamedata_read_tbl` reads BITMAPS.TBL or decodes BITMAPS.BIN, handles shareware exclusion markers, loads referenced polygon models, and modifies engine globals
- `d2/main/d1_in_d2/d1_in_d2_assets.h`: D2 already has an owned, unpublished asset generation followed by validation and publication
- `android/app/src/main/java/com/dxxredux/app/SetupGameFiles.kt`: launcher admission delegates to the native edition query
- `android/app/src/androidTest/java/com/dxxredux/app/DemoImportChecks.kt`: the original-package corpus verifies installed bytes and readiness for the package's native game, not D1-in-D2 gameplay

Use a shared native reader for shareware source semantics, with explicit inputs and owned output. Adapt that output separately to native D1 and to the existing D2 asset generation. Compile the same reader into host checks and Android. Keep game-format decisions out of Kotlin. Place new cross-platform shared code under the repository's shared-code convention in `android/`, with no JNI or Android runtime dependency in the reader.

Do not call the existing global-mutating D1 loader inside D2 and then copy its globals. Do not introduce a second independently maintained D2 implementation of the table grammar. Preserve the registered-PIG path and existing D2 publication lifecycle. Refactor only the source-reading pieces needed for this feature; broad engine deduplication is not a prerequisite.

## Part 1: Establish the exact source contract and fixtures

Size: small to medium. Dependency: none

- Inventory the authentic DOS 1.4 package and installed HOG/PIG: table, palette, POF models, mission levels, briefing/endlevel resources and any external dependencies actually consumed by native D1
- Record archive provenance, hashes and member inventories. Distinguish uncompressed and compressed representations from genuinely different editions
- Trace the full native D1 table-loading call graph, including model loading, bitmap name lookup, sound decoding, initialization defaults and parser reset behavior
- Identify shareware semantics that cannot be inferred from PIG size alone. The 5092871-byte size is shared with other early demos; establish structural validation and dependency checks before relaxing admission
- Capture a native D1 reference for definition counts, important references, decoded model/bitmap/sound hashes and the seven-level campaign inventory
- Document which inputs are mandatory and how reads are bound to the selected asset set, avoiding accidental resolution from another mounted installation

Acceptance: an authentic, reproducible fixture manifest and a dependency map explain every input required for DOS 1.4 startup. Missing fixtures are reported as unqualified cases, not passing tests. No downloaded package or engine source is rewritten to make the fixture pass

## Part 2: Isolate and share the definition reader

Size: large. Dependency: part 1

- Extract BIN text decoding, table tokenization, shareware filtering and definition construction into a context-owned reader
- Inventory and represent textures, clips, effects, walls, sound mappings, robots/AI, weapons, powerups, player ship, reactor/object definitions, gauges and model references actually needed by the demo
- Give the reader explicit resource lookup and bitmap/sound name resolution. Model reads must produce owned data without publishing engine globals
- Preserve native D1 semantics for defaults, index allocation, skipped entries, fixed-point conversions and model references. Keep D2-only field defaults in the D2 adapter
- Add bounded counts, spans and references; return useful source/member/line errors for malformed input. Ensure a failed or repeated load releases buffers and resets parser state
- Switch the native D1 shareware path to the shared reader through a thin publication adapter, retaining its observable behavior

Acceptance: the shared reader consumes authentic fixtures on the host; normalized output agrees with the pre-refactor native D1 reference. Native D1 still launches the demo. Focused malformed-input and repeated-load cases fail cleanly. Host and Android builds compile the same implementation

## Part 3: Build a complete D2 asset generation from shareware

Size: large. Dependency: part 2

- Add a shareware preparation route to `d1_in_d2_read_assets`, retaining registered-PIG preparation
- Convert shared definitions and owned models into `d1_asset_generation`; reuse existing reference validation, publication and retirement
- Audit existing bitmap and sound readers for shareware behavior. Verify uncompressed/RLE bitmap handling, sound compression, logical sound mappings and sample conversion against native D1; reuse format decoders where compatible
- Include table and model dependencies in source identity, along with the existing relevant assets, so save/restore and caches cannot silently reuse definitions from a different source
- Resolve resources from the selected source with defined precedence. Loading must work without retail D1 or D2 data present
- Verify failure does not replace the active generation, leak allocations, or leave partial references

Acceptance: D2 prepares and validates the real demo using its assets alone. Model, texture and sound references resolve; source replacement and malformed/missing dependencies preserve the prior active state. Both PIG representations produce equivalent logical assets where fixtures permit that claim

## Part 4: Connect capability checks, mission selection and launch

Size: medium. Dependency: parts 1 and 3

- Extend the native capability query beyond the current size-only rejection if needed to distinguish DOS 1.4 from other same-size editions and to inspect required dependencies
- Have launcher readiness, mission visibility and native startup consume the same native classification rules; startup still performs full validation
- Admit shareware sources whose format and dependencies validate. Report concrete unsupported-format or missing-resource reasons without a DOS 1.4-only whitelist
- Audit D1 shareware HOG recognition, `.sdl` level selection, campaign length, briefings, endlevel flow and final-level completion in D2
- Check launcher fallback and D1-only/both-games-installed selection, including supported storage/staging paths

Acceptance: an authentic imported DOS 1.4 set is selectable and launches in D2; incomplete or unsupported sets are explained consistently. Test Flight readiness follows validated capabilities and actual campaign support, rather than either its size or a historical blanket exclusion

## Part 5: Qualify runtime behavior and persistence

Size: medium to large. Dependency: part 4

- Extend `android/tests/test_d1_in_d2_standalone.ps1` or its reusable helpers for a demo-only installation. Use introspection to assert source identity, mission, level and asset readiness
- Load every demo level and exercise actual combat, damage, pickups, doors, animation, reactor destruction and level transition in representative scenarios
- Check textures, model appearance and representative sounds against native D1 with targeted captures and decoded-data checks
- Exercise file save/load and existing in-memory restore/rewind paths on this source, then switch between shareware D1, registered D1 and ordinary D2. Include missing/changed-source rejection
- Test on an Android emulator and the attached physical device when available, using an isolated test asset set and preserving user content. Run a host D2 launch using the same original assets

Acceptance: a repeatable runtime suite reaches gameplay, traverses the demo campaign, covers representative interactions and persistence, and reports actual evidence. A successful process start or main menu is insufficient. Compare source semantics and targeted behavior without requiring unrelated whole-engine replay determinism

## Part 6: Close the coverage gap and publish support evidence

Size: medium. Dependency: parts 4 and 5; test schema design can start after part 1

- Add explicit per-source/per-engine expectations to the demo corpus: import result, native-game readiness, D1-in-D2 admission and runtime qualification
- Keep `DemoImportChecks` as the original-package import check and compose it with a real engine-launch runner using those imported outputs. The runtime suite must not substitute a pre-extracted retail fixture
- Keep exact imported-byte hashes distinct from normalized decoder-output comparisons. Use authentic source fixtures and native D1 reference evidence so production code does not generate its own sole oracle
- Require all existing demo catalog entries to declare expected engine support; test unsupported cases as expected rejections and mark unqualified runtime combinations explicitly
- Run relevant host/native checks, Android builds and the registered-D1/ordinary-D2 regression controls. Register new runners and timeouts in the automation catalog and run both catalog validators
- Update `d1-in-d2-support-matrix.md` and implementation evidence with exact tested editions, platforms, storage paths and remaining limits

Acceptance: regressions in extraction, launch admission and gameplay fail different named checks. The DOS 1.4 support claim links to a completed original-package-to-gameplay run. Other demo import successes cannot be mistaken for D2 engine compatibility

## Delivery order and completion boundary

1. Fixture/dependency inventory with native D1 baseline
2. Shared reader and native D1 adapter, independently verified
3. D2 generation preparation and failure handling
4. Source-aware admission and campaign startup
5. Runtime/persistence qualification
6. Corpus integration and support documentation

Keep the launcher exclusion until preparation and native validation work; enable it with the corresponding runtime regression coverage. Each part is a reviewable change or small series, rather than one combined parser/UI/test rewrite.

The milestone is complete when the authentic DOS 1.4 download installs through production code and runs the seven-level demo in D2 with its own assets, representative gameplay and persistence checks pass, and native D1 plus existing registered-D1-in-D2 and ordinary-D2 controls remain green. Any unavailable compressed fixture or untested platform remains explicitly unqualified.

## Implementation evidence, October 8

The authentic outputs already available under `temp/import-parity-audit/native-demo` and `native-flight` establish:

| Property                              | DOS 1.4         | Test Flight     |
| ------------------------------------- | --------------- | --------------- |
| HOG bytes                             | 2339773         | 1626232         |
| PIG bytes                             | 5092871         | 5092871         |
| PIG bitmap/sound counts               | 1152 / 70       | 1152 / 70       |
| Table bytes / physical lines          | 41634 / 806     | 41634 / 806     |
| Referenced non-excluded POF resources | 45, all present | 45, all present |
| Campaign levels                       | 7 SDL           | 3 SDL           |

Both tables have SHA-256 `b6db41b2405d3356229c4d34e36e710c35ee07f02f4cc2ba0781e9145de2512c`; both palettes have SHA-256 `10a357857f8df8a0c70feda07cc2b231fcbb0c66779bb5164ce1fcd2aa7d632b`. This is positive evidence for a shared decoder, not yet proof of gameplay compatibility. Some older `game_data/..._extracted` directories still contain historical truncated extraction outputs, so they are not valid PIG fixtures.

First implementation slice: `d1_shareware_table.h` supplies bounded TBL/BIN physical/logical line reading, decoding, comments and continuations without engine dependencies. Native D1 now calls it. The remaining semantic parser still writes D1 globals and must be separated before reuse in D2. Native input tests cover line endings, final unterminated lines, continuations, truncation and a fixed encoded vector; actual table comparison uses independently decoded fixture output.

Further source comparison: DOS 1.4 and Test Flight PIGs differ in exactly one byte (offset 4805402). All common model and level members match; differing common HOG members are descent.txb, credits.txb, ending.txb and order01.pcx. Referenced POF models use versions 6 and 7.

Test Flight initially failed native D1 headless loading because its HOG size was unrecognized and the mission loader selected retail `.rdl` names. Added its builtin identifier to both mission loaders and selected the existing three-level SDL campaign path. With this change native D1 loads all three Test Flight levels. Launcher exclusion remains pending full runtime qualification and D2 preparation.

Verified this slice:

- Windows D1 build passed twice, including after the campaign change (`temp/d1-shareware-reader/windows-build-flight.log`)
- `ctest --test-dir buildd1 -R '^test_d1_shareware_table$' --output-on-failure` passed
- Built native reader test consumed both authentic tables: 806 lines, normalized FNV-1a `346bcd94`, matching an independent Python decode
- Native D1 headless coop-start dump loaded all seven DOS 1.4 levels and all three Test Flight levels (`demo-levels.json`, `flight-levels.json` in the evidence directory). This validates native asset/level loading, not rendering or gameplay
- Scoped formatting and both automation catalog checks passed

Next implementation work: context-owned semantic definitions and POF loading, D1 adapter, D2 generation adapter and shareware sound decode, source-aware native admission, then original-package Android/runtime qualification. No D2 shareware support claim is made yet.

Second implementation slice: `d1_shareware_model.h` decodes POF versions 6-8 into an engine-independent source structure. It validates chunk spans, duplicate/missing submodels, counts, instruction offsets, gun indices and parent trees without touching live state. Native D1's model and reactor-gun readers use this decoder, then copy into engine structures; obsolete global-cursor POF decoding was removed. The bytecode span is borrowed until the adapter copies it into owned storage.

Evidence:

- Both authentic HOGs pass `test_d1_shareware_model`: 45 models, 96 submodels, 52 guns, 16 animation blocks; truncating each actual POF is rejected
- Malformed source count/span/offset tests and both shared-reader CTests pass
- Native D1 loads every DOS 1.4 and Test Flight level with the shared POF decoder. Complete coop-start metadata equals the earlier output for each edition (`demo-model-levels.json`, `flight-model-levels.json`)
- Final Windows D1 model build passed (`windows-model-final-build.log`); prior Windows D2 campaign build also completed successfully (`windows-d2-build.log`)
- Scoped formatting passed (`model-quality.log`)

Next: move the existing table semantics into a context-owned shared implementation rather than rewriting the grammar. Keep resource operations behind engine adapters, and bind D2 to unpublished generation storage. The table parser must retain shareware placeholder indices, defaults, model variants and AI conversions. The D2 adapter must supply shared POF decoding and context-local robot-joint construction; existing global `robot_set_angles` cannot be used during D2 preparation. After that, integrate compressed sounds and validate source dependencies/identity before relaxing readiness.

Third implementation slice: moved the existing table grammar from `d1/main/bmread.c` into `d1_shareware_definitions.hpp`. `D1TableParser` has per-load tokenizer/parser state, explicit destination views, bounded array access, and virtual resource operations. `d1_shareware_native.cpp` binds native D1's original tables and resource operations; D2 can bind an unpublished `d1_asset_generation` instead. The grammar is shared source, not a second independently maintained implementation. Native bitmap/editor resource loading and serialization remain in `bmread.c`.

Verified:

- Final Windows D1 build passes (`windows-definitions-final-build.log`)
- Shared parser compiles against D2 headers (`d2-definitions-compile.cpp` / `.obj` in the evidence directory)
- DOS 1.4's seven levels and Test Flight's three levels load through the shared definitions parser; full coop-start output equals pre-refactor JSON for each (`*-bounded-levels.json`)
- A real native load with an out-of-range `$ROBOT_AI 100000` override fails with a bounded definition error at line 1 (`malformed-native.log`)
- Scoped mixed-language formatting passes (`definitions-quality.log`)

Details to preserve in D2 integration:

- D1 legacy table loading leaves `Num_vclips` at zero despite populating its fixed 70-slot array. D2 should carry the correct extent, as its registered reader already does for zero-count files
- Shareware exclusion markers preserve indices, including robot slots with `model_num == -1` and dummy clip/bitmap entries. D2 validation and level consumers must distinguish unavailable definitions from malformed references; do not replace missing robots with unrelated models or broadly disable reference validation
- The authentic PC demo's 70 compressed sounds include 35 whose decoded length is `2 * encoded_bytes - 1`, and 35 with exact double length. A bounded decoder must honor the declared decoded length rather than write a padding sample past the allocation
- D2 asset preparation already mounts descent.hog, but may also mount D2 content. Bind shareware table/model reads to the selected D1 source and include these dependencies in source identity

Next concrete step is the D2 resource adapter: bind prepared bitmap/model/definition storage, construct robot joints without live globals, decode sounds through shared native code, then validate and publish the combined generation. Launcher admission stays blocked until that route is functional and tested.

## D2 integration and initial Android runtime evidence

The D2 adapter now reads table and POF resources directly from the selected HOG into an unpublished asset generation. Shared bounded sound decoding honors odd decoded lengths; shared robot-joint construction avoids modifying live engine arrays during preparation. Excluded shareware definitions retain their original indices. Source identity includes the shareware HOG, and PC shareware admission is enabled. Native D1 and D2 recognize Test Flight's three-level campaign; the obsolete launcher Test Flight rejection was removed.

The headless D2 entry point now uses production D1-only startup resources. DOS 1.4 uncompressed and compressed fixtures load all seven levels; Test Flight loads three. Their level metadata matches native D1. The final sound-map validation build was rerun against DOS 1.4 and still matches all seven native levels (`temp/d1-shareware-reader/demo-final-validation-levels.json`).

Android emulator `emulator-5582` runtime evidence on 2026-10-08:

- Uncompressed DOS 1.4, Test Flight and compressed DOS 1.4 each pass launcher admission, new game, movement, laser firing and sound playback in D2 with no retail assets installed
- Durable smoke results and asset introspection were copied to `temp/d1-shareware-reader/{demo,flight,compressed}-smoke-{result,state}.json`; each source publishes 70 sounds and 56 models
- All three sources additionally pass real quick-save/quick-load and natural ten-second rewind, restoring the changed ship position and retaining active model/sound assets (36 automation steps each). Durable results, native logs, introspection and screenshots are copied to `temp/d1-shareware-reader/{demo,flight,compressed}-restore-*`. Flight and compressed runs use the final rebuilt APK
- `test_d1_in_d2_standalone.ps1 -SharewareSmoke -D1DataDirectory ...` now exposes the reusable isolated scenario; the helper preserves and restores app data
- Windows D2 metadata build passes; Android debug build for all configured ABIs passes after the final sound-map checks (`d2-runtime-validation-build.log`, `android-runtime-validation-build.log`)
- The rebuilt shared sound test passes all three PIG fixtures: 70 sounds, 35 odd lengths, 827545 decoded samples and FNV1a `770dec7e`, matching the legacy decoder reference
- Scoped formatting and both automation catalog checks pass, including registration of the restore support script (`runtime-automation-catalog.log`, `runtime-runner-catalog.log`)

These are extracted-fixture runtime checks, not yet original-installer-to-gameplay qualification. Remaining work includes selected-source palette binding, endian-safe model bounds, full definition validation and unavailable-robot consumers, campaign interaction/transition/completion, both-games-installed/source switching controls, physical-device validation, original-package import composition and the final support matrix. Earlier shareware editions without authentic fixtures remain unqualified. Do not infer completion from these smoke results.

## Shared validation, portable bounds and campaign progression

- Both adapters now calculate model bounds with the shared bounded little-endian POF reader, avoiding native-endian and unaligned vector casts. It preserves legacy first-vertex/translation behavior and rejects truncated vertex records and translated-coordinate overflow. Synthetic unaligned/negative-coordinate tests and all 45 actual models in both HOGs pass
- Native D1 and D2 each load all seven DOS and three Test Flight levels after this change; complete level metadata matches the pre-refactor reference (`*-bounds-levels.json`)
- Ship, weapon and powerup references moved from the registered-only reader into common generation validation, alongside count limits, clip frame limits and object-bitmap pointer checks. DOS uncompressed/compressed, Test Flight, registered D1 and ordinary D2 host loads pass (`*-shared-validation-levels.json`). A same-length BIN mutation from ship explosion clip 58 to 99 is rejected during preparation (`invalid-ship.log`)
- Android debug and isolated diagnostic builds pass (`android-bounds-validation-build.log`, `android-isolated-build.log`)
- `-SharewareCampaign` extends the existing first-level interaction route with the authentic demo's reactor model indices. DOS 1.4 passes doors, ceiling-light behavior, monitor destruction, lava metadata, reactor destruction/wreck rendering and a normal rendered exit to level 2 on the emulator (`demo-campaign-*` in the evidence directory)
- Initial physical-device gameplay reaches level 1 but pauses for first-run graphics calibration. This is recorded as a failed test, not a supported-device result. The gameplay runner now explicitly marks that separately tested setup step as offered and preserves/restores `no_backup` as well as files/preferences

Remaining qualification still includes the complete campaign/final-level ending, unavailable-robot consumers, selected-source palette behavior with both games, source switching, a passing physical run, original-package-to-gameplay composition and the support matrix. Portable bounds and common ship/weapon/powerup validation are implemented and verified; do not leave them listed as pending implementation.

Physical and mixed-installation qualification completed later in the same work session:

- Attached phone `JYPR42510121028` (Android API 33, Mali-G77 MC9), isolated package `com.dxxredux.app.nsdtest`: DOS demo movement, firing, sound, quick-save/load and natural rewind pass (`physical-demo-*`), followed by the full door/effect/reactor/exit route to level 2 (`physical-campaign-*`, 68 steps)
- Emulator with retail D2 and the DOS demo installed together: verifies initial D2 asset mode, selects `D1: Descent Demo`, verifies imported demo mode with 60 models (56 source plus four optional companion models), then passes gameplay, save/load and rewind (`both-installed-*`, 40 steps). The same host validation also accepts ordinary D2 and registered D1
- Runner fixes discovered during qualification: graphics setup-command values must be strings; insert setup steps after indexed scenario composition; use the demo mission name in a real mission picker; restore `no_backup` during cleanup. The intermediate failed runs are not counted as passing evidence. The diagnostic app's backup directories were restored and removed before the clean passing runs

Next priorities: compose original downloaded package import with these engine checks; test complete campaign/final ending and remaining source-switching cases; audit unavailable robot creation paths; publish the exact qualified support matrix. The phone and ordinary both-games-installed smoke checks are now verified rather than pending.

## Original-package import and runtime composition

The registered `test_demo_import.ps1` now runs the entire catalog through `installDemoArchive`, checks explicit per-engine readiness from `demo_import_oracles.json`, and uses the exact installed on-device outputs for the two PC shareware runtime cases. Instrumentation retains those sets in a test-owned directory; the isolated runtime helper copies them with their manifest into the selected installation while preserving the original app state. No host extraction or replacement game data is involved. Cleanup removes the retained test outputs after the suite.

Verified on emulator-5582, API 34 x86_64:

- All 11 original archives pass import, installed-byte hashes, manifest checks, per-engine readiness and failed-import preservation
- `desc14sw.exe` and `descent 1 demo 1-4.zip` each pass 37-step gameplay/save/load/rewind and 71-step door/effect/reactor/normal-exit-to-level-2 runs
- The first composed campaign attempt exposed a test-route problem: the ship died while waiting beside the destroyed reactor. The route now relocates out of the blast area immediately after destruction, asserts survival, then checks the wreck and flies through the ordinary exit trigger. It does not alter shields, grant invulnerability or replace the exit transition
- `original-package-runtime-final.log` exits successfully; `original-package-qualification.json` records APK/oracle hashes and all four run IDs. Durable results, introspection, native logs and screenshots use `package-{exe,zip}-{smoke,campaign}-*` in `temp/d1-shareware-reader`
- App/instrumentation build, scoped mixed-language quality checks and both automation catalogs pass. The suite master timeout is 1800 seconds to include the runtime phases. Final device checks confirm test fixture/export and backup directories were removed
- The support matrix now separates qualified PC shareware/Test Flight, unqualified earlier shareware, and still-unsupported registered PC 1.0/Mac formats. It records actual physical arm64 and original-package emulator evidence

Remaining: complete campaign/final-ending qualification, remaining source-switching and malformed-source retirement cases, unavailable-robot creation audit, and final source/ABI/regression review. The original-package-to-gameplay coverage gap is closed for both PC download catalog entries; this does not complete the entire implementation plan.

## Campaign completion and robot consumers

The reusable Android runner now accepts `-SharewareLastLevel 3` or `7`. After
the existing level-one combat, door, reactor and normal-exit route, it loads each
remaining level, checks the live player, invokes that level's authored exit
tunnel, and verifies advancement. The last level must show its ending briefing,
order presentation and score flow before returning out of gameplay. This is
campaign-transition coverage, not a claim of full combat coverage on every level.

Both extracted-fixture runs passed on API 34 x86_64: Test Flight completed 91/91
steps (`a9dc0b9d905a4ef9adb10018118ef339`), and DOS 1.4 completed 123/123
steps (`db91895328a5498ca743acf8af3f477b`). Durable results, scripts, native logs
and introspection are under `temp/d1-shareware-reader/{flight,dos}-complete-*`.
The original-package suite now requests the seven-level campaign for both PC
downloads; its expanded run passes for both downloads (123/123 campaign steps each). Evidence is in `original-package-full-campaign.log` and `full-package-{exe,zip}-{smoke,campaign}-*`; the installed APK hash is in `full-campaign-installed-apk-hash.txt`.

Robot consumers now reject unavailable definitions before factory creation,
boss gating, dropped-robot creation and paging. Level preparation checks factory
flags and object-contained robots; generation validation checks robot-contained
robots and the default robot-drop reference. The five authentic host controls
(DOS uncompressed, DOS compressed, Test Flight, registered D1 and ordinary D2)
passed after the initial consumer guards. Final native D1 and D2 host integration suites pass, including default/drop-reference rejection checks (`robot-final-{native-d1-tests,host-tests}.log`). All five authentic host controls pass again with the final validation. The Android three-ABI rebuild passes; Test Flight passes the full 91-step campaign on the final APK. `temp/d1-shareware-reader/campaign-qualification.json` records exact APK hashes and run results. Remaining work is source switching, source-failure preservation and the final requirement audit; campaign endings and unavailable-robot checks are now verified.
