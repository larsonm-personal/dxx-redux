# GuideBot worker heap-corruption diagnosis and hardening

## Evidence

- Batch `20260906_125826` processed all 1,794 selected levels. Its sole infrastructure failure was `dontpnic.json|0|9|level09.rdl` (D1-in-D2). The stage deliberately returned 1 after saving the batch results. Ordinary simulation timeouts were not the reason for this stage failure.
- Windows Application event 1000 at 13:05:32 identifies `dxx-redux-d2-headless-route.exe`, exception `0xc0000374` (heap corruption), detected in `ntdll.dll`. Decimal process exit was `-1073740940`. Detection in the allocator does not identify the original corrupting write.
- The worker log is empty. The WER report remains, but its referenced temporary dump has disappeared. There is insufficient evidence to locate the crash within engine startup, navigation, or teardown.
- Two isolated serial runs using the currently available executable, seed 1, and no regression-file writes both confirmed the route in 1,197 frames. Their raw result JSON hashes match exactly. Artifacts: `android/temp/dontpnic9_crash_check_20260906`. This does not prove that the crashing executable and the reproduction executable are identical, nor that the intermittent fault is fixed.
- Earlier batch failures included a fixed path-expansion scratch-buffer overflow (see `2026-09-05-simulation-batch-failures.md`). The present failure is the same broad native-memory-safety category, but is not established to have the same cause. The previously recorded dontpnic secret-level weapon-order failure is a different level and signature.
- Cleanup ran after the failure report; there is no evidence it caused this worker crash. Nor does a passing serial rerun establish that parallel execution caused it.

## Phase 1: Make each failure independently reproducible

Before launching each worker, write a scratch-only manifest containing its identity, effective working directory, exact executable and argument list, seed, speed, time limits, input hashes, executable hash/build identity, and matching symbol location. Record process ID and timestamps after launch. Hash shared inputs/binaries once per batch, referenced by each manifest.

At retirement, append exit code in signed decimal and hexadecimal, timeout/start-error distinctions, and artifact paths. Print a final concise failure digest, not just the summary directory and generic stage failure. Keep paths and verbose diagnostics out of checked-in `.simulation.json` files.

Acceptance: a deliberately crashing test worker produces a complete manifest and a useful final summary; other workers still retire and publish results normally. A controlled routing timeout must remain distinct from a native crash.

## Phase 2: Preserve evidence at the crash boundary

Add low-volume native phase breadcrumbs with explicit flushes: process entry, data mounting, mission/level load, route planning, simulation start, objective transitions, and teardown. Include frame and RNG state where initialized. Avoid logging every frame. The process pool currently writes captured stdout/stderr to the item log on completion; inspect and improve incremental persistence without blocking pipe drainage or interleaving worker output.

Audit the existing Windows `crashdump.c` helper and whether each headless target installs it. Its second-resolution filename in the working directory is unsuitable for concurrent workers. Any retained helper needs an explicit per-worker destination, unique identity/PID, and error reporting. Do not rely solely on an in-process exception handler for heap corruption: fail-fast exceptions or corrupted allocator state can prevent reliable capture.

Provide an opt-in external debugger/dump-capture mode attached to the specific worker process, with dumps and matching binaries/symbols retained under the failed item. Keep it headless and bound to parent lifetime. Do not silently enable machine-wide WER registry settings. Retention must preserve a bounded set of failure bundles separately from routine successful scratch runs.

Acceptance: an intentional native heap-corruption fixture yields a retained dump or an explicit capture failure, plus its manifest. Concurrent failures cannot overwrite each other's artifacts. Closing the parent must still terminate workers and diagnostic helpers.

## Phase 3: Locate the first invalid write

Reproduce dontpnic L9 repeatedly in serial, then with a small bounded parallel workload, using one recorded executable and input set. Preserve the original failure independently of retries. Successful retries are diagnostic observations, never replacements that make the original batch green.

Verify toolchain support for an instrumented Windows build. Prefer AddressSanitizer or an equivalent memory diagnostic that catches the offending write earlier than heap teardown. If full page heap/Application Verifier is needed, use a separately named diagnostic executable, document the per-image settings, and restore them on cleanup; obtain user direction before persistent system configuration changes. Compare instrumented and normal builds without claiming they have identical allocator behavior.

Use the first-write stack and allocation history to choose an engine fix. Revisit the earlier expanded-path buffer only if evidence points there; do not blanket-disable parallelism, lengthen timeouts, or downgrade the crash into a routing failure.

## Phase 4: Validate the engine fix and reporting

Add a focused regression for the corrupting operation once identified. Repeat the failing mission case with fixed seed and compare successful outputs. Run the four primary mission targets (original D1-in-D2, Counterstrike, Castaway Redux, Obsidian), then a full corpus pass. Check both native-crash counts and lost previously successful routes. Keep determinism checks targeted rather than doubling every routine corpus run.

## Investigation and implementation results

### Confirmed defects, versus attribution of the original crash

The old D1 and D2 `ReadConfigFile()` allocate exactly `PHYSFS_fileLength()` bytes, then call the unbounded `PHYSFSX_gets()`. That helper does not ensure termination before calling `strlen()`. A valid config containing just `DigiVolume=8` without a final newline reproducibly produces an ASan heap-buffer-overflow in `ReadConfigFile`, at the call to `PHYSFSX_gets`. Evidence: `android/temp/config_bounds_asan_before/stderr.log`.

Headless workers were also sharing the executable directory as their PhysFS write/search directory. `StartNewLevel` calls `set_highest_level`, which reads the pilot and calls `write_player_file`; the latter also calls `WriteConfigFile`. Thus different worker processes both read and truncate/rewrite `descent.cfg` and `RouteBot.plr`. A fixed simulation seed does not make this external file race deterministic.

A concurrent config-rewrite fixture reproduced another heap-buffer-overflow with the old executable on attempt 3: `android/temp/config_multiline_race_before_3/stderr.log`. Its allocation was only one byte. This is direct evidence of unsafe concurrent startup config reading, although the sanitizer reported an out-of-bounds read rather than the original corrupting write. **The original dontpnic heap-corruption event still cannot be conclusively attributed to this defect without its dump.** Do not present either a read overflow or a successful rerun as proof of that attribution.

The crashing image PE timestamp was `0x6a9dc1d7` (12:41:11); the subsequent normal executable was already `0x6a9dc9ab` (13:14:35). Historical byte identity cannot be recovered from the old batch artifacts.

### Changes

- Both engines now reserve terminator space and use capacity-bounded config reads, rejecting invalid lengths and handling empty/malformed tokens safely
- The headless regression runner gives every individual run a fresh private PhysFS user directory; the native entry point removes the shared writable directory from the search path before config/player loading
- Isolated workers skip the interactive executable's `d2x.ini`; normal interactive argument loading is unchanged
- Fresh worker directories are required rather than silently reusing pilot settings from an earlier attempt
- Startup/route/result/shutdown phase markers go to unbuffered stderr
- Batch runs save launch manifests (including engine and route-input hashes), separate process outcomes, an engine/DLL/available-PDB snapshot, and a final infrastructure-failure digest; this diagnostic material stays out of canonical simulation JSON
- Added a bounded repeated-process stress helper and a native integration fixture for unterminated configs, concurrent rewrites, and private-directory persistence

### Validation so far

- Both Windows D1 and D2 builds completed; D2 argument-default test passed
- Old image: 224 ASan dontpnic L9 runs and 100 current-normal-image runs under external ProcDump monitoring did not reproduce the original crash
- Fixed image: unterminated config test passes under ASan; ten concurrent multiline rewrite tests pass; private-directory test confirms the shared config is unchanged and the pilot is written privately
- All 88 primary levels passed the infrastructure checks under ASan and twice under the normal build. Repeats are identical, the native and ASan canonical outputs are identical, and all four files' level results are unchanged from checked-in data, including timings and RNG fields
- Snapshot-based dontpnic L9 rerun completed twice with identical outputs
- Deliberate exit-123 worker fixture verifies failure records survive and the stress helper exits nonzero
- Full isolated ASan corpus pass completed all 1,794 items under `android/temp/corpus_isolated_asan_after`, in about 17 minutes. dontpnic L9 confirmed in 1,197 frames. There were exactly two infrastructure failures, both precise sanitizer findings described below, and no recurrence of the Windows heap-corruption signature
- Final normal-build smoke test confirms the finer startup phase markers and snapshot-based execution under `android/temp/dontpnic_final_diagnostics_after`

### Separate findings from the full sanitizer pass

These are not established causes of the original dontpnic event and are not fixed in this change:

1. `CD - Descent - Levels of the World (USA).json|27|0|comet.rdl`: global out-of-bounds read at `d2/main/gameseg.c:2213`, `set_ambient_sound_flags_common`. The loader warns about unknown D1 texture 14924, then indexes `TmapInfo` with a texture index without a range check. Follow up by validating decoded texture references at the loading boundary, preserving explicit diagnostics for invalid input rather than treating arbitrary memory as texture flags.
2. `kcxf2.json|0|4|kcxf204.rl2`: global out-of-bounds read at `d2/main/gamesave.c:1055`, in the wall-animation lookup during `load_game_data`. Follow up by checking both wall references and clip indices before indexing `Walls` and `WallAnims`, and determining whether this asset needs an unsupported format feature or has invalid data.

Both logs and process manifests are retained in the full corpus run directory. The sanitizer batch correctly returns failure for these errors; they were not rewritten as routing timeouts or suppressed.

No canonical regression JSON was regenerated by this investigation. ProcDump was used only for the selected processes; no global crash-debugger registration or page-heap registry configuration was enabled. Automatic default dump capture and the remaining broader diagnostic enhancements are not yet implemented.
