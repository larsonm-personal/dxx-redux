# CD audio synthetic storage cleanup 2026-05-07

## Goal
- Hide launcher-managed synthetic CD source artifact files from the App Storage Files dialog
- Clean up merged-local synthetic BIN/CUE artifacts whenever the owning CD source is removed

## Plan
- [completed] Confirm which CD source files are synthetic launcher-managed artifacts and where removal currently leaks them
- [completed] Centralize CD source artifact cleanup in AudioSourceManager so all remove paths delete the same local files and release permissions when applicable
- [completed] Filter internal CD source artifact files out of the storage browser using the same source-owned artifact set
- [completed] Add focused JVM coverage for the artifact helper logic
- [completed] Run focused Kotlin compile/tests and the Android code-quality pass, then rerun validation

## Notes
- Current hypothesis: merged-local SAF imports create local `.bin` and `.cue` files in `filesDir`, but source removal only deletes the cue in one UI path and never hides those internal artifacts from the storage browser
- Cheap check: compare the storage browser file listing with current CD source removal logic. Readout confirmed the browser walks all files under `filesDir` unfiltered, while source removal is split across UI call sites and leaks merged-local `.bin` files
- Implemented: storage browser now hides launcher-managed internal CD artifacts and generated merged-local orphan artifacts, while CD source removal now deletes owned local artifacts and prunes unreferenced generated merged `.bin`/`.cue` pairs
- Validation: focused Gradle compile/tests passed after the code-quality pass. `run-code-quality.ps1 -Fix` still reports existing non-autofixable `SetupActivity.kt` max-line warnings and a summary line that lists `psscriptanalyzer` despite its fix pass completing
- Follow-up hypothesis: merged-local imports still use the source id as the staged file stem, which yields `custom-...` filenames for unknown discs, and the SAF browser still lists merged-local sources because it treats local absolute paths as link entries instead of requiring a real SAF URI
- Follow-up plan: rename merged-local staged files from the original CUE base name, keep the single-BIN SAF path unchanged, and filter merged-local sources out of the SAF links browser so only actual SAF-backed CD sources appear there
- Follow-up implemented: merged-local staged `.bin`/`.cue` files and copied local CUE files now derive their stem from the original CUE base name with collision handling, while the SAF browser now skips merged-local and other local-path CD sources and shows only actual SAF-backed CD entries
- Follow-up validation: focused Gradle compile/tests passed before and after the code-quality pass. `run-code-quality.ps1 -Fix` still reports the existing non-autofixable `SetupActivity.kt` max-line warnings and the same misleading summary line for `psscriptanalyzer`
- Visibility correction: App Storage Files now hides only SAF-backed helper artifacts such as the local copied CUE for a SAF source, while merged-local CD BIN/CUE files are shown as real app-storage files so multi-BIN imports appear in one browser or the other as expected
- Visibility correction validation: focused Gradle compile/tests passed before and after the code-quality pass. `run-code-quality.ps1 -Fix` still reports the same existing non-autofixable `SetupActivity.kt` max-line warnings and misleading `psscriptanalyzer` summary line
- Label follow-up: generated merged CD BIN/CUE files in App Storage Files are now labeled `Imported (merged) CD audio` instead of the generic BIN/CUE type labels
- Label follow-up validation: focused Gradle compile/tests passed before and after the code-quality pass. `run-code-quality.ps1 -Fix` still reports the same existing non-autofixable `SetupActivity.kt` max-line warnings and misleading `psscriptanalyzer` summary line
