# Plan: PC-Side Native Extract Utilities

Status: Completed 2026-04-12 for the late-stage desktop utility tranche.

## Goal

Promote the Android-side native extraction code into real PC-side utilities so
desktop workflows can use project-owned tools instead of external archive
tools for normal extraction. Keep external tools only for creating regression
oracles and manifest fixtures.

## Current gap

- `extract_cd.exe` already handles ISO 9660 and hashes tracks, but it stops at
  HFS detection and does not run the native STi2 extraction path
- the Android JNI path already performs HFS `Install Descent` extraction plus
  STi2 file extraction
- `game_data/extract_all_cds.ps1` still falls back to `extract_mac_cd.ps1`
  when `extract_cd.exe` hits a Mac HFS disc
- `game_data/extract_mac_cd.ps1` still uses Python plus `unar` as the desktop
  reference pipeline for the committed MacPlay oracle files

## This tranche

- [x] move the HFS plus STi2 extraction helper logic out of JNI-only code into
  shared native code callable from both JNI and desktop tools
- [x] teach `extract_cd.exe` to extract Mac HFS disc content directly into
  `data_tracks/`
- [x] add an end-to-end desktop parity test that runs the native Mac path and
  compares output bytes against the checked-in MacPlay `data_tracks/` oracle
- [x] update `extract_all_cds.ps1` to trust the native Mac path instead of
  falling back to the legacy script
- [x] leave `extract_mac_cd.ps1` in place as the legacy oracle-generation path
  for now

## Validation target

- native desktop extraction of `Descent - Mac macplay` produces the same file
  set and bytes as `game_data/CD images/Descent - Mac macplay/data_tracks/`
- the committed regression files do not change when re-generated via the new
  native desktop path

## Validation completed

- `ctest --test-dir android/tests/build -C Debug --output-on-failure -R
  "hfs_tests|sti2_tests"` passed after the shared-helper refactor and again
  after code formatting
- `android/tests/build/Debug/extract_cd.exe` extracted the MacPlay data track
  to `temp/` with file-set parity and byte-for-byte parity against the checked-
  in `data_tracks/` oracle
- `android/tests/build/Release/extract_cd.exe` produced the same file-set and
  byte-for-byte parity, matching the binary used by `extract_all_cds.ps1`
- the `extract_cd.exe` stdout JSON was kept compatible with the committed
  `track_hashes.json`, so the native path does not churn the regression file
- `android/run-code-quality.ps1 -Fix` completed successfully
- `android/gradlew.bat :app:assembleDebug` completed successfully, compiling the
  shared helper through the Android Gradle plus NDK path
