# Input-demo direct-command policy and fixture compatibility audit

## Goal

Minimize the inherited D1/D2 direct-command replay diff by moving common event
iteration, validation, failure handling, and shared commands behind one explicit
boundary. Keep game-specific command behavior in compact adapters, then audit
the input-demo corpus and refresh only fixtures invalidated by the fully
validated implementation.

Input-demo compatibility may break in this tranche. Do not add compatibility
branches solely to preserve obsolete regression recordings.

## Existing work to preserve

- Preserve current deterministic simulation and final-state comparison behavior
- Preserve engine-owned command effects rather than simulating them in the demo layer
- Preserve D1/D2 policy differences only where they are intentional and tested
- Preserve unrelated route-planner, Guide-Bot, mission metadata, and secret-area work
- Keep Windows D1/D2 and all configured Android ABIs supported

## Phases

### 1. Live audit and boundary design

- [x] Read the prior input-demo cleanup, parity, command, and diff-minimization docs
- [x] Inventory current D1/D2 recording, parsing, iteration, and application paths
- [x] Inventory direct-command names and classify common versus game-specific policy
- [x] Record target-file diff metrics against `upstream/main`
- [x] Define a shared API that is smaller than the removed inherited bodies

### 2. Focused fixtures before movement

- [x] Cover ordered iteration of zero, one, and multiple commands in one frame
- [x] Cover malformed and unknown command failure without partial later application
- [x] Cover common death-abort and live-difficulty commands in D1 and D2
- [x] Cover D2-only Guide-Bot, marker, weapon-drop, and flag policy through an adapter
- [x] Capture intentional D1/D2 failure and replay-unload behavior

### 3. Extraction and cleanup

- [x] Move common iteration, validation, dispatch result, and diagnostics to shared code
- [x] Add compact explicit D1 and D2 policy adapters
- [x] Remove superseded duplicated command loops and wrappers
- [x] Keep command effects in canonical engine functions
- [x] Review the schema deliberately; retain version 4 because the clean boundary does not change the wire shape

### 4. Deterministic validation

- [x] Run focused direct-command writer/parser/dispatcher fixtures
- [x] Record and replay representative D1 and D2 command sequences
- [x] Verify exact final state, command order, result, state trace, and RNG trace
- [x] Build Windows D1 and D2
- [x] Build all configured Android ABIs
- [x] Run scoped code quality and `git diff --check`

### 5. One-time demo compatibility audit

- [x] Inventory every local regression demo affected by the new command policy or format
- [x] Determine whether the final format or behavior invalidates any D1 or D2 fixture
- [x] Re-record and regenerate normalized state and RNG traces only for invalidated fixtures
- [x] Run the complete D1 and D2 input-demo regression matrices
- [x] Confirm no old compatibility shim remains solely for replaced fixtures

### 6. Campaign closeout

- [x] Record exact inherited-file reduction and remaining private adapter sizes
- [x] Update the high-coupling campaign and candidate catalog
- [x] Run the final combined build and focused regression matrix for H01-H06

## Validation evidence

- The shared compiled policy materializes and parses the complete current-frame
  direct-command batch, preflights every command before mutation, and then
  applies commands in their exact direct-command order. Interleaved non-command
  events are ignored.
- The dead phase applies only `death_abort`; the gameplay phase ignores
  `death_abort` and applies common difficulty and game-specific commands. D1
  rejects D2-only commands. Failed replay policy remains intentional: D1 logs
  and returns failure without unloading, while D2 unloads the failed replay.
- Synthetic recorder and replay fixtures cover no replay, zero commands, one
  difficulty command, all 12 commands, interleaved events, exact payload and
  order, dead/gameplay filtering, adapter rejection, malformed and unknown late
  events without partial mutation, D1 unsupported commands, and both unload
  policies. Truncation coverage proves future and pending direct commands are
  removed when the recorder timeline is replaced.
- The real command-bearing D1 and D2 recordings replay with exact final-state
  and RNG comparison. Logs: `temp/h06_d1_death_abort_replay.out.txt` and
  `temp/h06_d2_death_abort_replay_post_audit.out.txt`.
- The primary matrix at `temp/h06_input_demo_matrix_20260712_150718` passes all
  4 D1, 11 D2, and 4 D1-in-D2 cases. The filtered suite report at
  `temp/h06_input_demo_run_all_final_20260712_153835/report_20260712_153836.md`
  passes the exact final tree: 10 of 10 tests, 22 corpus replays, three render
  variants, runtime smoke, and the result, state, and RNG comparers.
- Windows D1/D2 and the four focused replay/recorder executables pass; output is
  `temp/h06_windows_both_final.out.txt`, with the post-audit D2 rebuild and
  focused rerun in `temp/h06_windows_d2_post_audit_final.out.txt`. The final D1
  and D2 runtime smoke is in
  `temp/h06_input_demo_runtime_smoke_post_audit.out.txt`. Android arm64-v8a,
  armeabi-v7a, and x86_64 build without compiler warnings; output is
  `android/temp/h06_android_assemble_debug_final.out.txt`. Scoped code quality
  and `git diff --check` pass.
- Against `upstream/main`, D1 `gamecntl.c` moved from `+185/-10` to
  `+122/-10`; D2 moved from `+458/-14` to `+265/-17`. This removes 256
  inherited additions and 253 total inherited changed lines.
- The remaining D1 engine-local policy spans are a 10-line death callback, a
  17-line difficulty callback, and a 19-line invocation wrapper, with no
  game-specific adapter. D2 retains a 10-line death callback, a 17-line
  difficulty callback, a 48-line game-specific adapter, and a 19-line
  invocation wrapper. Each
  inherited `gamecntl.c` retains only dead and gameplay replay phase calls while
  live command recording stays at the canonical action sites.
- The wire schema remains version 4 and no compatibility shim was added. The
  ignored local corpus contains 15 `.dximdemo`/classic-demo/RNG-trace triples;
  only one D1 and one D2 recording contain a direct command, both
  `death_abort`. Both pass the final implementation, so no re-recording or trace
  regeneration is required. Only the corpus `.gitignore` and `README.md` are
  tracked.
