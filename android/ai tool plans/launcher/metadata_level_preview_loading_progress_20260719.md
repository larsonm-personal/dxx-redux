# Metadata Level Preview Loading Progress

## Goal

Replace the black interval while a metadata level preview initializes with the existing Android loading-progress overlay, while preserving the texture loader's throttling and normal gameplay behavior.

## Phase 1: Trace and design

- [x] Trace metadata preview startup from Kotlin through native level load and automap initialization
- [x] Document the existing texture loading overlay lifecycle and update throttling
- [x] Identify stable preview-loading phases and measurable work units

## Phase 2: Implement

- [x] Reuse the shared Android loading-progress bridge during preview initialization
- [x] Report useful preview phases without excessive UI updates
- [x] Ensure success, failure, and early-return paths always hide the overlay
- [x] Preserve D1/D2 consistency and ordinary level-start texture progress

## Phase 3: Verify

- [x] Add or extend high-level coverage for preview loading progress
- [x] Run scoped code quality for touched files
- [x] Run relevant unit and integration tests
- [x] Run required D1/D2 CMake builds and tests
- [x] Record results and any device-only validation

## Results

- The black interval occurs while the preview process initializes game data, loads the level, performs the canonical metadata scan, replans from the selected player start, and renders the first automap frame. Preview startup did not previously own a loading overlay even though the shared native bridge was already used by gameplay texture caching.
- `LevelPreviewActivity` now places the existing `LoadingProgressOverlayView` above the surface and touch overlay, displays it immediately, and exposes the same JNI show/hide methods as `MainActivity`.
- Native preview startup reports monotonic phases from request parsing through mission and level loading, map topology, secret scanning, objective visibility, and automap opening. Canonical and live-route metadata callbacks receive separate progress ranges.
- Added absolute progress updates to the existing loading bridge. They use the same monotonic-clock throttle as texture loading, allowing at most one ordinary JNI/UI flush per 300 ms. Forced initial and final updates remain immediate.
- A scoped guard completes and hides the overlay on success and every early-return or failure path. Successful completion occurs only after the first automap event frame has been processed.
- Preview introspection records completion, maximum percent, metadata callback count, and actual throttled UI flush count. The maintained preview integration test now rejects missing, incomplete, or over-frequent loading updates.
- The final focused Uneasy4 emulator run passed. Its first frame took 6,829 ms, 738 native metadata callbacks were reduced to 22 UI updates, progress reached 100%, camera input remained functional, and the preview closed and cleaned up normally.
- Scoped code quality passed. The full Android unit-test suite and debug APK build passed for arm64-v8a, armeabi-v7a, and x86_64. Both D1/D2 Windows builds and their route snapshot and level metadata scan tests passed.
