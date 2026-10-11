# Exact selected GuideBot browser route, 2026-10-11

GQR-0287/GQF-0301 admitted TODO/OPEN from GQ2-CHUNK-0384 after publication audit PASS4149. Diagnosis and implementation plan only under plan_general_diagnosis_resume_20261009.md. No implementation or runtime acceptance.

## Concrete problem and required result

watch_guidebot_simulation.ps1 indexes MissionJson/TargetIndex/Level/LevelFile, but both visible and headless launch pass only MissionJson and Level. regenerate_all_guidebot_simulations.ps1 has no TargetIndex parameter and enumerates every compatible metadata target with that level. Current -MOON-.json target0/MOON01.RL2 and target1/MOON02.RL2 both use level0. Selecting one indexed item can therefore run other missions, and the browser's first simulation-file lookup can omit the selected target. This differs from BR-0668 duplicate corpus identities: these are distinct valid identities whose caller filter is incomplete.

## Allowed implementation boundary

- Browser helper, regeneration runner's explicit work-item selection, and maintained test_guidebot_simulation_browser.ps1 or its focused support fixture
- Keep default whole-corpus behavior and existing Mode/Level/LevelFileFilter/sampling/publication contracts. Add an optional exact target selector to the actual runner if necessary; do not invent a second runner or duplicate native mission policy
- Pass the selected identity on both Desktop and Headless paths. Resolve results/progress against that exact request and expected artifact instead of the first file in a directory
- Simplify Get-GuidebotBrowserEntries' identical array/object branches to the shared @($value) expression, retaining compatibility-helper enumeration behavior and malformed-input policy
- Existing process ownership, cleanup and verified output publication remain BR-0173/0543/0233; this candidate owns selection and correlation only

## Acceptance before implementation completion

- [ ] Actual runner work-item selection proves one exact requested target/level/file from synthetic multi-target metadata with overlapping levels, including level0 and negative levels; unspecified selector preserves all existing work items
- [ ] Actual browser invocation construction for visible, restart and headless paths captures the selected target; stubbing process launch is acceptable only if production selection still executes
- [ ] Decoy-first result/progress files cannot substitute another target; missing, duplicate, conflicting or wrong-level selected evidence is diagnosed without displaying an unrelated result
- [ ] Existing single-target filters/search/priority/initial rendering and scalar/array metadata controls pass, including malformed input and missing target
- [ ] Existing timeouts, child exit reporting, Repeat and batch publication behavior retained; focused catalog and scoped quality checks pass if test registration changes

Do not require a GUI game run for the pure filter regression. A representative actual visible/headless selection is separate integration acceptance when available. Preserve engine format authority, desktop platforms and D1-in-D2 identity; no inherited engine edit or saving is proposed.

Canonical admission is diagnosis only; every implementation acceptance gate above remains open. Immutable source snapshot preserves the preceding proposed-plan version
