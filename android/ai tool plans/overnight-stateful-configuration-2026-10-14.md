# Overnight stateful configuration campaign

## Goal and boundary

Exercise real supported settings transitions with deterministic, replayable cases until 2026-10-14 14:00 UTC. This is behavioral/configuration exploration, not binary mutation. User subsequently authorized small production fixes as failures are collected. Pause expansion for each confirmed failure, preserve the before-fix repro, make the smallest sound fix and rerun focused plus neighboring cases. Do not merge or publish changes. Keep profiles and outputs isolated per case. Retail fixtures are unavailable; public shareware coverage and Android-only gaps must remain explicit.

## Capability gate

Desktop CMake does not compile Android game automation/introspection. Add a small OFF-by-default DXX_HOST_CONFIG_PROBE test bridge for semantic menu snapshots and queued SDL key events. Reuse actual menu callbacks, config persistence, graphics rendering and audio implementations. Do not port the Android framework. A successful process alone is not a functional pass.

## Execution order

1. Build both desktop engines with the test bridge and verify public D1/D2 data hashes
2. Smoke each actual menu path with semantic title, item, selection and effective-setting assertions
3. Generate deterministic pairwise configurations over supported volume, music mode, stereo, filtering, vsync, brightness and mouse-control choices; add risky graphics/music/navigation triples
4. Exercise A -> B -> A, back/reopen and relaunch persistence, then gameplay/menu/automap/save/load transitions where the native bridge exposes a reliable oracle
5. Reproduce failures in fresh profiles and retain exact command sequences, snapshots, configuration bytes, logs and source/build identity
6. Reduce failed sequences only while preserving prerequisites and the same failure signature; never reduce away the oracle

## Oracles and reporting

Every reported functional pass requires observed menu route and effective value assertions; persistence passes additionally require config bytes and fresh-process state. Detect crashes, sanitizer reports, early exit and bounded hangs separately. Audio playback/output and GPU appearance are separate oracles, never inferred from settings alone. Public D1-in-D2, mod packs, Android symfm/SF2, launcher lifecycle and graphics safety are tested only if the actual target supports them; unsupported cells are skipped with reasons, not silently dropped or passed.

## Runtime bounds

Single game process at a time per private sandbox/display; monotonic case and operation deadlines. Stop scheduling cases at the overall deadline and finish/terminate the current bounded case. Preserve partial results and journal incrementally. Baseline controls precede randomized cases. Seed: 20261014. No shared user profiles or device state.

## Confirmed failure and fix gate

Public D1-in-D2 level 1 -> Escape -> Abort Game reproduces an assertion in nm_draw_background1 on two fresh profiles and an independent music run. The persistent main menu retains the initial D2 demo background name (menud.pcx), which is re-resolved against D1 presentation assets on return. Refresh its stable menu background path when the main menu reactivates. Do not suppress missing-resource assertions. Add an explicit-runner regression for native D1, native D2, and D1 -> D2 -> D1 using one disposable profile, including real rendered main-menu capture. Preserve pre-fix binary and data hashes. Optional external interception/probe-OFF reproduction was stopped after a capability block and must not be retried.

The initial two-line refresh passes native D1/D2 and imported-D1 return, but the mandatory same-process D1 -> D2 -> D1 gate still fails on return from D2. Hold the production commit until content-scoped filename selection is diagnosed and both directions plus mission-picker cancellation/options-return pass. Before/after evidence: temp/host-mode-menu-before-fix and temp/host-mode-menu-after-fix.

The diagnostic build records `HOST_CONFIG_PCX_ERROR path=d2-original/d2-original/menu.pcx hires=0 error=1` on the reverse return. D2 demo low-resolution candidate selection incorrectly sees D1 menu.pcx at root after D1 is mounted. Existence checks must use the same scoped presentation resolver as the eventual image read. The fix adds a small resolver-backed existence helper and uses it for main/stars candidate selection; retain activation refresh. The four-file production patch is 12 additions and 2 replacements. Native D1/D2 and imported-D1 options-return/cancel-picker controls already pass; the full rebuilt gate remains pending.

Final menu gate: temp/host-mode-menu-scoped-fix-rerun/summary.json passes all four planned routes (six real level/Abort transitions, 16 valid rendered captures, mission-picker cancellation and options-return, clean exits). Immutable D2 e134d2850c06 includes only the menu production fix, no incoming songs fix. Both probe-OFF builds pass. A controller issue discovered en route (listboxes clamp, ordinary menus wrap) was fixed with a dedicated negative unit control; all 11 controller controls pass. The focused four production files are staged; commit currently awaits a Git author identity.

## Current handoff status

The focused menu fix is committed as d34e08b04; the independent soundtrack fix is f9918585f. Native D1 and D2 expanded gameplay matrices each completed 14/14 with all 281 planned interactions covered. The existing imported-D1 matrix is running. Keybinding cancel/persistence and actual before/after input-motion controls pass in all three modes. Detailed durable results and infrastructure exclusions are in testing/host_configuration_results_20261014.md.

Custom aspect 0x0 caused SIGFPE in both native engines through the ordinary Screen Resolution UI. A mirrored validation/current-mode-fallback patch was prepared and statically reviewed, but repeated execution restriction notifications stopped its build/runtime verification. Keep that patch and test uncommitted and unvalidated. Parent narrowed the remaining work to completion of already-running unrelated matrices and evidence consolidation; do not launch new test axes or retry aspect work.

The stopped aspect repair is now preserved at temp/held-aspect-repair/unvalidated-menu-input.patch with its unvalidated regression beside it; both source menu.c files were restored to committed HEAD. The unused optional diagnostic source is preserved at temp/held-optional-diagnostics/host_config_repro_input.c. Neither held directory belongs in the verified infrastructure commit.

## Final bounded result

All already-running expanded matrices completed: D1 14/14, D2 14/14, imported D1 14/14, with all 281 planned interactions per mode covered. The 42-case workstream is stopped. Final clean verification reported 149 CTests and both probe-OFF builds passed; the exact offline allowlist passed 34 stdlib-only controls and automation catalog passed 252 checks. Strict aggregate catalog and PSScriptAnalyzer limitations remain documented. See the final dated report and reusable README.host-functional-campaigns.md; held unvalidated work remains excluded.
