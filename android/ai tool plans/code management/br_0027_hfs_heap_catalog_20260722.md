# BR-0027 heap-backed HFS catalog reuse

## Goal

Load one dynamically sized HFS volume and catalog context, reuse its validated
entries for listing and extraction, and report allocation or capacity failures
instead of consuming large fixed stack frames or silently truncating results

## Plan

- [x] Read repository instructions and the complete BR-0027 finding
- [x] Trace catalog parsing, public API ownership, Mac fallback callers, tests,
  and Android build inclusion
- [x] Introduce an owned heap-backed HFS catalog context with checked growth
  and direct entry extraction
- [x] Migrate listing, single-file extraction, and Mac extraction callers while
  preserving compatibility where practical
- [x] Add allocation, growth, scan-count, reuse, and extraction regression tests
- [x] Run scoped code quality, native tests, frame-size checks, and Android ABI
  builds
- [x] Finalize the finding disposition and validation record

## Validation

- Scoped code quality passed for all changed C, header, CMake, test, and plan files
- HFS tests passed 8/8 against both known Mac discs
- STi2 and native Mac extraction tests passed 9/9, including eight exact output oracles and one catalog scan
- The complete native extraction suite passed 13/13
- Android debug native builds passed for arm64-v8a, armeabi-v7a, and x86_64
- NDK r27d AArch64 `-O2` compilation passed with frames above 32 KiB treated as errors
