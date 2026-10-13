# S21 mod campaign findings, build 24970

Status: testing and fix validation complete, 2026-10-12; MF-003 remains an open defect

The eight-hour campaign completed 451 attempts: 380 passed, 70 hit the assertions described below, and one hit a diagnosed host-runner timeout. No native crash or ANR was classified. All 111 normal level starts passed. Follow-up testing confirmed two engine bugs, which are now fixed and validated on the S21: a texture-index race and stale cross-mission secret companions. A stock Counterstrike save-range rejection remains unresolved

## Build, device and execution

Device: S21+ SM-G996U, serial `R3CR40Q4XPK`. Tests used the isolated package `com.dxxredux.app.nsdtest`; the Play installation and its saves were not modified

The main campaign waited for completed internal build 24970, source `cb32b6a96`. Its diagnostic launcher enabled automation and isolated application identity/signing; all 16 ARM64 native libraries were byte-identical to the completed official APK/bundle. Three artifact hashes and all 16 native-library hashes were independently reverified

Main output: `android/temp/mod-fuzz-s21-overnight-24970-20261012/`

Follow-up output: `android/temp/mod-fuzz-s21-followups-24970-20261012/`

The main Python process ran from 00:26:43 until approximately 08:26 PDT, consuming 28,766 seconds of its eight-hour budget and reaching the final-minute reserve. It continued through the conversation interruption. Detached follow-up jobs used verified predecessor process handles, serial device access, bounded children and durable results. All jobs have now exited

Only after the original-build app tests finished, a separate fix-validation APK replaced the two engine libraries. Its other ZIP entries matched the original APK, excluding signing metadata. After four successful app replays, the original diagnostic APK was restored. A live-device SHA-256 check matched the original APK exactly (`fd9fec3f136825c9e0733f440d1805c2fe304d9e3f16cdb5ab6d158dfbbb285e`), version code 24970. The phone's original stay-awake setting, 0, was also verified restored

Evidence: `build-provenance.json`, `build-provenance-reverified.json`, follow-up `fixed-apk/provenance.json`, and `final-device-verification.json`

## Coverage and outcomes

All nine recommendations and four alternate-resolution archives were exercised. Disabled archives and installed-list rows were retained and checked

| Main campaign phase | Attempts |
| --- | ---: |
| Baselines | 7 |
| Singletons | 35 |
| Pairs | 24 |
| All recommended mods enabled | 7 |
| Reversed priority | 4 |
| Normal level sweep | 111 |
| Authored UUD variants | 4 |
| Existing save-toggle/menu-recovery/UUD2SP regressions | 3 |
| First-secret entry/exit/revisit | 2 |
| Seeded random cases | 219 |
| Reproductions | 35 |
| Total | 451 |

All 197 systematic IDs ran. The 111 passing normal levels comprise native D1 27, D2 24, D1-in-D2 27, Vertigo 20, PC D1 demo 7, PC D2 demo 3 and Mac D1 demo 3. Actual palette snapshots cover all six D2 palettes. Pair results were 22 passes and two native D1 texture-owner failures; all four reversed-priority cases passed

The 70 main assertions group by observed reason into ten texture-owner failures, four extra-texture restore rejections, and 56 mission/source-definition restore rejections. These include repeated occurrences and automatic reproductions, not 70 distinct bugs. Controlled comparisons below establish the stale-companion cause for both Counterstrike and D1-in-D2

Additional completed checks:

- Nine targeted original-APK cases: four controls passed and five reproduced the known defects
- Sixteen shared-list cases passed: D1, D2, D1-in-D2 and Vertigo toggle/resume, engine switches, priority reorder and all-disabled gameplay, with all 13 archives installed. The eight applicable PC recommendations were enabled together; the Mac-only pack and alternate resolutions remained installed but disabled. Game-specific rendered textures and played sounds were checked
- Four full-app fix replays passed: two native D1 texture toggles, Vertigo-secret to Counterstrike save/restore, and Vertigo-secret to D1-in-D2 save/restore
- Native mod-save fixture: all 7 D1 and 20 D2 cases passed
- Native renderer fixture with the fix: all 11 D1 and 23 D2 cases passed, including 64 concurrent-metadata/index-rebuild iterations in each of three content modes
- Thirteen offline controller checks passed. Both Android engines and both native fixtures compiled. Scoped code quality and both test catalogs passed

Preparation also included a 19-case physical smoke run (18 passes and the first texture-owner failure) and an intentional assertion failure that verified artifact capture and power restoration. These are separate from the 451-attempt total

## MF-001: Temporary metadata files invalidate the texture index

Status: fixed and validated

With only `d1-hires-256-textures-ktx2.dxa` enabled, a fresh native D1 level rendered replacements. Disabling the pack, resuming, re-enabling it and resuming again could leave zero high-resolution textures despite the archive being installed, enabled and mounted. Retail 128px/512px variants and some demo cases also reproduced it. Gameplay restoration itself succeeded

All ten overnight owner-assertion failures had a failed final texture index. Both follow-up toggle replays still failed after ten-second observations; the fresh-start control passed. Texture diagnostics identified `.route_metadata_ingame_cpu_duty.<uuid>.tmp` as the rejected path. That unrelated root file can disappear between enumeration and `PHYSFS_stat` during an atomic metadata write, causing the index to discard every replacement

The shared `pngfile.c` fix skips irrelevant root entries before querying them. Checks remain for texture paths, allowed texture directories and hierarchy limits

The renderer regression rebuilds the index and checks actual GPU image/mask pixels while another thread repeatedly creates/deletes the temporary metadata filename, using the game's PhysFS policy. The old libraries failed the exact new assertion; fixed libraries passed all 34 renderer cases. The two fixed-app toggle replays also passed, each rendering 15 mipmapped 256px replacements after reactivation, with successful final indexes

Evidence:

- Main `00009`, `00012`, `00013`, `00033`, `00034`, `00060`, `00061` and associated reproductions
- Main `texture-index-correlation.json`
- Follow-up `02-native-d1-fresh`, `03-native-d1-toggle-long`, `04-native-d1-toggle-long-repeat`
- `renderer-baseline/d1.log`, `renderer-baseline/baseline-verification.json`, `renderer-fixed/d1.json`, `renderer-fixed/d2.json`
- `app-fix-validation/fixed-d1-toggle-1` and `fixed-d1-toggle-2`

## MF-002: Host launcher-handoff race

Status: fixed and replayed successfully; infrastructure failure

Case `00019-singleton-d1-in-d2-L1` timed out because the host's two-second fallback force-stopped the launcher six milliseconds after it began resuming the script. Exit history reported a user-requested force stop; no native crash or ANR was captured

The helper now uses its bounded, run-correlated handoff wait on physical targets as well as software emulators. Host regression checks passed. Follow-up `01-handoff-replay` then passed all 58 steps, restored the expected energy and rendered 15 mipmapped 128px replacements

The original helper, before/after hashes, logs and exit evidence remain in the main campaign artifacts. This timeout lacked final automation-result/introspection files; that capture gap is retained explicitly rather than counted as a gameplay result

## MF-003: Stock Counterstrike saves can hit the extra-texture guard

Status: open; reproduced without enabled mods

Counterstrike level 5 with UUD2SP alone rejected a disabled-mod restore with `Save uses extra UUD2 textures; enable UUD2 Texture Fixes`. Automatic replay reproduced it. Later level 16 with UUD2SP and 512px textures hit the same guard and reproduced it

Both primary saves have verified definition payloads with `textures = 910` and used texture ID 910. Their level-source hashes match the stock HOG members `d2levb-1.rl2` and `d2levd-4.rl2`; neither has an extra HAM source

The clean no-mod level-5 control also failed. Every mod was disabled, the HAM patch was explicitly missing, and no current Counterstrike slot companion accompanied the captured primary. Thus an enabled mod and the stale-companion defect are both unnecessary for this rejection

The remaining investigation must establish how the out-of-range reference enters the world and its intended meaning. Simply loosening the guard would weaken protection against genuinely missing assets. No MF-003 engine fix is claimed

Evidence: main `00199-random-d2-L5`, `00409-random-d2-L16` and their reproductions; follow-up `05-level5-no-mod-restore`, including its verified `save-pair-summary.json`

## MF-004: Fresh campaigns inherit a previous mission's secret companion

Status: fixed and validated

After visiting Vertigo's first secret, new Counterstrike and D1-in-D2 games could save their primary state together with the old Vertigo secret companion. Restore then correctly rejected the mismatched mission definitions or sources

A Counterstrike level-17 primary and its companion were captured together during the original failure; the primary bytes match the controller's failure archive exactly. Controlled follow-ups then removed active secret state and passed both destination cases. Adding one uninterrupted Vertigo-secret prefix reproduced both failures, with no automation cleanup between missions. Each failed pair has verified payload hashes: the primary identifies the destination mission, while its slot companion identifies `d2xlvls1.rl2` and `d2x.ham`

The Android new-game fix drains checkpoint work, removes the two active secret scratch files and refreshes the cached companion before starting a fresh D2 or D1-in-D2 game. It preserves per-slot companions and refuses to start if cleanup fails. It prevents new contamination; it does not repair already incompatible saves

The extended native fixture passed active-file removal, saved-slot companion preservation and repeated cleanup. Both previously failing full-app prefix cases passed with the rebuilt engines

Evidence:

- Main `00210`, `00226`, and `00227-random-d2-L17-repro-1/observed-save-pair.json`
- Follow-up `06-clean-counterstrike`, `07-clean-d1-in-d2`, `08-secret-then-counterstrike`, `09-secret-then-d1-in-d2`
- `11-native-fix/d1.json`, `11-native-fix/d2.json`
- `app-fix-validation/fixed-secret-counterstrike`, `fixed-secret-d1-in-d2`

## Lessons and limits

Fresh starts were strong coverage for loading the recommended packs, but transitions exposed the important bugs. Checking that an archive is mounted is insufficient: MF-001 required checking what reached rendering. Repeated incompatibility messages also needed save-pair evidence to distinguish a wrong companion from an incompatible mod

Only the first authored D2 and Vertigo secrets were exercised, including exit/revisit and a retained robot-state marker. Other secrets remain a coverage gap. Random non-diagnostic gameplay cases skip asset-owner validation; seven passing D1 random cases also had a failed final texture index, so those passes do not establish correct replacement rendering. The run is not a frame-pacing benchmark, and no observed crash/ANR does not prove crash freedom

The original failure filter omitted `.sgb`/`.sgc` companions. Future capture now includes them; selected original pairs were recovered separately, and controlled follow-ups captured complete pairs. Empty-mod reductions now retain save/restore transitions. Failure-signature grouping now ignores unrelated Android service noise while preserving native fault locations

The first detached app follow-up attempt failed before gameplay because the copied controller lacked its sibling helper import path. Those jobs are preserved under `import-failure-*` and are excluded from case counts. The corrected wrapper was offline-validated and all planned jobs subsequently ran. A renderer waiter date-parsing error was also corrected before any device action. Its initial baseline summary scanned wrapper logs only; the native assertion is verified separately in `baseline-verification.json`. Original raw results were preserved

`audit.py` in the follow-up directory regenerates `completion-audit.json` from case contents rather than case-ID labels. Final device verification confirms the original diagnostic APK bytes, restored power setting, and unchanged tested source hashes. All background jobs are terminal. The remaining engineering follow-up is MF-003
