# Production import coverage

## Objective

Keep the existing extraction architecture lightweight while sharing decisions
that can drift and testing the original sources through the launcher paths.
Do not rewrite working codecs or build a second test-only importer.

## Work and acceptance criteria

- [x] Move split SOW assembly into a shared native batch API using ARJ continuation
      flags and offsets, used by the desktop CD tool, Android CD import, and nested
      demo extraction; eliminate per-package append decisions
- [x] Cover complete, reordered, missing, conflicting and damaged volumes with
      native tests, including real demo/CD output hashes
- [x] Extract only the launcher operations necessary to call real archive import
      and publication from integration tests, with complete known-package validation
- [x] Add a reusable original-demo corpus test invoking production launcher
      extraction, checking installed output paths, sizes, hashes and readiness
- [x] Strengthen CD regression assertions to validate actual expected contents,
      distinguish extraction from launch coverage, and exercise the split-volume CDs
- [x] Run the host corpus, Android integration, appropriate builds, scoped quality,
      and automation catalog checks; audit the final coverage against this plan

## Constraints

Preserve unrelated concurrent worktree changes and user device data. Run Android
tests serially on a test emulator. Existing native format code remains shared.
Independent expected hashes remain reviewable data, not regenerated from a test
run's output. Required corpus fixtures must fail clearly when absent.

## Evidence

The raw D1 and D2 SOW entries have ARJ flag 0x04 for a file that continues and
0x08 plus a 32-bit offset at header byte 30 for its continuation. This matches
the ARJ reference format (`https://github.com/joncampbell123/arj`, `defines.h`
and header readers). The previous blanket append option ignored these fields.
Host and launcher CD loops both used overwrite mode, while the existing native
real-media test supplied append mode explicitly. General CD regressions checked
filenames but did not compare expected output hashes.

## Findings and implementation

The defect was a policy gap: host tests supplied append mode while production
callers chose overwrite or collision rejection. The batch API now derives volume
order and completeness from ARJ flags and offsets. Both CD entry points and nested
ZIP demos call it; package-specific volume lists are gone.

The device corpus found two additional real bugs: legacy ZIP filenames were read
as UTF-8, and BinHex CRC calculation added two erroneous zero bytes. The BinHex
synthetic fixture generator repeated the decoder mistake. Fixed ZIP handling in
the existing shared stream reader (also used for mission archives), corrected
BinHex, and pinned a fixed CRC vector independent of the Kotlin fixture generator.
CPython's reference `Lib/binhex.py` and `binascii.crc_hqx` agree with the original
HQX header CRC 0x249c, versus the old calculation's 0x8817.

Downloaded demos now call `installDemoArchive`, which requires complete extraction
and uses existing archive rollback publication for game files and assets.json.
The instrumentation calls this production operation. It checks all 11 catalogued
original packages against reviewed hashes, canonical installed paths, manifests,
launcher readiness, and failed-import preservation. Missing fixtures fail the
runner; they cannot silently become a passing corpus test.

Correct assembly also exposed stale Test Flight identification: its prior PIG
identity described only the final 28,518-byte fragment. Recognition now uses the
complete 5,092,871-byte file. Its existing unsupported-launch policy is preserved.

## Reusable checks and evidence

- `android/tests/test_demo_import.ps1 -Serial <test-emulator>` runs the production
  demo operation using the installed debug APK and RecoveryInstrumentation APK
- `sow_integrity_tests` covers reordered, incomplete, overlapping, conflicting,
  corrupt and cancelled volume assembly; `sow_real_media_tests` checks retail,
  Preview and Test Flight through the shared volume and staged-disc APIs
- `test_all_extracts.ps1 -Filter '*Test Flight*' -SkipLaunch` and
  `-Filter '*3-Level Interactive Preview*' -SkipLaunch` now compare installed
  sizes and SHA-256 against reviewed `fixtures/cd_import_oracles.json`
- The new demo runner is registered with the master suite, instrumentation build
  requirement and 900-second timeout
- Host assembly passed for all 17 available CD SOW parent groups; full original
  Test Flight and Preview CD extraction matched the same pinned byte oracles
- Android Test Flight import passed; Preview import matched all three pinned
  files and explicitly reported its existing game-launch exclusion as SKIP
- All 11 original demos passed on emulator-5582; physical device data was untouched
- 113 focused JVM tests passed, including mission ZIP consumers of the shared
  stream reader; native SOW tests and both automation catalog checks passed

## Deliberate limits

This is extraction/publication/readiness coverage, not a claim that every demo
was played. General CD specs without a pinned content oracle still retain their
existing filename and launch coverage. Host mission metadata generation is not
an Android storage test, and GOG push-based game-launch tests are not substitutes
for the separate actual installer-import tests. No broad codec/framework rewrite
was needed: Android-specific I/O is covered on Android and shared format decisions
remain in the existing cross-platform core.

Final verification: full Android debug and instrumentation builds passed with
all three native ABIs after the unrelated concurrent engine edit was completed.
The final emulator run passed all 11 demos, real launch-readiness checks and both
incomplete-package and missing-volume preservation cases. The final native build
and all three SOW suites passed. Scoped code-quality checks and diff whitespace
checks passed. No physical-device changes or unrelated worktree edits were made
for this coverage work.
