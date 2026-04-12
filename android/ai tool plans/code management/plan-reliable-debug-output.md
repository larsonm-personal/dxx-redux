# Plan: Reliable Debug Output and Console Buffer for Integration Tests

## Problem

The test infrastructure relies entirely on logcat for SCRIPT_RESULT and step-by-step
progress. Logcat is lossy: buffer overflow, dropped lines, interleaving, and emulator
restarts all cause AI tool sessions to time out with zero diagnostic information.

## Solution: Three Layers

### Phase 1: File-Based Automation Results

Currently `stop_script_fail()` and `advance_step()` in game_automate.cpp only call
`__android_log_print` (logcat). If logcat drops the line, the test runner times out.

1. **Write `automation_result.json` on PASS/FAIL** -- add `write_result_file()` that
   writes to `g_automate_dir/automation_result.json`. JSON:
   `{"result":"PASS|FAIL","steps_completed":N,"total_steps":N,"reason":"...","elapsed_ms":N}`
   Called from `stop_script_fail()` and at script completion in `advance_step()`.

2. **Write `automation_log.jsonl` step-by-step** -- one JSON line per event (step
   start, assertion result, timeout). Fields: `seq`, `step`, `total`, `action`,
   `status`, `elapsed_ms`, `detail`. Opened/truncated at script load, fflush after
   every write. This is the durable record of what happened.

3. **Delete stale result/log files at script load** -- unlink both files at the top
   of the load path so prior run results cannot confuse the test runner.

Files: game_automate.cpp only. All behind `#ifdef INTROSPECT_ON` (already present).

### Phase 2: Test Runner Hardening

4. **Primary result source: file, not logcat** -- in `Watch-AutomationResult`
   (test_helpers.ps1), every poll iteration: `adb shell run-as <pkg> cat
   files/automation_result.json`. If it exists and parses, use it as authoritative.
   Fall back to logcat only if file not found.

5. **Dump diagnostics on timeout/failure** -- on failure, cat
   `automation_log.jsonl` and debug log files, print last 30 lines of each.

6. **Delete stale result file before broadcast** -- in `Start-AutomationScript`,
   `rm -f files/automation_result.json` before sending the AUTOMATE broadcast.

Files: test_helpers.ps1 only.

### Phase 3: Console Ring Buffer via Introspection

`con_printf` is the engine's workhorse logger. On Android, its `printf()` goes to
invisible stdout; on Android, con_printf output is routed to the debug log
system (DLOG_GAME category) instead of writing a log file. A ring buffer
makes this output queryable through the existing introspection system.

7. **Create `console_ringbuf.cpp` / `.h`** in android/app/src/main/cpp/shared/.
   512 lines x 256 chars ring buffer with monotonic sequence counter. API:
   - `console_ringbuf_add(const char *line)` -- add line to ring buffer
   - `console_ringbuf_get_json(uint64_t since_seq)` -- return JSON with lines since seq
   Thread-safe via mutex (game thread writes, JNI thread reads).
   Entire file guarded by `#ifdef INTROSPECT_ON`.

8. **Hook `con_printf` in d1 and d2** -- in d2/main/console.c and d1/main/console.c,
   after the existing `printf(buffer)` call, add `#ifdef INTROSPECT_ON` block (~3
   lines) calling `console_ringbuf_add(buffer)`. This is the ONLY d1/d2 change.

9. **Add `console` section to introspect.json** -- in `game_introspect_get_state()`,
   add a `console` key containing the last ~50 lines from the ring buffer.

10. **Add JNI method `nativeGetConsoleSince(long seq)`** -- returns console ring
    buffer JSON directly without a full introspection dump. Added in jni_main.c and
    declared in MainActivity.kt. Behind `#ifdef INTROSPECT_ON`.

11. **Add `introspect.sh console` subcommand** -- reads the console section from
    introspect.json.

Files: console_ringbuf.cpp (new), console_ringbuf.h (new), game_introspect.cpp,
jni_main.c, MainActivity.kt, CMakeLists.txt, introspect.sh, d1/main/console.c,
d2/main/console.c.

### Debug-Only Guards

All new C/C++ code is guarded by `#ifdef INTROSPECT_ON`, the same define used by
the existing introspection and automation systems. This define is:
- Set in android/app/src/main/cpp/CMakeLists.txt for debug Android builds
- Not set for Windows/Linux/Mac desktop builds (those use d1/CMakeLists.txt and
  d2/CMakeLists.txt which do not define INTROSPECT_ON)
- Not set for release Android builds (when that build variant is configured)
- The d1/d2 console.c hooks are the only engine changes; they compile to nothing
  without INTROSPECT_ON

The Kotlin layer additionally gates adb broadcast access behind `BuildConfig.DEBUG`.

Double-check: the ring buffer, file writes, and JNI methods all live inside
`#ifdef INTROSPECT_ON` blocks. The 3-line console.c hook is also inside
`#ifdef INTROSPECT_ON`. Zero performance impact in release builds.

## How Game Scripts Leverage the New Functionality

### Automation result file (replaces logcat as primary result channel)

The test runner automatically uses `automation_result.json` as the primary pass/fail
source. No script changes needed -- every existing and future test benefits. When
logcat drops the SCRIPT_RESULT line, the test still passes/fails correctly via the
file.

### Step-by-step log (automation_log.jsonl)

On test failure or timeout, the runner automatically dumps the last 30 lines of
`automation_log.jsonl`. This shows exactly which step was executing and what it was
waiting for. Example JSONL line:
  `{"seq":5,"step":3,"total":12,"action":"wait_for","status":"timeout","elapsed_ms":20000,"detail":"screen_mode = game"}`

For manual debugging, cat directly:
  `adb shell run-as com.dxxredux.app cat files/automation_log.jsonl`

### Console ring buffer (via introspect.json or direct JNI query)

Every introspection dump now includes the last 50 con_printf lines in a `console`
section. This means:
- AI tools that call `introspect.sh` see recent engine output alongside game state
- The `introspect` automation action captures console output at that moment
- After a test, `introspect.sh console` shows just the console lines

For polling during test execution:
  `adb shell run-as com.dxxredux.app cat files/introspect.json | python3 -c "import json,sys; d=json.load(sys.stdin); [print(l['text']) for l in d.get('console',{}).get('lines',[])]"`

For direct query without introspection dump (if JNI method is called from Kotlin):
  Would be exposed through a broadcast or direct native call.

### Diagnostic dump pattern (for test scripts)

Scripts can add an `introspect` step before risky operations to capture engine state
including console output:
```json5
// Before a tricky operation, snapshot state including console
{"action": "introspect"},
{"action": "wait_for", "field": "in_game", "value": "true", "timeout_ms": 30000},
```

After a test failure, the runner's diagnostic dump shows:
1. Last 30 lines of automation_log.jsonl (what the script was doing)
2. Last 30 lines of debug log files (what the engine was doing)
3. These are files, not logcat, so they survive buffer overflow and emulator issues.

## Verification

- Build with `gradlew clean assembleDebug`, install on emulator
- Run `test_launch_to_automap.json5` for d2:
  - Verify `automation_result.json` exists with PASS
  - Verify `automation_log.jsonl` has step entries
  - Verify introspect.json has a `console` section
  - Verify test runner reads result from file
- Clear logcat mid-test to verify file-based result still works
- Run `run-code-quality.ps1 --fix` on modified files
