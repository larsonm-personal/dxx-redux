# Enemy Within full archive import

- [x] Trace the 936,110,524-byte wrapper through the launcher's stream probe
- [x] Reproduce the source-limit failure in a regression test before changing limits
- [x] Raise the disk-staged source ceiling to 2 GiB, retaining entry and memory budgets
- [x] Test the full wrapper through the Android URI probe and import, checking Rebirth selection and extracted assets
- [x] Run archive/mission unit tests, Android build/device regression, scoped quality and test catalog checks

The October 2 source-limit fix capped ZIP sources at 512 MiB. Existing Enemy
Within device tests stage the 408,845,824-byte Rebirth child instead of the
936,110,524-byte wrapper. The real-pack JVM test calls the file-based importer,
which also bypasses the launcher's stream probe. Neither covers this regression.

## Validation

- The real-pack test failed before the change with `ZIP source exceeds 536870912 bytes`
- After the change, all 146 selected JVM tests passed with no failures or skips
  (`ArchiveInputStreamsTest`, `ExtractionLimitsTest`, `MissionZipTest`,
  `ModManagerMissionZipTest`, and `MissionZipMusic*Test`)
- The unconditional synthetic test reads an entry at the existing 512 MiB entry
  ceiling from a ZIP larger than 512 MiB, checking CRC, byte count and cleanup
- The real-pack test now probes the original 936,110,524-byte wrapper before
  importing it, instead of bypassing the launcher probe
- Registered `test_enemy_within_import` in the normal storage-import suite,
  enabled instrumentation provisioning and allowed 1,200 seconds in the master runner
- Both automation catalog checks and scoped mixed-language quality checks passed
- ARM64 Android CMake, app and instrumentation builds passed
- `test_enemy_within_import.ps1 -Serial RFCY703C48J` passed on Samsung SM-S931U
  using `com.dxxredux.app.nsdtest`. It verifies the original wrapper hash,
  successful stream probing, URI import, Rebirth selection and archive hash,
  extracted MN2/HOG/DXA hashes, mission catalog ownership, and staging cleanup
- Retroid disconnected during validation; the successful device result is Samsung
- Fixed the master runner's app and instrumentation APK paths to use the current
  Gradle `intermediates/apk` artifacts. The previous `outputs/apk` files were stale
  (main APK version 24450 versus rebuilt 24500). The new test correctly rejected
  an unrelated PASS from stale instrumentation before this was diagnosed
- Local diagnostic installs only; no production release was published

Logs: `android/temp/enemy-within-import-20261008/`
