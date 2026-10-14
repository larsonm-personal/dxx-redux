# Native functional configuration campaign, 2026-10-14

## Result status

This report covers the menu, settings, gameplay and keybinding work. Public D1/D2 shareware fixtures were used; imported D1 ran as an actual D1 mission inside the D2 engine. All three expanded matrices are terminal: 42 of 42 gameplay/settings cases passed. No additional campaigns are running in this workstream.

Two production fixes are committed locally on `test-overnight-config-fuzz`, based on the `cmake` merge `d3ad53950`:

- `d34e08b04`: refresh persistent main-menu background after a content switch and select candidate menu/stars resources in the actual presentation namespace. Four files, 12 additions and 2 replacements. Both directions of D1/D2 return are covered.
- `f9918585f`: correct imported-D1 builtin soundtrack routing. See the separate music report for actual converted-source fingerprints and PCM evidence.

A third failure remains unresolved: entering custom aspect `0x0` and exiting Screen Resolution produced SIGFPE in both engines. A small validation patch was prepared, but its build/runtime validation was stopped by an execution restriction. It is preserved outside the working source at `temp/held-aspect-repair/unvalidated-menu-input.patch`; both `menu.c` files were restored to committed HEAD. Do not describe this patch as fixed, verified, or part of either committed fix. The unvalidated regression is preserved at `temp/held-aspect-repair/test_host_aspect_menu.py` and excluded from the passed deliverable.

## Completed functional coverage

| Suite | Completed evidence | What was asserted |
|---|---|---|
| Original settings matrix | D1 15/15; D2 15/15 | Eight-axis pairwise configurations plus music/filter/stereo triples; actual widget values, effective configuration, A-B-A, reconstructed widgets, config persistence, clean process relaunch |
| Expanded gameplay matrix | D1 14/14; D2 14/14; imported D1 14/14 | Eleven axes, 281 planned interactions per mode, actual difficulty and committed presentation, graphics/control/reticle widgets, automap, live options A-B-A, changed ship orientation then save restoration, real rendered gameplay, relaunch and restored baseline |
| Persistent menu return regression | 4/4 routes | Native D1, native D2, imported D1, and one disposable D2 process/profile running D1 -> D2 -> D1; six level/Abort transitions, mission-picker highlight/cancel, Options return, 16 nonblank valid rendered captures, clean exit |
| Keyboard binding regression | D1, D2, imported D1 all pass | Cancel leaves the full map unchanged; unused U initially causes zero motion; rebound U moves the ship while old Up causes zero motion; binding persists across relaunch; complete map restoration reverses these controls; windows exit cleanly |
| Controller controls | 11/11 | False assertions, hung/stale observations, crashes, sanitizer diagnostics, late teardown failure, incomplete campaign, ambiguous labels, transient menu snapshots and clamped listbox navigation rejected |
| Gameplay oracle controls | 5/5 | Wrong mission/level, no actual pre/post change and ineffective save restoration rejected; custom pairwise and high-risk triples complete |

The expanded matrix axes are transparency effects, colored dynamic light, cockpit visibility, classic depth ordering, sticky rearview, mouse flight indicator, reticle shape, reticle size, keyboard sensitivity, mouse sensitivity and difficulty. Seed is `20261014`. The two high-risk triple groups are transparency/dynamic-light/classic-depth and cockpit/reticle-shape/reticle-size.

The binding regression contains three negative and three positive real input-motion windows per mode. All nine negative windows have exactly zero forward-vector delta. Positive windows exceed the explicit movement threshold. Save restoration requires a deliberate preceding change, the same actual mission/level, and restored orientation within the asserted tolerance; first smoke restoration errors were exactly zero.

## Evidence locations

All paths below are repository-relative. Temp artifacts are retained evidence, not source deliverables.

- Original settings: `temp/host-config-final-d1/summary.json`, `temp/host-config-final-d2/summary.json`
- Expanded gameplay: `temp/host-gameplay-expanded-matrix-{d1,d2,d1in2}/summary.json`, per-case `result.json`, `trace.jsonl`, captured frame and engine logs; `cases.json` records exact factors and rows
- Menu fix before/after: `temp/host-mode-menu-before-fix`, `temp/host-mode-menu-after-fix`, `temp/host-mode-d2-return-diagnostic`, `temp/host-mode-menu-scoped-fix-rerun/summary.json`
- Menu failure diagnostic: `HOST_CONFIG_PCX_ERROR path=d2-original/d2-original/menu.pcx hires=0 error=1`; initial root existence selected a D1 filename for D2 demo, then the missing scoped path was prefixed a second time
- Binding: `temp/host-bindings-{d1,d2,d1in2}/result.json` and traces
- Deliberate live controller controls: `temp/host-config-native-controls-d1/result.json`
- Confirmed unresolved aspect failure: `temp/host-aspect-zero-repro/{d1,d2}/result.json` and traces
- Public combined fixture hashes: `temp/host-public-d1d2/manifest.json`
- Immutable binaries and exact source hashes: `temp/overnight-binaries/*/manifest.json`; expanded matrices use D1 `fda30bc9bd7c` and D2 `396d5a3ff83c`, bindings use D1 `ecc6d7fa62e7` and D2 `f6ed2037858d`

Source commit alone does not identify these diagnostic binaries: the manifests also record working-tree/source hashes and binary SHA256. None includes the unvalidated aspect patch.

## Oracles and limitations

The Python process runners are Linux-only: they use POSIX process groups (`killpg`) and Linux profile locations. The gameplay CLI requires an explicit timezone-qualified `--deadline` for future reproductions. Existing running campaigns retain their original 14:00 UTC cutoff.

The bridge is OFF by default (`DXX_HOST_CONFIG_PROBE`) and reads live engine state while queueing ordinary SDL key events. It does not set game configuration or gameplay state directly. Each case has a private HOME/profile, operation and process bounds, immutable executable copy, full command/state trace and fatal-log checks. A clean exit is required and late diagnostics are checked. Deadline exhaustion/incomplete execution is not success.

Rendered captures use actual OpenGL output in SDL's offscreen compatibility context with software Mesa. Capture metadata checks GL errors, dimensions and byte counts; spatial checks reject blank frames. Separate rendering work proves nearest/trilinear differences. Matrix captures establish valid nonblank output, not comprehensive visual correctness of every graphics option.

Native audio is SDL_mixer/FluidSynth with a public SF2. The matrix settings alone do not prove correct audio source or samples; use the separate music/source report. Full Android launcher/mod combinations, Android ymfm/TSF behavior, retail fixtures, hardware/mobile GPU behavior, fullscreen/window-manager behavior and Android lifecycle remain outside these native results. The original static mod coverage was not repeated or presented as this transition coverage.

No merges or remote publication occurred.

## Infrastructure eligible for a separate local commit

The exact 21-file list owned by this workstream is retained at `temp/host-infrastructure-owned-files.txt`. The separately owned offline wrapper/catalog and music/render hooks must be coordinated into the same infrastructure package. No files are still being written by this workstream.

Owned menu/gameplay/controller source:

- `android/tests/host_config_probe.c`, `android/tests/host_config_probe.h`
- `android/helpers/host_config_campaign.py`, `android/helpers/host_gameplay_campaign.py`, `android/helpers/host_binding_campaign.py`
- `android/tests/test_host_config_campaign.py`, `android/tests/test_host_gameplay_campaign.py`, `android/tests/test_host_mode_menu_return.py`
- Guarded bridge build/hooks in each engine's `CMakeLists.txt`, `main/CMakeLists.txt`, `arch/sdl/event.c`, `main/newmenu.c`, `main/kconfig.c`
- `android/tests/README.host-functional-campaigns.md`, this report and `android/ai tool plans/overnight-stateful-configuration-2026-10-14.md`

Coordinate the separately owned music/render/component-test files before including them. Scoped clang-format and Ruff completed successfully on the new bridge/runners; legacy C files were skipped by the repository's new-file formatting policy. Final clean verification passed all 149 CTests and both probe-OFF production builds. Final packaging verification passed all 34 explicitly selected offline Python controls (stdlib-only, including Python `-S`) and all 252 automation-catalog checks; plain test listing was valid. The strict aggregate catalog remained blocked by the existing `/proc` child-process capability limitation, and PSScriptAnalyzer was unavailable. Those blocked checks are not claimed as passing.

Explicitly exclude `temp/held-aspect-repair/` and the unused optional `temp/held-optional-diagnostics/host_config_repro_input.c`. These were preserved intact outside test discovery; committed source retains the earlier verified menu fix. Keep all temp evidence out of the infrastructure commit.
