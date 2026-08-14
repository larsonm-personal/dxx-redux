# GQR-0039 JNI exception-safety remediation plan

## Scope

- [x] Reconfirm `GQF-0052` evidence and current strict UTF-8 helper boundaries
- [x] Inventory the assigned fallible JNI acquisitions, allocations, lookups, stores, and callbacks
- [x] Centralize checked JNI construction and acquisition where it reduces repeated branch-added code
- [x] Stop each assigned path at its first pending exception and release only resources actually acquired
- [x] Add focused source contracts for partial acquisition and allocation failure
- [x] Run scoped formatting and tests, then compile the Android native targets for every configured ABI
- [x] Record exact changed paths, diff metrics, validation, and any remaining device-only limitation

## Constraints

- Preserve the strict UTF-16/UTF-8 conversion behavior delivered for `GQF-0037`
- Do not clear or replace a Java-origin pending exception
- Do not edit inherited `d1` or `d2` sources
- Do not edit the canonical quality ledger or the parent campaign plan
- Preserve unrelated concurrent worktree changes

## Evidence-owned surfaces

- `jni_midi_preview.c`: byte/string acquisition and MIDI byte-array publication
- `jni_gog_import.c`: list arrays, callback lookup/invocation, and array stores
- `jni_cd_preview.c` and `jni_fingerprint.c`: partial string/array acquisition and result construction
- `jni_saf.c` and `jni_udp_reconnect.c`: checked object/class/method/array construction
- `jni_disc_import.c`: listing arrays and progress callback initialization
- `jni_main.c`: startup, completion, and fatal JNI sequences

## Completion record

- Status: complete
- Product paths: `jni_midi_preview.c`, `jni_cd_preview.c`, `jni_fingerprint.c`,
  `jni_saf.c`, `jni_main.c`, `extract/jni_gog_import.c`,
  `extract/jni_disc_import.c`, and `shared/net/net_udp_reconnect_jni.c`
- Test path: `android/tests/test_jni_exception_safety_contracts.py`
- Diff metrics: 342 insertions and 228 deletions across eight product files,
  plus one 75-line focused source-contract suite
- Validation:
  - Nine focused JNI exception-safety and strict UTF-8 contracts pass
  - Scoped clang-format and UTF-8 BOM quality checks pass
  - `git diff --check` passes for all product and test paths
  - `:app:externalNativeBuildDebug --no-parallel --no-daemon` passes D1 and
    D2 for arm64-v8a, armeabi-v7a, and x86_64
- Remaining limitations: the repository has no maintained device-side JNI
  fault injector, so allocation, lookup, callback, and pending-exception failure
  ordering is enforced by source contracts and real all-ABI compilation rather
  than a CheckJNI forced-OOM runtime. The existing strict codec tests retain
  ASCII, BMP, supplementary scalar, malformed UTF-8/UTF-16, and embedded-null
  coverage
