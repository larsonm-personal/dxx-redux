# BR-0647 Remediation Plan

## Scope

Make the maintained D2 MSAA smoke test prove successful multisampled framebuffer creation, use, and resolve independently of the requested setting

## Work

- [x] Inspect the finding, Android MSAA lifecycle, introspection, test script, and runner
- [x] Publish persistent effective MSAA diagnostics and assert current-frame progress
- [x] Run scoped quality, native/build validation, and the maintained emulator test
- [x] Record exact validation and archive BR-0647 in the done ledger

## Validation

- Scoped C/C++ quality and `git diff --check` passed
- Android debug assembly passed for arm64-v8a, armeabi-v7a, and x86_64 with JDK 21
- The final installed D2 run passed with maximum 4 samples, effective 2x MSAA at 640x480, generation 1, 16 binds, 16 resolves, complete status, zero GL error, and a resolved latest pass
- The paired D1 unified automation and both Windows builds passed; the Windows CMake trees register no CTests
