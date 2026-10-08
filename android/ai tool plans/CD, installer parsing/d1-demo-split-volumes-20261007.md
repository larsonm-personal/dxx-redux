# D1 PC demo split-volume import

- [x] Inspect the connected phone's import logs and trace the collision check
- [x] Verify the D1 installer volume layout and reference output hashes
- [x] Declare the D1 split volumes and extend existing import regression coverage
- [x] Run scoped formatting, relevant builds/tests, and verify the fixed import on the device

The phone downloaded `desc14sw.exe` successfully. The launcher extracts
`descent1.sow` and `descent2.sow` separately with append disabled, then rejects
their shared `descent.pig` output. D2 already declares its split volumes in
`DemoInstallerPackages`; D1 currently lacks that declaration.

Both known D1 PC packages contain byte-identical `DESCENT1.SOW` and
`DESCENT2.SOW` volumes. The first produces only 1,416,672 bytes of the PIG;
the reference assembled PIG is 5,092,871 bytes, SHA-256
`b67865e513452a35887a20270d17fdfb5af1a2edaaae247bc523489f1d84f9ac`.
The fix declares the two ordered volumes for both packages, reusing the
existing assembly path and retaining collision rejection for unrelated archives.

Validation:

- Scoped code quality passed
- Host extraction of both real D1 packages produced the expected HOG and full
  PIG hashes using the newly declared volume order
- CMake Release build of `test_sow_direct`, `test_sow_integrity`, and
  `test_sow_huffman` passed; all three SOW CTests passed, including real D2 media
- The existing package unit suite now checks ordered volume declarations for
  both D1 and both D2 PC packages
- `:app:assembleRelease` for the existing legacy distribution passed, including
  native builds for all three ABIs
- `:app:testDebugUnitTest` passed all 17 tests in `DemoInstallerPackagesTest`
  and `ArchiveInputStreamsTest`
- Both automation catalog validation scripts passed
- Updated the existing `com.dxxredux.app.github.legacy` installation on
  `JYPR42510121028` with `adb install -r`, then used the launcher's D1
  `Download and install` button. Both SOW extractions logged `append=true`,
  the resulting PIG was 5,092,871 bytes with the expected hash prefix,
  and the launcher displayed `Installed D1 Demo: 2 files` and `Descent 1 Ready`
- Left the device at the launcher with the demo installed

Device evidence: `temp/d1-demo-device.log` and `temp/d1-demo-screen.png`.
Build log: `android/temp/d1-demo-build-final.log`.
