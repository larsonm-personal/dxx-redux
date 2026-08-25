# Debug and release behavior parity audit

## Goal

Make debug and release builds run the same game simulation and recovery logic,
while allowing debug builds to add assertions, diagnostics, developer controls,
and observability that do not mutate normal state.

## Plan

- [completed] Inventory `NDEBUG`, `_DEBUG`, and `DEBUG` conditionals in D1,
  D2, and Android native code.
- [completed] Classify each conditional as diagnostics-only, developer UI/tooling,
  performance instrumentation, or behavior/state-changing.
- [completed] Compare mirrored D1/D2 cases and identify changes inherited from the
  original source separately from Android-port regressions.
- [completed] Recommend the smallest parity fixes, prioritizing simulation,
  networking, save state, and error recovery.
- [completed] Record findings and validation requirements before implementation.

## Principle

Debug-only code may inspect, assert, log, or expose explicit developer tools.
Ordinary gameplay, simulation state, recovery behavior, network protocol, save
semantics, and content availability should not depend on build type.

## Findings

- D2 `validate_all_paths()` mutates robot simulation only in debug builds by
  clearing `path_length` after a diagnostic validation failure. The GuideBot
  exception fixes the observed companion regression, but full parity requires
  making this validator non-mutating for every robot.
- D1 and D2 define `MAXIMUM_FPS` as 1000 in debug and 200 in release. This is
  an intentional developer convenience and is outside the parity-fix scope.
- D1 and D2 unlock every level in debug builds. This is an intentional original
  developer shortcut and must be preserved.
- D1 and D2 reject an out-of-range player explosion packet only in release;
  debug builds assert instead. Bounds rejection should be unconditional, with
  an additional debug assertion or diagnostic if useful.
- D1 and D2 IFF writers perform required file seeks inside `Assert()` calls.
  Standard assertions do not evaluate their expression in release builds, so
  release IFF output follows a different and likely incorrect write path.
- Debug-only AI suspension, animation tests, slew mode, render outlines,
  collision inspection keys, demo editing tools, and input-demo logging are
  dormant explicit developer facilities. With their defaults inactive, they
  do not change ordinary gameplay and can remain build-specific.
- The remaining debug conditionals are overwhelmingly assertions, counters,
  labels, logging, editor helpers, and memory checks. No other ordinary
  simulation mutation was found in the direct conditional audit.
- The Android `internal` build explicitly forces native CMake Debug, while the
  public `release` build uses release native compilation. Removing behavioral
  use of `NDEBUG` is therefore necessary even though most current phone tests
  use internal/debug builds.

## Recommended implementation order

1. Make D2 path validation diagnostic-only for all robots and simplify the
   temporary GuideBot-specific retention branch.
2. Move all required IFF seeks outside assertions in both D1 and D2.
3. Preserve and document intentional developer conveniences such as the FPS
   ceiling and level-selection shortcut.
4. Make multiplayer bounds rejection unconditional in both games, then audit
   other network `Assert()` calls as a separate packet-hardening pass.
5. Build and run D1/D2 debug and release variants plus representative input
   demos to verify simulation parity.

## Approved narrow implementation scope

- [completed] Make D2 path validation diagnostic-only for every robot, while
  retaining focused GuideBot logging for phone verification.
- [completed] Move required D1/D2 IFF seeks outside assertions so the operation
  occurs in every build.
- [completed] Make the existing D1/D2 player-explosion packet bounds rejection
  unconditional instead of assertion-only in debug.
- [completed] Keep the D1/D2 polygon-model recursion assertion debug-only so
  Android's always-evaluated assertion macro does not break release builds.
- [completed] Use the build-independent allocator interface in Android level
  metadata setup so D1 release compilation does not reference debug internals.
- [completed] Make Android `Assert` follow standard `NDEBUG` semantics while
  retaining crash breadcrumbs in debug builds.
- [completed] Build and test the affected D1/D2 and Android configurations.
- [deferred] Preserve intentional original developer conveniences, including
  debug level selection, FPS ceiling, slew mode, AI controls, render outlines,
  collision inspection keys, and demo editing tools.

## Validation

- Windows D1 and D2 release builds passed with `run-windows-build.ps1`.
- Windows D1 and D2 debug builds passed with the `x86-debug` preset.
- All 43 D2 host tests passed after the final Windows build. D1 currently
  defines no CTest tests.
- Android `assembleDebug` and `assembleRelease` passed for arm64-v8a,
  armeabi-v7a, and x86_64 with JDK 21.
- Scoped code-quality checks and `git diff --check` passed.
