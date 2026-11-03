# Pumo metadata browser loading follow-up

Fix extracted-file metadata fallback that reopens a solid archive when a local
file legitimately has no specialized summary. Skip unsupported file types before
opening archives, and keep extracted sources authoritative. Add local timing/error
diagnostics for supported metadata reads. Add independent Files list scroll arrows.

Movie inspection still requires the classic mission loader even in movie-only mode.
Do not claim it supports XL or send known unsupported XL targets to that worker.
Show an explicit movie-inspection limitation while retaining feature inspection.
Reuse the already inspected reports instead of rescanning the entire package.

Validation completed:

- 14 focused JVM tests passed (GameFileMetadataTest and MissionMovieScanTest)
- Debug x86_64 APK and instrumentation APK assembled successfully
- Extended package_documents instrumentation passed on owned DxxTraversal20261102,
  emulator-5556: extracted media details, independent Files down/up arrows, and XL
  movie boundary; final extracted WAV metadata read took 1 ms
- Scoped formatting/lint and git diff whitespace checks passed
- UI test refreshes accessibility nodes after scrolling and waits for the parent
  dialog after closing details; failure diagnostics include tree and screenshot

Evidence is in temp/pumo-metadata-loading: build-tests.log, build-ui-test.log,
instrumentation.log, ui-logcat.log and formatter logs. The retained failure.png
shows the correctly rendered list arrow during an earlier accessibility-cache
failure in the test, not a remaining rendering defect.

Limits: UI validation uses an extracted synthetic mission package, not a complete
original Pumo reimport or a physical device. Unit tests use an unreadable parent
archive to prove media details never reopen it. Large HOG inspection still reads
HOG contents; this change does not optimize all supported metadata parsers. XL
movie references remain unverified, and classic route/statistics analysis remains
unavailable for unsupported XL formats. Feature inspection stays available.

Changes are local and uncommitted. No metadata regeneration, mission edits, save
clearing or published-bundle replacement was performed. The owned emulator was
installed with data-preserving debug updates; retain its profiles and disk state.
