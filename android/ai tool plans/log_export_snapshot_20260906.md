# Current-run log export failure

- [x] Trace the previous URI grant fix and the current pre-chooser copy failure
- [x] Reproduce append-during-export and publish a bounded snapshot from one open file
- [x] Cover growth, empty logs, truncation, and immutable grants; build and run scoped quality checks

The launcher and game append to the same debug log. Strict copy validation reads
to EOF and rejects growth as an incomplete copy; publication also samples length
separately. Export must capture length once on the opened file and copy exactly
that prefix. Keep strict copies for imports and non-log exports

The regression initially failed with `Incomplete copy of growing-debuglog.txt:
expected 180000 bytes, received 180023`. The initial progress callback appends
23 bytes to the real source file, reproducing the existing share copy path
without relying on scheduler timing

Debug-log Share and Open now flush the launcher writer, open the source once,
capture its descriptor length, and publish exactly that prefix. This does not
require stopping the game or launcher writers. Tests also exercise an empty
snapshot, rejection and cleanup after truncation, and later appends leaving the
published grant unchanged. Existing strict copy and grant retention tests remain
in the focused validation run

Validation passed: 15 tests across FileProviderGrantStoreTest and
LauncherFileCopyTest, `:app:assembleDebug -Pandroid.injected.build.abi=x86_64`
including the native CMake build, scoped Kotlin formatting, and `git diff --check`

Physical-device confirmation of the user's exact post-engine-exit sequence is
still pending; no matching exception was present in the inspected emulator logcat
