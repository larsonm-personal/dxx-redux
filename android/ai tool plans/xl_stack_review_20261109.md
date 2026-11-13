# D2X-XL stack integration review

Review the live GitHub stack and its exact tip without altering existing branches or stashes

1. Inventory open PR heads and bases, stack ancestry, aggregate changes and tip CI failures
2. Create an isolated detached checkout of the exact tip and read its current instructions
3. Reproduce relevant failures with Windows builds and focused contract/integration tests
4. Inspect gameplay boundaries, cross-platform coverage and test/CI complexity; propose or locally verify bounded repairs
5. Record evidence, prioritized findings, simplification/integration sequence and remaining validation limits

Do not commit, push, merge or post externally under this review request
Historical overnight prompts are context, not renewed execution instructions

## Review result

Reviewed on 2026-11-09 against PR #461, exact head `bf388fc1064ea3ce70bfe88daceff3348d359608`

The stack contains useful compatibility work and unusually explicit reference-source comparisons, but I would not integrate it wholesale yet. The native asset lane is genuinely failing, several new tests do not establish valid engine state, Windows developer builds need repairs, and the slow-motion/render-clock work has an explicit replay capability gap. Stabilize the linked native harness, then consolidate the stack into independently reviewable groups

The review covered PR inventory and ancestry, aggregate production changes and test structure, tip CI, fresh Windows D1/D2 builds, and focused reproduction of all five reported private native failures. It is not an exhaustive line-by-line audit of 54,000 added lines or a campaign playthrough

### Frozen scope and evidence

- 59 open PRs at inventory time: 58 in the logical stack and independent tooling PR #454
- Tip branch: `fix/xl-separating-robot-bumps-20261109`
- Compared with merge base `258d9a1ee6e81f5830b35598b37844ab44fea6b6`; tip is 80 commits ahead
- Inventory-time `origin/cmake`: `49e92bbcdf92b31f77c705f65587d3dfe81f299c`, including merged readiness-baseline PR #456
- Aggregate: 254 files, 54,223 additions, 195 deletions
- Approximate path classification: production 49 files / 2,392 additions; tests 146 / 49,277; documentation 53 / 2,213; CI 6 / 341
- PR #449 and #451 latest heads are not literal ancestors of the tip, but their fixes were repeated/expanded in #450 and #452. Inspection found semantic propagation, not evidence of missing fixes
- Complete PR metadata and review logs are under `temp/xl-stack-review-20261109/`; `pr-inventory.json` records exact heads, bases and descriptions
- Detached review worktree: `temp/xl-stack-review-20261109/source`; original branch and three existing stashes preserved; no commits, pushes, merges or external comments

## Findings and repair routes

### 1. Native CI is red; three failures share invalid synthetic multiplayer setup

Private run: https://github.com/larsonm-personal/dxx-redux/actions/runs/42684951549

The host job compiled successfully, entered native tests, and reported five failures. I reproduced the same five failing test names with the tip's test semantics on Windows, after the build-only portability repairs below and supplying the original hash-checked assets

| CI test | Local evidence | Repair/status |
| --- | --- | --- |
| `xl_countdown_runtime` | Post-zero displayed seconds expected -1 where pinned XL integer division returns 0 | Corrected local expectation; full native test passes |
| `xl_modern_properties` | `Protocol handling missing in multi_send_data` | Unresolved fixture setup failure |
| `xl_modern_water` | Same protocol error | Same repair route |
| `xl_observatory_descriptor` | Same protocol error after supplying required D1 HOG | Same repair route |
| `xl_projectiles` | Incorrect XL hit-history expectation under multiplayer; after correction, ordinary Gauss fixture fails | First assertion corrected locally; full test still fails |

`android/XL_compat/runtime/no_damage_fusion_test.h:20` sets `Game_mode` to `GM_NETWORK` without establishing `multi_protocol`. Its weapon-selection controls reach production `select_weapon`/sound replication (`d2/main/weapon.c`), then the deliberate fatal default in `d2/main/multi.c:1654`. These tests do not yet demonstrate a gameplay regression in the feature named by the test

Repair the fixture boundary once: provide a scoped, explicit recording network transport for tests, assert the expected sends, and restore global mode/transport/player state. Alternatively use a disposable properly initialized network environment. Audit the other new synthetic multiplayer cases for the same assumption. Do not weaken production protocol validation, replace network mode with obsolete serial flags, or blindly set UDP while leaving its queues and peers uninitialized

CI's sanitized report retains test names but no assertion or bounded error fingerprint (`coverage` is empty). Add allowlisted failure IDs and numeric context at the native harness boundary, preserving the private-asset publication rules. This would have made the first diagnosis possible without rebuilding locally

### 2. Countdown and projectile assertions diverge from effective production semantics

Countdown: #457 correctly changed fixed-point conversion from shifting to division. At -1.25 seconds, adding the display offset 0.875 leaves -0.375, which integer division truncates to 0. `runtime/countdown_controls.h` still expected -1. I corrected that assertion and added a comment. The complete native test passes, including save/restore and malformed-state controls. The independently compiled pinned-reference suite also passes 370,998 countdown comparisons and 180 terminal episodes against reference revision `2576a98b5de310e3ac7c9777df2677d1627b8bae`

Projectile: profile 4 switches to multiplayer, which makes `xl_level_extensions_active()` false. `runtime/projectile_test.h` nevertheless selected the expected hit-history representation using the outer `xl` argument. The engine correctly used classic history. I changed the expectation to use `xl && profile != 4`; the hostile-contact matrix then passes

The next assertion fails in `projectile_ordinary_gauss_spin`, so the projectile test is NOT repaired end to end. With XL active, multiplayer=0, frame=512, skip=1, seed=17, the fixture observes no spin and zero random draws instead of the expected spin. A second diagnostic run confirmed that the reused victim already has `OF_EXPLODING` and -25 shields; the colocated player also has -25 shields. Earlier iterations apply Gauss blast damage despite the shot's `OF_HARMLESS` direct-damage flag; the loaded weapon has a 10-unit blast radius. Production correctly excludes the exploding target from the torque path. Isolate/reset complete case state and distinguish direct-damage suppression from blast behavior before changing engine torque arithmetic. Diagnostic logs are in `native-gauss-state/projectile/controls.log`; temporary diagnostic code was removed from the proposed patch

### 3. Windows developer targets expose build gaps

Fresh x86 MSVC builds found stack-introduced portability problems that public CI did not catch:

- `android/app/src/main/cpp/shared/route_confirmation_desktop.cpp`: newly included developer headers lead to Windows socket-header ordering conflicts. Local repair includes `winsock2.h` before those dependencies
- `android/XL_compat/runtime/pumo_player_test.h`: explicitly capture `sweep_time` in the lambda; explicitly declare the JSON predicate's `bool` return type because engine `true`/`false` macros otherwise conflict with inferred Boolean returns

Two older defects also blocked this review and have separate local repairs; do not attribute them to the stack:

- `android/tests/game_automate_input.cmake`: missing SDL include directory, originating in pre-stack commit `ee7265c2c`
- `android/XL_compat/audio_voice.cpp`: x86 narrowing warnings treated as errors for already-bounds-checked 64-bit sample indices, originating in `f72b1fa0e`; explicit `std::size_t` casts repair the conversion

After these repairs both game binaries and the D2 native developer lifecycle target build. Add the actual developer target to the supported Windows architecture matrix, rather than relying only on small contracts or a normal game build

The full local asset-free x86 suite remains blocked later by an existing ambiguous indexing expression in the pinned OOF reference adapter (a vector-derived Array also has implicit pointer conversion). Keep this as a distinct reference-harness portability issue; do not modify the pinned source casually

### 4. Render-clock integration has a user-visible replay limitation

`android/XL_compat/runtime/slow_motion_runtime.cpp:200` rejects input demos whenever an enabled XL level's player owns Omega, with the explicit message that a render-clock timeline is unimplemented. Recorder/replayer hooks enforce this even without using a slow-motion activation

The guard is honest, but this is a capability tradeoff requiring an integration decision. Finish the recording/replay timeline contract before presenting the work as generally integrated, or defer the dependent Omega/render-clock change. Do not remove the guard and silently permit nondeterministic replays

Slow-motion activation itself is currently exercised by test callers; ordinary pickup/control activation remains incomplete, and pickup IDs 48/49 remain rejected. Save version 30 -> 32 adds weapon-clock state but does not establish complete active slow-motion persistence. Separate the incomplete feature from independently useful combat corrections

### 5. Readiness baselines and large contract suites do not establish campaign support

The readiness artifact reports 4/8 passing, all four passes from classic/imported cases, with zero passing XL-active cases. Newly uploaded XL campaigns are 0/4: BoilingPoint level 7, Dinter level 1, Sphere level 2 and WShock level 6 all report `load_failed`, with no useful root-cause fingerprint

Merged #456 makes those known failures expected for reporting; it does not repair them or waive the separate native test failures. Label them explicitly as unsupported/blocked until import/load/runtime causes are separated and verified

The production changes are mostly narrowly gated to XL, and pinned-source arithmetic comparisons plus classic/multiplayer negative controls are valuable. The weakness is composition: synthetic CPU consumers and per-action receipt checks can pass while the linked engine harness aborts. The native reproductions above demonstrate this gap concretely

## Simplification and integration sequence

1. Land independent build hygiene/tooling (#454 separately) and bounded CI diagnostics. Repair native fixture lifecycle and reproduce all five original failures to passing before accepting gameplay integration
2. Separate opt-in observation/probe tooling from runtime features. Keep one clear developer boundary and one packet format; retain asset provenance and authentication checks
3. Group damage, ordinary contact, force/torque, blast and mine-chain changes into coherent combat patches with linked native tests, pinned arithmetic comparisons, and classic/multiplayer controls. Preserve their source-specific rounding and ordering; avoid a generic physics rewrite
4. Consolidate Pumo route tests into a common scenario runner and declarative actions. The largest warm JSON fixtures alone contribute about 18,600 lines; `boss_gameplay_consumer_test.cpp` contributes another 5,531. Share setup, state capture and cleanup rather than adding another standalone contract for each hallway/switch. Retain meaningful negative cases and explicit assisted-versus-connected-travel distinctions
5. Keep slow-motion/render-clock/save-format work as a separate feature series with explicit activation, persistence and replay acceptance criteria
6. Rebase/regroup only after preserving the frozen tip and confirming the semantically repeated parent repairs. Run curtailed CI at each coherent integration boundary and the complete public/private tip checks on the final candidate

Do not squash everything into one unreviewable patch or keep all 58 micro-PRs as permanent architectural boundaries. A few feature-sized groups can retain useful source rationale while reducing coordination and test duplication

## Validation performed and limits

| Check | Result |
| --- | --- |
| Existing tip public engine smoke, sanitizer/replay and XL contract workflows | Passing; inspected actual jobs, not just aggregate badges |
| Fresh Windows D1 build | Builds after pre-existing SDL include repair |
| D1 registered tests | 88 pass after focused input retest, 1 skip, 1 unresolved music PCM assertion out of 90 |
| Fresh Windows D2 game and developer lifecycle target | Build after local portability repairs |
| Original five private native failures | Same five reproduced locally with original assets |
| Countdown after expectation repair | Full focused native test passes |
| Pinned trigger/countdown oracle | Pass, including 370,998 countdown comparisons |
| Projectile after first assertion repair | Hostile-contact cases pass; Gauss stage still fails |
| Private-asset controller unit suite | 42/42 pass with CMake on PATH |
| Developer compile contract | 2/2 pass |
| Probe artifact unit suite on Windows | 6 tests, 2 errors: Linux x86_64 identity fixture does not mock Windows AMD64 host architecture |
| Full local public x86 contracts | Blocked by reference OOF adapter indexing ambiguity after audio cast repair |

The remaining D1 music assertion concerns retaining the loaded source after stop. Its source is unchanged by the stack, but I did not run a full baseline comparison and therefore do not conclusively assign its cause

No Android/emulator run, interactive campaign playthrough, actual multiplayer session, or full private-suite rerun was performed. Cached third-party dependencies were reused; engine objects were freshly compiled. Initial local missing-tool/missing-D1-HOG setup failures were corrected and are not counted as stack defects

## Local deliverables

All proposed source edits remain uncommitted in the detached worktree. The limited repairs are review aids, not a claim that the stack is green

- `temp/xl-stack-review-20261109/stack-repairs.patch`: two test expectation fixes and the stack's Windows developer build repairs; includes formatter changes within the touched files
- `temp/xl-stack-review-20261109/host-repairs.patch`: the two pre-existing local host build repairs, kept separate for attribution
- Both patches passed reverse-application checks against the edited worktree; `git diff --check` passed
- The required single scoped mixed-language formatter invocation passed for all six edited files (`scoped-quality-final.log`). Earlier attempts could not launch the installed clang-format from the sandbox; the authorized external-tool run succeeded
- Final post-format D2 game/developer build succeeded, and the full countdown test passed again (`final-build.log`, `native-final-countdown/results.json`). The existing C4805 warning in untouched `changing_light_test_records.h:18` remains
- Scratch reproduction entry points: `msvc-env.ps1`, `build-local.ps1`, `run-focused.ps1` and `run-public.ps1` under the review artifact directory; these refer to this machine's existing dependency/asset paths

The report is suitable for planning integration. The patches still require the remaining fixture repairs and supported-platform CI before a merge candidate can be called green
