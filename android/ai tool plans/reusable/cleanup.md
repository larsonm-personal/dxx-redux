# Reusable cleanup playbook

This playbook is for repeatable cleanup passes on the Android branch: lint and warning cleanup in `android/`, warning cleanup in branch-added `d1/` or `d2/` code, D1/D2 diff shrinkage, de-duplication, and test-suite simplification or runtime reduction.

Use it when the goal is to reduce maintenance cost without changing user-visible behavior.

## Source plans

The guidance here is distilled from these prior plans:

- [plan_code_cleanup_and_test_cleanup_20260514.md](../plan_code_cleanup_and_test_cleanup_20260514.md)
- [d1d2_diff_shrink_study.md](../d1d2_diff_shrink_study.md)
- [cleanup_metl154_rename_finalize.md](../cleanup_metl154_rename_finalize.md)
- [cleanup_net_udp_extract.md](../cleanup_net_udp_extract.md)
- [plan_fix_branch_touched_warnings_20260428.md](../plan_fix_branch_touched_warnings_20260428.md)
- [plan_cmake_format_lint.md](../plan_cmake_format_lint.md)
- [plan_run_all_tests_prereq_preflight_20260515.md](../plan_run_all_tests_prereq_preflight_20260515.md)
- [relay-removal-and-test-dedup.md](../relay-removal-and-test-dedup.md)
- [plan_test_parameterization.md](../plan_test_parameterization.md)
- [plan_lint_cleanup_and_vscode_exclusions.md](../plan_lint_cleanup_and_vscode_exclusions.md)

## First step

Create a tranche-specific plan under `android/ai tool plans/` before editing. Keep it short and update it as the work finishes.

Suggested shape:

```markdown
# Plan: Cleanup Tranche YYYY-MM-DD

## Goal
- One sentence about what gets cleaner

## Scope
- Files or subsystems in scope
- Files or subsystems explicitly out of scope

## Cheap check
- Fast command or focused test that should prove the change is safe

## Steps
- [ ] Baseline current diff, warnings, and test behavior
- [ ] Apply the smallest cleanup slice
- [ ] Run scoped formatter, build, and focused tests
- [ ] Compare before and after, then update this plan
```

Avoid bundling unrelated cleanup. A good tranche can be validated in one sitting and has a before/after signal.

## Baseline before editing

Run only the probes needed for the current tranche. Useful defaults:

```powershell
# Show current local changes before touching anything
git status --short

# Track D1/D2 branch delta before and after a diff-shrink tranche
.\android\diff_vs_upstream.ps1 -Top 20

# Check for stale file-mutating formatter tasks before any cleanup rerun
.\android\stop-stale-formatters.ps1
```

If `stop-stale-formatters.ps1` reports an active stale formatter after a timeout, interruption, or file-newer prompt, kill it before starting another formatter pass:

```powershell
.\android\stop-stale-formatters.ps1 -Kill
```

Do not start two cleanup or formatter passes in parallel. `android/run-code-quality.ps1` uses `android/temp/run-code-quality.lock.json`; treat a live lock as real unless the owning process is gone.

## General cleanup principles

- Prefer behavior-preserving cleanup: remove dead diagnostics, narrow warning fixes, move duplicate helper bodies, and delete compatibility aliases only when the replacement is already proven.
- Keep changes small enough that failures identify the last slice, not a mixed pile of refactors.
- Record the before/after metric that justified the work: warning count, `diff_vs_upstream` totals, test count, runtime, or duplicated helper count.
- Do not fix inherited upstream warnings or style in `d1/` and `d2/` unless the branch caused them.
- Do not format broad upstream-owned files in `d1/` or `d2/` just because a formatter can touch them.
- Keep new Android code under `android/` where possible, especially `android/app/src/main/cpp/shared/` for native helpers used by both games.
- Preserve Windows, Linux, and macOS host builds. Android-only changes in upstream-like files should be behind the local preprocessor style already used nearby.
- Keep logs concise. Remove success-path spam after it has served its debugging purpose, but keep warnings, malformed packet diagnostics, timeouts, security-relevant failures, and hard failure breadcrumbs.

## Lint and warning cleanup

Use the central script for Android-owned code:

```powershell
# Scoped, file-mutating cleanup for touched Android files
.\android\run-code-quality.ps1 -Fix -Paths android\run_all_tests.ps1 android\tests\test_launcher_dpad.ps1

# Full cleanup pass when the touched surface crosses language/tool boundaries
.\android\run-code-quality.ps1 -Fix
```

`run-code-quality.ps1` currently wraps these tools:

- `clang-format` for C/C++
- `ktlint` for Kotlin
- `PSScriptAnalyzer` for PowerShell
- `shellcheck` for shell linting
- `shfmt` for shell formatting
- `cmake-format` and `cmake-lint` for branch-added CMake files

CMake formatting and linting are intentionally scoped to branch-added CMake files:

- `android/app/src/main/cpp/CMakeLists.txt`
- `android/app/src/main/cpp/extract/CMakeLists.txt`
- `cmake/*.cmake`
- `tools/etc2tool/CMakeLists.txt`

Do not apply cmake-format or cmake-lint to upstream `d1/` or `d2/` CMake files unless a tranche explicitly changes that policy.

Warning cleanup rules:

- Fix only warnings caused by branch-touched code.
- Leave warnings that already exist on the upstream/cmake baseline alone and mention them in the tranche notes.
- Prefer deleting unused branch leftovers over adding suppressions.
- If a variable exists only for Android logging, put the declaration in the same preprocessor block as its use.
- If a variable is genuinely needed on one side of a conditional but intentionally unused on another, use the smallest local `(void)name;` suppression.
- Match the surrounding macro spelling. This tree has nearby code using forms such as `ANDROID`, `__ANDROID__`, and `__android__`; do not normalize a file as part of a warning fix.

Small example for Android-only logging:

```c
#ifdef __ANDROID__
char logbuf[256];
snprintf(logbuf, sizeof(logbuf), "sync failed player=%d", player);
MPDIAG(logbuf);
#endif
```

Small example for an intentional non-Android unused variable:

```c
poll_count++;
#ifdef __ANDROID__
MPDIAG_SYNC_COUNT(poll_count);
#else
(void)poll_count;
#endif
```

## D1/D2 diff minimization

Use [android/diff_vs_upstream.ps1](../../diff_vs_upstream.ps1) to choose and measure tranches:

```powershell
.\android\diff_vs_upstream.ps1 -Top 20
```

Good first targets are files with large Android-only helper bodies in both games, especially when the bodies are near-duplicates. Prior plans found high-value categories like:

- OGL diagnostics and merged-wall helpers in `d1/arch/ogl/ogl.c` and `d2/arch/ogl/ogl.c`
- UDP Android networking helpers in `d1/main/net_udp.c` and `d2/main/net_udp.c`
- Android introspection, automation, touch, and overlay hooks in menu/control files
- branch-added helper files that exist separately in both `d1/` and `d2/`

Preferred extraction pattern:

1. Identify the smallest duplicated helper body that is Android-owned or branch-owned.
2. Move the implementation to `android/app/src/main/cpp/shared/` or a subdirectory such as `shared/net/`.
3. Keep D1 and D2 call sites in place and replace only the local helper body with a thin wrapper or direct shared call.
4. Pass game-owned state, callbacks, log adapters, and type-specific pieces as parameters instead of reaching across ownership boundaries.
5. Wire the shared source into both D1 and D2 CMake targets.
6. Build both Android and Windows host targets before continuing to the next helper.

Thin wrapper example:

```c
#ifdef __ANDROID__
static int
net_udp_rebind_for_hosting(void)
{
    return android_net_udp_rebind_for_hosting(
        &UDP_Socket,
        net_udp_open_socket,
        net_udp_close_socket,
        net_udp_mpdiag_adapter);
}
#endif
```

When D1 and D2 types differ enough that one shared source would become unsafe or unreadable, use `android/app/src/main/cpp/shared/d1/` and `android/app/src/main/cpp/shared/d2/`. A two-copy split in `android/` is still better than large Android-only bodies in upstream-like engine files.

Do not consolidate existing upstream D1/D2 files just because they are similar. This project intentionally tracks upstream duplication. The cleanup target is branch-added code, Android-only code, and helper bodies that can leave `d1/` and `d2/` with small hooks.

## Rename and dead-diagnostic cleanup

Old investigation names can linger after the code becomes generic. Clean them up only when the runtime behavior is already understood.

Good rename/deletion candidates:

- hardcoded target names from one-off investigations, such as old texture or level names, when the code is now generic
- compatibility alias bridges whose users have already moved to the new name
- debug helpers that no longer have active callers
- success-path logs that flood output during normal tests

Guardrails:

- Keep D1 and D2 mirrored unless a real signature or data-layout difference forces divergence.
- Prefer small anchored patches in both files.
- Do not change route selection, packet layout, save format, or rendering behavior as part of a rename cleanup.
- If a diagnostic is still useful, rename it to the generic feature name and keep it gated.

## Test cleanup and runtime reduction

The recurring test cleanup goal is to keep coverage high while reducing repeated setup, duplicated scripts, stale state, and blind waiting.

Use shared helpers before adding local helper copies:

- [android/test_helpers.ps1](../../test_helpers.ps1) for PowerShell test helpers
- [android/run_test.ps1](../../run_test.ps1) for a single automation script
- [android/run_all_tests.ps1](../../run_all_tests.ps1) for suite cataloging, tiers, filters, and unattended runs
- [android/game_scripts/](../../game_scripts/) for maintained game automation scripts
- [android/tests/](../../tests/) for wrapper or host-side test scripts

De-duplication rules:

- Source `test_helpers.ps1` instead of copying helpers like `Write-Status`, `Send-MpCommand`, `Wait-ForCondition`, setup readiness checks, emulator redirection helpers, or introspection helpers.
- Keep only test-specific cleanup or orchestration in an individual test script.
- Remove obsolete relay or emulator redirection paths when the platform no longer needs them.
- If several scripts differ only by one fixture, resolution, game, or option, parameterize the script instead of maintaining copies.

Parameterization pattern for JSON5 automation scripts:

```json5
_info: {
  params: {
    RESOLUTION: {
      options: {
        "128": { DXA_SHA: "..." },
        "256": { DXA_SHA: "..." },
        "512": { DXA_SHA: "..." },
      },
    },
  },
},
```

Then use `${RESOLUTION}` and `${DXA_SHA}` in the script and dependency metadata, and delete the old per-value scripts after the runner supports the params.

Suite cleanup rules:

- Add explicit `run_all_tests.ps1` catalog entries for wrapper scripts that auto-discovery misses.
- Mark manual helpers as manual-skipped instead of letting unattended runs hang or fail ambiguously.
- Classify tests into the lightest valid tier: no-infra, emulator, server, extract/import, manual, or equivalent local categories in the current runner.
- Use `-Filter` for focused validation and `-StopOnFail` when iterating on a failing slice.
- Make runner-owned prerequisites fail fast before Tier 0 if the runner cannot restore them.
- Clear cross-test state in shared reset paths, not in every individual test.
- Prefer durable readiness signals over sleeps: setup introspection, automation result files, automation logs, debug-log progress checkpoints, and specific game introspection fields.
- For long launcher-backed tests, treat default timeouts as floors and extend from durable progress signals rather than plain wall-clock waits.

Useful commands:

```powershell
# Focused suite validation
.\android\run_all_tests.ps1 -Filter test_input_demo_regressions -StopOnFail

# Single game automation run with output captured to a file
$dep = (Get-Content .\dependency_base.txt -First 1).Trim()
& "$dep\android-sdk\platform-tools\adb.exe" logcat -c
.\android\run_test.ps1 -ScriptName test_pause_menu_return.json5 -Game d2 -TimeoutSeconds 180 2>&1 | Out-File temp\test_pause_menu_return_d2.txt -Encoding utf8
Write-Output "EXIT: $LASTEXITCODE"
Get-Content temp\test_pause_menu_return_d2.txt | Select-Object -Last 60
```

For Android test runs, always clear logcat first and capture output to a file. Do not rely on terminal scrollback for pass/fail.

Report triage rules:

- Treat old suite reports as historical until a focused rerun proves the failure still reproduces.
- Include both `FAIL` and `TIMEOUT` rows when building a candidate list from `run_all_tests.ps1` output.
- Inspect the full per-test log, not just the markdown report tail. Multi-game scripts can show a later game pass after an earlier game failed.
- Prefer durable files and explicit markers over generic log tails: `automation_result.json`, `automation_log.jsonl`, `ASSERT_FAIL`, `TIMEOUT`, `FAIL for`, runner kill lines, setup readiness failures, and emulator health failures.
- Use a dedicated `-ReportDir` for each triage round so fresh evidence is not mixed with stale suite output.

## Validation ladder

Pick the smallest ladder that covers the tranche. A typical cleanup ladder is:

```powershell
# 1. Formatter/linter pass after edits
.\android\stop-stale-formatters.ps1
.\android\run-code-quality.ps1 -Fix -Paths path\to\touched\file path\to\touched\dir

# 2. Android build and JVM unit tests for launcher/native integration
.\android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain

# 3. Host build when d1/, d2/, shared native C/C++, or CMake changed
.\run-windows-build.ps1 -Target both

# 4. Focused suite or script validation
.\android\run_all_tests.ps1 -Filter changed_area_or_test_name -StopOnFail

# 5. D1/D2 diff metric after diff-shrink tranches
.\android\diff_vs_upstream.ps1 -Top 20
```

If a command times out or the editor reports that a file is newer, stop and check stale formatter tasks before rerunning anything mutating.

When server code is touched, use the server-specific path instead of the Android ladder alone:

```powershell
Push-Location server
cargo build
cargo test
Pop-Location
.\server\rust_lint.sh
```

## Definition of done

A cleanup tranche is done when:

- the plan file says what changed, what was intentionally left alone, and which validation passed
- `d1/` and `d2/` changes are mirrored where the feature exists in both games
- branch-owned warnings introduced or exposed by the tranche are fixed
- broad inherited warnings are documented but not churned
- file-mutating formatters have finished and no stale formatter task remains
- focused tests pass, or any skipped test is explicitly categorized as manual/environmental
- `diff_vs_upstream` before/after totals are recorded for diff-shrink work
- any new shared helper has both Android and host-build coverage when it is compiled into both paths

## Quick checklist

- [ ] Create and maintain a tranche plan in `android/ai tool plans/`
- [ ] Baseline `git status --short` and, for D1/D2 cleanup, `diff_vs_upstream`
- [ ] Choose one small cleanup slice
- [ ] Keep branch-owned code in `android/` or `shared/` when possible
- [ ] Keep D1 and D2 call sites mirrored and thin
- [ ] Fix only branch-owned warnings
- [ ] Use shared test helpers and parameterization before adding scripts
- [ ] Run scoped `run-code-quality.ps1 -Fix`
- [ ] Run Android build/unit tests and host build as needed
- [ ] Run focused test filters or captured `run_test.ps1` scripts
- [ ] Update the tranche plan with results and remaining work
