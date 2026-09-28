# Suite failure fixes

- Diagnose the four failures from report_20260927_160331.md without changing existing game-data edits
- Correct Unicode fingerprint output handling and verify enumeration
- Fix mission extraction publication paths and failed-batch reporting; cover publication with JVM tests
- Close the About dialog before continuing the graphics automation
- Investigate baseline numeric representation differences and preserve meaningful regression checks
- Run affected host/JVM tests, build Android if needed, and run device tests serially

## Fixes and validation

- Hidden Windows workers inherited a non-UTF-8 decoder. Explicit UTF-8 output
  decoding fixes the fingerprint filename mismatch; the same filtered suite run
  reproduced the failure before the change and passed afterward
- Mission managers shared a staging directory without coordinating publication.
  A per-store lock now covers extraction and manifest mutation across instances.
  The overlapping-manager regression and existing extraction/manager JVM tests
  passed (42 tests); Android debug assembly passed
- Failed ZIP summaries tolerate absent automation reasons under strict mode;
  checked with a synthetic missing-result record and existing batch tests
- Graphics automation closes About before looking for Advanced
- Baseline changes were confined to the last few floating-point bits of computed
  route distances. Comparison accepts up to four adjacent doubles only on those
  fields; all other fields remain exact. The baseline is unchanged. The full D1/D2
  baseline and tests rejecting meaningful distance changes passed
- Process-pool tests, PowerShell compatibility checks and scoped formatting/lint passed
- Device mission ZIP sample passed all three archives, including `-MOON-.zip`
  (29/29 steps each), with regression JSON publication disabled
- A graphics rerun encountered a separate emulator System UI ANR that stole dialog
  focus. Click diagnostics confirmed the app click succeeded; temporary click
  changes were removed and the emulator was cold-restarted for final validation
- Final graphics automation passed D1 42/42 and D2 67/67 steps on the clean
  emulator with the final APK; no changes to accessibility clicking remain

Logs are in `temp/fingerprint-suite-fixed.log`, `temp/secret-baseline-fixed.log`,
`android/temp/suite-fixes-build.log`, and `temp/suite-fixes-quality.log`

Device logs: `temp/mission-zip-suite-fixed.log`, `temp/ogl-runtime-final.log`
Final Android build: `android/temp/suite-fixes-final-build.log`
All four affected tests were rerun; the entire 77-minute suite was not repeated
