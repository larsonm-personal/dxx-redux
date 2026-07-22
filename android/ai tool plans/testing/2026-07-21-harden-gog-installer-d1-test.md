# Harden GOG installer D1 test

## Plan

- [x] Inspect the complete failure log, wrapper, automation script, and import diagnostics
- [x] Trace host and device installer validation plus inherited extraction state
- [x] Implement bounded missing-size Galaxy extraction without extending timeouts
- [x] Add synthetic and real-installer coverage for missing Galaxy expanded sizes
- [x] Run scoped code quality, broader sequence/build verification, and record results

## Findings

- The report and an exact `test_extract` -> `test_gog_installer_d1_unified` reproduction both fail in about 25 ms inside native extraction; the 120-second wait only hides that immediate error.
- The pushed device fixture matches the host SHA-256 (`cd754293de928f73a3772630a85d274b22940c857b1aab80078afa962a5492a7`).
- Native diagnostics show all seven D1 files rejected as Galaxy files with no declared expanded size.
- The D1 installer legitimately stores all seven game files as nested Galaxy zlib streams, but its Inno version does not populate `external_size`. Treating those entries as ordinary files publishes compressed data and crashes the game at launch.
- Missing sizes are now measured with a no-output streaming inflate pass. The existing per-entry expansion cap is enforced during measurement, then ratio, total-size, and free-space limits are checked before the real extraction publishes anything.
- The real extraction must reproduce the measured size and checksum; mismatches remove the output.

## Verification

- Scoped code quality passed for `inno_reader.c` and `test_gog_fd.c`.
- Focused native `test_gog_fd` passed with synthetic missing-size and declared-size streams plus the real D1 and D2 installers. It asserts all seven D1 game entries are Galaxy-classified and verifies checksum failures do not publish files.
- Android `assembleDebug` passed with JDK 21 and the APK installed successfully on the emulator.
- Windows CMake builds passed for D1 and D2.
- The exact `test_extract.ps1` -> `test_gog_installer_d1_unified.ps1` sequence passed: the predecessor reached Lunar Outpost, D1 imported seven files, all launcher assertions passed, and the imported game reached Lunar Outpost (`PREDECESSOR_EXIT=0`, `TARGET_EXIT=0`).
- No timeout values were changed.
