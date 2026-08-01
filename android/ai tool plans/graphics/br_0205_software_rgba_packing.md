# BR-0205 software RGBA packing

## Goal

Make the Android software renderer write the exact RGBA byte order declared for its native-window buffer and reject an unexpected locked-buffer format.

## Plan

- [x] Trace palette conversion, native-window format selection, and software renderer build coverage
- [x] Add one endian-independent RGBA byte packing helper with focused sentinel tests
- [x] Update the software blitter to validate its locked format and write explicit RGBA bytes
- [x] Build and run the focused native test and Android product code; document the separate software-mode configuration blocker
- [x] Run scoped code quality and archive BR-0205 with dated evidence

## Verification

- The focused RGBA8888 test passed pure red, pure blue, asymmetric color and alpha, transparent, grayscale, and destination guard-byte cases.
- All 22 native CTests passed.
- Android debug assembly passed for arm64-v8a, armeabi-v7a, and x86_64, compiling the production blitter for every supported ABI.
- A direct x86_64 software-mode configuration reached and reproduced BR-0394: the Android parent tries to mutate the omitted `d1_arch_ogl` and `d2_arch_ogl` targets. That separate P1 blocks the paired software emulator comparison but does not affect the byte-level regression or default build.
