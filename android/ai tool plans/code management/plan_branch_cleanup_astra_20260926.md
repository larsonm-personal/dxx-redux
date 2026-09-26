# Fresh branch cleanup audit - 2026-09-26

## Scope and baseline

Continue cleanup and simplification of all branch changes, with particular attention to minimizing inherited D1/D2 source diffs without moving engine format knowledge out of the engine

- Baseline: `d19b478e` (`rework d1/d2 launch buttons`)
- Prior completed work: [2026-09-25 plan](plan_branch_cleanup_astra_20260925.md), including continuation committed in `1721f72b`
- Initial dirty file: `android/outstanding_bugs.md`; preserve concurrent user changes
- Fresh inventory and duplicate survey: `temp/cleanup_astra_20260926/`
- Historical worker dispatch instructions are not part of this plan

## Fresh candidates and execution order

1. Move identical Android launcher thumbnail capture from both inherited `state.c` files into existing `state_android_shared.c`
   - Preserve allocation failures, canvas restoration, RGB6 conversion, and framebuffer readback
   - Keep native save serialization and desktop thumbnails in their engines
   - Validate both Android game builds and save/load integration; inspect saved thumbnail metadata
2. Make the audio source BIN URI list canonical
   - Remove redundant singular persisted field and fallback helper, updating current producers and consumers together
   - Preserve single/multiple BIN imports, URI order, source visibility, and artifact ownership
   - Run relevant JVM tests and audio import/preview integration
3. Remove the unused `legacyMissionRequirement` builder after confirming no references
4. Audit formatter wrapper discovery/filter duplication
   - Directory filters currently use prefix matching without a path separator
   - Share discovery only if it simplifies wrappers while preserving their distinct scopes and dependency handling
   - Exercise explicit files, directories, siblings, nonexistent paths, and inherited-file exclusions
5. Evaluate human-readable control parsing duplication
   - Named bindings and numeric bindings have different semantics; do not consolidate constructors without preserving warnings and defaults

## Boundaries

- Do not broaden into upstream engine deduplication or private network queue policy
- Old Android persistence migrations can be removed; supported game and disc formats remain supported
- Recheck concurrent build activity before invoking Gradle/Ninja
- Format changed owned files in one scoped invocation and wait for completion
- Record actual validation outcomes and final diff metrics before concluding

## Progress

- Fresh branch inventory and duplicate survey complete
- Thumbnail implementations selected for first mechanical comparison and extraction
- Thumbnail bodies mechanically compared: identical except the render signature
- Shared thumbnail implementation added; 138 inherited lines removed with call ordering unchanged
- Singular BIN-URI storage and fallback helper removed; all producers, consumers, JVM fixtures, and the launcher integration fixture use the canonical ordered list
- Unreferenced legacy mission requirement builder and its unused imports removed
- Seven formatter wrappers now share file selection; directory boundaries exclude similarly named sibling directories
- File-selection regression checks pass, including CMake allowlists and inherited exclusions; scoped format/lint and an actual CMake format/lint invocation pass
- Existing autosave/resume integration now asserts readable 200x100 launcher thumbnails at all three checkpoints
- Both Android engines compile for all three ABIs; APK packaging and all 43 targeted JVM tests pass

## Candidate dispositions

- Keep human-readable and numeric control deserializers separate in this round: human-readable parsing validates named bindings, suppresses inactive button-mode bindings, accumulates warnings, and normalizes extreme actions. A shared constructor alone would obscure those differences; JSON rewriting would add another intermediate representation. This deserves a focused serialization change if pursued later
- Preserve per-tool scope/exclusion policy and tool discovery in formatter wrappers; only file selection is shared. CMake's existing allowlist is unchanged
- Desktop state code is untouched: the deleted inherited blocks were entirely inside `__ANDROID__`, and the existing shared source is built under Android CMake guards

## Validation evidence

- `:app:packageDebug :app:testDebugUnitTest` succeeds; 43 tests across seven audio, ownership, and multiplayer protocol suites pass
- All six packaged D1/D2 ELF build IDs match the newly linked arm64-v8a, armeabi-v7a, and x86_64 artifacts
- Shared thumbnail body tokens match both prior functions after selecting the game-specific render signature, including allocation failures and canvas restoration
- Extended autosave/resume integration passes 45 D1 steps and 44 D2 steps, including missing-pilot restoration and readable thumbnail metadata at all checkpoints
- Saved D1 and D2 trailers each contain 60,000 RGB6 bytes at 200x100, all channels within 0..63, with 49 and 53 distinct channel values respectively
- GOG Windows installer import, MIDI/CD preview, and in-game redbook playback pass all 55 steps, including wrapper cleanup
- Referenced-BIN redbook integration passes all 69 steps, including playback, transport controls, and source-handle failure handling
- File-selection regression and final scoped code quality checks pass; direct CMake format/lint checks pass on a real in-scope file
- Only existing engine warnings were emitted (DGSS array terminators and D1 ObsChat array truthiness); no new warnings originate in the shared helper
- Tracked cleanup patch passes reverse-apply and whitespace checks; user-owned outstanding bugs edits are excluded

## Final result

All four selected changes are implemented and validated. The control parser candidate has an explicit deferral above

- Owned code and tests: +414/-610 across 29 paths, net 196 lines removed, excluding this plan
- Inherited engine files: 138 lines removed, no additions
- Exact patch, new-file copies, metrics, equivalence checks, build/JVM logs, APK build IDs, saved-thumbnail measurements, and integration logs are under `temp/cleanup_astra_20260926/`
- No staging or commits performed; `android/outstanding_bugs.md` left untouched
