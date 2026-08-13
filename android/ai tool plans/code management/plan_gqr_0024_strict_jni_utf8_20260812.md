# GQR-0024 strict JNI UTF-8 plan - 2026-08-12

## Objective

Route standard UTF-8 SAF URI and MIDI path/JSON strings through one strict JNI
conversion boundary. Preserve BMP and non-BMP scalar values, reject malformed
UTF-8 deterministically, and avoid Modified UTF-8 APIs for standard UTF-8 data.

## Plan

- [x] Confirm the live ranking, clean worktree, finding, and remediation scope
- [x] Inventory the affected native/Kotlin bridges, existing UTF-8 codec and JNI
  helpers, build ownership, and focused tests
- [x] Implement or extend one shared strict standard-UTF-8 JNI conversion API
- [x] Migrate SAF URI and MIDI path/JSON boundaries without unrelated JNI cleanup
- [x] Add raw/escaped BMP, non-BMP, malformed, exception, and round-trip coverage
- [x] Run focused host/JNI tests, scoped quality, Windows D1/D2, Android ABIs,
  and relevant launcher tests
- [x] Audit warnings and diff scope; mark GQR-0024/GQF-0037 terminal

## Starting state

- HEAD: `d1ad925cbbb76ddc964e9c6db05a947c4cfa5596`
- Worktree: clean
- Ranked impact: 56 (`MEDIUM-HIGH`), rank 9
- Finding: `GQF-0037`, P1/high compatibility/JNI-boundary/encoding
- Non-goal: broader fallible JNI acquisition/allocation work remains GQR-0039

## Result

- SAF URI callbacks and MIDI path, entry-name, state, and JSON strings now use
  `dxx_jni_string_to_utf8` / `dxx_jni_string_from_utf8` exclusively
- Partial MIDI conversion storage is freed and SAF conversion failure returns
  before invoking the Activity
- Raw and escaped BMP/non-BMP SAF URIs normalize to identical standard UTF-8;
  the compiled codec corpus retains malformed, surrogate, overlong, and
  capacity rejection coverage
- Three source contracts, both focused CTests, the launcher unit suite, Windows
  D1/D2, and Android arm64-v8a, armeabi-v7a, and x86_64 builds passed
- All product/test changes are branch-added Android paths; inherited D1/D2
  impact is zero
