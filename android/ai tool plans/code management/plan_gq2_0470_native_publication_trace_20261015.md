# Chunk 0470 native installer and publication trace, 2026-10-15

Diagnosis and plans only. Partial consumer continuation under plan_general_diagnosis_resume_20261009.md; no product or test edits, fixture execution, payload inspection or new canonical disposition.

The previous turn made concrete progress with the GOG/GLES consumer checkpoint and PASS110 exact-source replay. This checkpoint continues actual native dispatch, output checks, Kotlin publication/copy and renderer configuration. Canonical 0470 remains TODO and its consumer gate remains open.

## Native manifest contract

JNI build_inno_file_list validates unique selected output paths and records names and declared sizes. extract_inno_archive validates unique selected names again, extracts selected game/audio entries, fails on an individual error, and propagates observed callback cancellation. It does not compare against the Java analyzed manifest.

nativeExtractFiles consumes expectedEntries only in its PKG branch. PKG compares count and exact ordered name/size/CRC strings before extraction. pkg_extract_all then checks entry/output budgets, regular selected records through its classifiers, every selected entry including skipped audio against the scanned manifest, per-file CRC before finish, complete expanded gzip stream, manifest count and Scripts CRC. It aborts the current physical output on detected copy or manifest failure. These source checks preserve BR-0069 FIXED; no fresh runtime or full parser-family claim follows.

Inno opens a fresh archive after the Kotlin manifest re-list. The native branch does not consume expectedEntries, so do not generalize PKG's caller-manifest binding to EXE. Potential replacement between re-list and native open remains a source-admission question for whole-owner reconciliation, not an admitted new finding here. No installer content was opened.

Inno per-file extraction validates the file/data index, rejects encryption and unsupported checksum types, validates chunk range, applies entry/total/count/free-space budgets, and checks progress cancellation. The buffered path validates checksum, requires exact written length and successful close, then commits a temporary output. Streamed regular/Galaxy calls propagate their errors; Galaxy also compares measured output length. The complete stream helpers, path validator, physical-output implementation and decoder families were not read here; no whole-family safety acceptance.

## Publication and cleanup

prepareDiscContent retains runtime, referenced mission/companion and GOG audio files, checks deletion of unused files, then stages the owned content entry. publishStagedArchiveFiles backs up replaced files, renames or exact-copies each stage file and records completed destinations. On Exception it deletes completed destinations and attempts reverse backup restoration. A failed restore preserves the backup and throws an explicit error. The reverse restoration uses all, which short-circuits after a false result; retain this exact limitation for later ownership reconciliation. There is no process-death transaction journal in this function. These facts do not by themselves reopen completed nominal failure handling.

LauncherFileCopy checks positive expected size, handles zero reads with a one-byte read, enforces maxBytes, writes a UUID sibling, flushes and syncs, then calls AtomicFilePublication. The latter serializes publication and retains a prior target through rename replacement. Its batch and multi-file operations are separate APIs; do not imply that archive publication uses their entire transaction contract.

Whole BR-0020 remains FIXED for fail-fast extraction and staged publication. Whole BR-0456 remains OPEN after reopening for shared temporary-root cleanup and overlapping-attempt lifetime; its prior exact-copy repairs remain intact. BR-0069 remains FIXED for PKG manifest parity. Their historical test records are not fresh execution in this tranche. Preserve owner status and acceptance until remaining cleanup callers are traced.

## Renderer and script staging

Gradle defaults useOpenGL to true, records BuildConfig.RENDERER and passes ANDROID_USE_OPENGL. CMake maps that exact string to OPENGL. The committed gradle.properties selects true. This is default source configuration, not proof of the installed APK's renderer.

Test-SoftwareAndroidTestTarget refers to explicit emulator mode and serial, and changes infrastructure/navigation budgets. It is not the game engine software-renderer selector. Prior software-probe stub observation therefore does not imply that the ordinary software CPU emulator uses that stub.

run_test accepts caller-selected DeviceName staging and checks device pathname presence, while push/copy/mkdir return codes and script digest are not checked in this function. Full caller naming/result admission and existing transport owners still need reconciliation. Preserve the distinction from the installer payload helper's size/SHA checks.

## Read recovery and next work

Windows wildcard file arguments to rg failed; corrected directory/glob searches supplied locators. The first combined publication read clipped unrelated helper initialization; only complete selected function boundaries are credited. gradle.properties guessed end16 exceeded its actual13 rows, so it was reread whole; subsequent CMake and PKG tail reads then completed. Combined owner output clipped portions; BR-0020 missing middle and BR-0456 missing lines16672-16673 were recovered explicitly. Accepted ranges are embedded exactly in the package.

Next: whole wrapper/master variant execution, deadlines, cleanup and final-result ownership; menu preset fixture/reset/identity, automap and Guidebot consumers; remaining material BR/quality/prior-plan reconciliation. Then synthesize direct unpooled disposition and immutable canonical publication/terminal Drive verification. Source gate only remains closed, no canonical ledger append, finding, remediation, rating, inherited saving or runtime credit. Broad queue/sweeps/preflights/investigations/supplements and closure remain required.
