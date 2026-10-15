# D1 failed-level guard

Base: verified origin/cmake 1b26c43cf. Minimal production change: report load_level failure before continuing level setup, preserving D2's fatal error contract and D1's successful-load ordering.

Validation: a Linux GNU-linker harness calls the real LoadLevel function with injected error 3 and success. Only the loader and observable exit/setup boundaries are wrapped. Build the CMake test and require the original source to fail and patched source to pass. Retain the earlier native-menu representative damaged-shareware evidence separately. No new binary mutation campaign, rollback redesign or gameplay changes.

## Results

- Current upstream cmake was verified by git ls-remote as 1b26c43cf928f9798513c0652a833c28f50a9e98 before branching
- Fresh CMake Debug configure/build succeeded; native d1x-redux and focused test executable built
- CTest test_d1_level_load_failure passed against patched source
- Rebuilt the same test with exact unmodified HEAD d1/main/gameseq.c: failed as required, reporting `Failed level continued into setup: boundary=2 current=0 rescan=0 diagnostic=`
- Restored the patched file, rebuilt native D1 and test, and reran CTest: passed
- New C test passed scoped clang-format dry-run and git diff --check passed. PowerShell wrapper intentionally not run under task constraints. Existing unrelated packed-member and strncat warnings remain in baseline source; new test and changed gameseq compilation add no warnings
- No full CTest aggregate, Android device, MSVC or macOS runtime claim

Production diff: retain load_level result and report the same error text/number as D2 before further setup. Successful branch is unchanged. The 87-line test calls actual LoadLevel, injecting only load_level return 3 or 0 and observing fatal/error and first post-load boundaries using GNU linker wrapping. Valid success must commit level 1 and rescan secret areas once before palette setup; failure must call Error with exact message without committing the level, rescanning or reaching palette setup. No production test hooks or alternate loader implementation.

The CMake target uses the existing headless target helper, with UNIX/non-Apple GNU-or-Clang guard for --wrap support. It is intentionally a focused caller-contract regression, not a full parser, rendering or gameplay test. The earlier real-menu observation used frozen native probes and public shareware with a two-byte game-data signature alteration: D1 exited -6 at songs_play_level_song's levelnum assertion after load_level returned 3; D2 already issued the explicit fatal loader diagnostic. Current upstream reproduction here independently proves the missing D1 caller guard with no damaged binary fixture.

Build directory: /tmp/dxx-d1-failed-load-build
Logs: /tmp/dxx-d1-failed-load-{configure,build,rebuild,oldbuild,negative,finalbuild,final}.log
Command after configure: cmake --build /tmp/dxx-d1-failed-load-build --target test_d1_level_load_failure dxx-redux-d1 -j4
Test: ctest --test-dir /tmp/dxx-d1-failed-load-build -R '^test_d1_level_load_failure$' --output-on-failure

Next configuration-fuzz target: change sound/video settings through normal menus, reject a mission with an unsupported critical descriptor directive, dismiss the error, and start valid content; verify persistent settings and actual output. Do not expand malformed-binary mutations.
