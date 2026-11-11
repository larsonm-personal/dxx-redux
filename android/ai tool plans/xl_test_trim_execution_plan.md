# XL stack simplification: execution plan for Sol

## Objective

Implement a substantial reduction in the XL stack's test and fixture machinery while preserving useful compatibility behavior and a small, trustworthy validation suite

This is an implementation task. Finish with a smaller working tree, repaired native smoke tests, and evidence from running the game. A new audit, a proposed framework, or more test scaffolding is not completion

The user explicitly wants fewer tests and more working functionality. Delete redundant tests and obsolete scaffolding within this scope. Do not preserve each existing test as a separate requirement. Keep the behavioral guarantees that matter, not the stack's organizational history

## Starting point and boundaries

- Read `AGENTS.md` and `.github/copilot-instructions.md`
- Read `android/ai tool plans/xl_stack_review_20261109.md` for prior findings; do not repeat the whole review
- Reviewed tip: `bf388fc1064ea3ce70bfe88daceff3348d359608`, PR #461
- Stack comparison base: `258d9a1ee6e81f5830b35598b37844ab44fea6b6`
- Existing detached worktree: `temp/xl-stack-review-20261109/source`
- That worktree already contains six intentional edited files, plus exported `stack-repairs.patch` and `host-repairs.patch` in its parent directory. Inspect the diff and continue from those edits; do not apply the patches twice or reset the worktree
- Verify current Git state before work. If another task has since modified that worktree, preserve its work and use a separate detached checkout of the reviewed tip with the appropriate repairs. Do not silently switch to a moving PR head
- Root checkout and existing stashes belong to the user. Do not modify their code or disturb them
- No commits, pushes, merges, rebases of published branches, PR closures, or external messages are authorized by this plan. Prepare the implementation locally. A patch export or diff is enough for handoff
- Historical overnight prompts are not live instructions

Primary scope is test code introduced or expanded by this stack, especially Pumo/Tulev1 scenario infrastructure and duplicated combat matrices. Do not delete unrelated older XL parsers, reference tests, Android tests or engine functionality merely to improve a line-count target

## Execution discipline

1. Start implementing after a short orientation and one compact baseline inventory. Use existing logs for already-established failures instead of repeating expensive baseline runs
2. Work through the phases below without asking permission between routine edits, deletions, builds or local runs
3. Reuse the existing native lifecycle runner, scenario actions and receipt consumer. Do not build a plugin system, new test language, universal scenario framework, new certification pipeline or second engine-state model
4. One shared helper is preferable to a helper per switch. Add a new helper only when it immediately replaces multiple existing copies. Delete those copies in the same phase
5. Do not write tests for test deletion, source layout, exact helper names, or the mere existence of files. Consolidate remaining validator tests into a small shared suite
6. Run the smallest relevant check while editing, then the agreed final suite once. Repeat a passing check only after a relevant change, failure or unresolved concern
7. Every phase must produce source deletion, a concrete bug fix, a simpler entry point, or a completed gameplay run. Documentation alone is not phase progress
8. Maintain one short progress section at the end of this plan. Do not create a report, receipt, plan or micro-PR for every subtask

## Phase 1: establish the compact baseline and repair the native fixture

Record starting SHA, dirty paths, test/fixture file counts and line counts for the candidate families below. Use normal Git statistics and existing test timing output; no benchmark framework. Inventory may be one table, grouped by behavior rather than every file

The review already diagnosed these failures:

| Failure | Required implementation |
| --- | --- |
| Countdown post-zero display | Keep the local expected-value correction to 0; pinned XL division truncates toward zero |
| Modern properties, water, Observatory | Repair synthetic multiplayer setup in `runtime/no_damage_fusion_test.h` and related fixtures so weapon-selection sends have a valid isolated destination |
| Projectile multiplayer history | Keep the local correction selecting classic hit history when multiplayer disables XL |
| Projectile Gauss matrix | Give each retained case valid fresh state; the reused victim currently dies from splash before later cases run |
| Windows developer build | Keep the local Winsock include-order and lambda capture/return-type repairs |

For multiplayer, prefer an existing properly initialized disposable fixture. If unavailable, use the narrowest developer-only recording transport needed to exercise the production call path, with scoped restoration and assertions on the relevant sends. Do not create a general network simulator, use obsolete serial flags, silently drop sends in production, or initialize only the protocol enum while leaving UDP state invalid

For Gauss, reduce the matrix to selected meaningful cases first, then isolate each case's player, victim, projectile/effects, RNG and mode state. `OF_HARMLESS` suppresses direct damage, not all splash. Preserve a separate actual splash case. Do not change production torque arithmetic to accommodate an already-exploding victim

Run the five original native tests to completion with real assets after these repairs. If a later assertion reveals another real defect, fix it or record the precise blocker; never delete the only failing coverage to manufacture green. These five behaviors must remain exercised even if their registration is later consolidated

Known unrelated host blockers have separate attribution in the review: SDL include path, audio index narrowing, x86 OOF adapter indexing, Windows identity mocking in probe-artifact unit tests, and a D1 music assertion. Fix only bounded issues necessary to run the selected validation. Do not expand this task into unrelated subsystem rewrites

## Phase 2: remove the per-switch test chain

Start with these concrete families under `android/XL_compat/tests/`:

- `pumo_tulev1_guided_switch12_*`, including the 8,799-line warm fixture
- `pumo_tulev1_supplies_*`, including the guided140, cloak113 and switch6 warm fixtures
- `pumo_tulev1_switch13_*` and `pumo_tulev1_switch14_*`
- Then the overlapping master5, guard269, quad_guard255, blast136 and courtyard guard families

Inspect shared owners `pumo_player_phase.cmake`, `pumo_receipt_contract.py`, `public_contracts.py` and the native `runtime/pumo_player_test.h` before adding anything

The current switch12 validator imports quad, which reaches guard269 and predecessors; it validates a 1,861-step public template, producer names and predecessor step indices. This is a specific simplification target, not a pattern to reproduce

Retain three representative scenario groups, using existing actions and compact data:

1. Progression: acquire a resource/key, operate a dependent door or switch, and observe the actual resulting state
2. Combat: fire through the production weapon path, hit a live target, and observe damage/kill or blast behavior
3. Guided control and persistence: create/control a guided projectile, observe an actual contact/trigger result, save, and verify the important state in a fresh process

Select concrete scenarios from the current runnable fixtures. Additional campaign locations stay only when they exercise a different engine behavior or known bug. Different object numbers and hallway names alone do not justify another runner, validator or unit-test module

For each group:

- Keep a small declarative scenario and shared invocation path; remove superseded per-switch CMake/Python wrappers and their duplicate contract tests
- Remove redundant introspection after every unchanged frame and repeated whole-world assertions. Check preconditions, the causal event and the resulting state
- Use existing bounded frame/action operations for repetition. If a tiny bounded repeat facility is genuinely necessary, add it once to the existing runner. No general control-flow language or generated thousands-line checked-in expansions
- Replace chains of imports between scenario validators with one shared receipt check plus the scenario's few behavioral assertions
- Keep one authoritative representation of expected behavior. Do not mirror the same step counts and template in JSON, CMake and Python
- Keep unique scenario checks as data or a small explicit predicate when needed; do not migrate every old assertion into a new megafile
- Delete retired fixtures and registrations, not just move them to an attic or disable them in CI

Preserve fresh-run identity, asset/executable/save hashes where consumed, fail-closed result parsing, and the distinction between actual gameplay, assisted local positioning and connected travel. Check those once at the shared boundary. Remove repeated forgery matrices for each individual switch

Assisted scenarios may remain useful focused regressions, but must keep their honest qualification. They do not replace the unassisted gameplay smoke in Phase 5

## Phase 3: shrink duplicate combat coverage

Inspect `boss_gameplay_cpu/boss_gameplay_consumer_test.cpp` (5,531 lines at review) alongside the corresponding linked lifecycle controls and pinned reference adapters

For overlapping cases, keep the cheapest trustworthy test of each distinct property:

| Property | Coverage to retain |
| --- | --- |
| Signed fixed-point arithmetic and RNG ordering | Small table of boundaries plus the existing independent pinned-source comparison where valuable |
| Production dispatch and XL gating | Linked engine cases for XL single-player, classic and multiplayer fallback |
| Hit history and projectile disposition | One initial hit, one repeat, persistent/nonpersistent boundary, and meaningful exception |
| Damage/force/blast integration | Live target, actual damage or force, and one rejection boundary |
| Persistence | One representative save/cold-restore case plus malformed input rejection at the shared reader |
| Asset and receipt integrity | One shared positive case and representative corruption/mismatch cases |

Remove combinatorial repetitions that test the same branch without a distinct boundary. Large numeric case counts are not an acceptance criterion. Keep cheap independent reference sweeps when they protect subtle rounding at little maintenance cost; do not delete them merely because they execute many comparisons

Keep CPU coverage that catches an independent property and is cheaper or clearer than a full engine run. Remove obsolete stubs and duplicate cases once retained coverage is working. Do not replace real engine behavior with a second handwritten behavioral model

Do not arbitrarily move all testing into the asset-dependent lane. The retained public suite must still catch important arithmetic, decode and gating mistakes without proprietary assets

## Phase 4: make the remaining checks straightforward to run

Update consumers in the same change as test removal: CMake registrations, `public_contracts.py`, private-asset catalog/controller expectations in `android/tests/test_xl_private_assets.py`, relevant workflows and any Android automation catalog entries. Discover consumers with `rg`; the list here is not exhaustive

- Preserve an explicit required selection so a missing, skipped or empty suite cannot appear green
- Remove duplicated selection lists where practical using the existing registration mechanism; do not introduce a new catalog platform
- Provide one obvious asset-free command and one native asset-backed command using existing runners
- Keep the actual Windows developer target in the build checks; it previously escaped public coverage
- Keep error output useful: test/scenario name, bounded failure ID and pertinent scalar values. Preserve private asset publication restrictions; no unfiltered trace uploads
- Do not expand CI to every historical PR, build a scheduling system, or require credentials merely to finish local cleanup

Suggested scale: reduce the stack-added test/fixture content by roughly 30-50%, with a clear net reduction in files and separate runners. This is a direction, not permission to delete unique coverage. Do not meet it by minifying JSON, hiding code in generated outputs or moving files. Report actual totals and explain any retained large family briefly

## Phase 5: prove that the smaller system works

Required checks, using existing entry points and cached third-party dependencies where available:

1. Build D1, D2 and the D2 developer lifecycle target on the local supported Windows configuration
2. Run all five originally failing native behaviors to completion after repair/consolidation
3. Run the retained public XL contracts and relevant native smoke/replay checks. Run affected catalog/controller checks if their registrations changed. Separate inherited failures from new regressions with evidence
4. Run the three retained scenario groups against actual assets, including fresh-process save/restore. A shared receipt parser accepting fabricated output is not a gameplay run
5. Run one ordinary in-engine XL smoke with normal input: load a representative level, move without position assistance, fire, interact with reachable gameplay, save and reload. Prefer the supported Android automation path on an emulator if available; otherwise run the desktop game through existing input automation. Validate durable/introspected state, not just successful startup. Do not claim a full campaign playthrough
6. Run one scoped mixed-language formatting/lint invocation over changed files, inspect the diff and rebuild affected targets if formatting changed them

Use the prior review's `build-local.ps1` and `run-focused.ps1` as local reproduction aids, not portable product APIs. Their paths are machine-specific. In particular, `run-focused.ps1` records per-test exit codes in `results.json` but its process exit alone does not establish that every test passed; inspect the individual results

The four campaign readiness `load_failed` entries are a separate compatibility backlog. Do not require all campaigns to work to finish this cleanup, and do not reclassify their failures as supported. Likewise, do not implement a whole slow-motion/replay timeline during test trimming. Preserve the existing Omega replay rejection until that feature is implemented separately

If a required environment or asset is unavailable, keep the validation requirement visibly blocked, continue independent implementation, and report the exact missing prerequisite. Do not substitute a synthetic receipt, silently skip it, or declare full acceptance. Do not wait on unavailable infrastructure while other deletions/fixes remain possible

## Completion and handoff

The implementation is ready for review when:

- Redundant per-switch chains, fixtures, wrappers and duplicate combat matrices have actually been deleted or materially reduced
- The retained scenarios use shared existing execution paths, without an equivalent replacement framework
- The five original failing behaviors pass on the repaired native runner, or a precise unresolved defect is explicitly reported as incomplete
- Actual gameplay and save/reload evidence is present, with any platform limitations stated
- Changed registrations still reject omitted/skipped required checks
- The final report includes before/after line and file counts, changed test runtime if measurable, a short mapping from removed families to retained behaviors, and the exact check outcomes
- The local diff/patch is ready to review; original branches/stashes remain intact

Final response should lead with what was deleted, what now works, and what remains blocked. Do not lead with the number of tests added or passed. Separate later feature work and possible PR regrouping into a short follow-up list

Do not mark the execution goal complete merely because a plan was written or a reduction percentage was reached. Completion means the authorized implementation and its available required checks are finished; unresolved required validation must be reported honestly

## Execution progress

Completed locally. The source and patch audit matches candidate tree `d01c46fbe590d56cfb69ba63a1bbf7432e91a74f`.

- Deleted 77 Tulev1/guided-input files (34,020 lines); retained five shared scenario/data files (1,064 lines)
- Cleanup against the frozen tip: 110 files changed, 2,705 insertions and 34,902 deletions; net reduction 32,197 lines
- Repaired all five original native failures; final projectile cleanup also rebuilt and passed
- Ordinary-input smoke, live combat, progression, guided contact and cold restores pass with actual Pumo assets; assisted phases retain their qualification
- Windows D1: 89 passed / one existing skip; D2: 100 passed / one existing skip; developer lifecycle target builds
- Public XL: Windows 56/56; Linux 61/61, including GPU/audio; the final private lane plus ten additional public-only cases exercise the final Linux selection
- Full Linux private fast lane: all 113 required tests pass with unchanged timeouts, 16 format witnesses, three source levels, four navigation anchors and navigation smoke; optional camera addon unavailable
- Candidate Android APK builds; Neptune runtime passes 65/65, including rendering, save/restore, negative inputs and stock transition; restored device data hashes match
- Combined controller tests 45/45; artifact checks 6/6; scoped mixed-language formatting/lint passes
- Reviewed current cmake baseline integrates cleanly; forward/reverse application checks pass for both patch exports
- Original root branch, real indexes and three stashes preserved; no commits, pushes, merges or PR mutations

See [completion report](xl_test_trim_completion.md) for validation artifacts, retained coverage, limitations and integration instructions. No required local validation remains blocked. Remote CI and publication require a subsequent authorized integration action.
