# Chunk 0477 objective and profile preview consumers, 2026-10-15

Selected actual boundaries following runner, save/load and lifecycle/import packages. Whole LevelPreviewRequestStore plus explicit source ranges. Four later gates remain open pending consolidated replay/owners. No code/test/helper execution, payload access or new runtime acceptance.

## Seeded profiling is launch, not an output oracle

LauncherScriptExecutor565-639 uses label for diagnostics/selection report, not source lookup. Actual profile mod_file uneasy4_render_profile.zip is resolved from active set ModManager; MissionZip inspection produces mission targets or falls back to generic ZIP target. Requested game filters targets. Analyzer must return ok and valid nonblank level rows; optional level_num filters candidates. Random(seed) chooses within the enumerated candidate order, so seed alone is not immutable source/candidate identity.

Request creation runs on IO, writes smoke selection including source/level/request, increments currentStep and starts LevelPreviewActivity after prepareForLevelPreviewLaunch. A successful Activity start is not later first-frame/render validation or acknowledged native completion. Keep profile purpose and existing launch/handoff/preview ownership; do not invent absent assertions.

Whole LevelPreviewRequestStore validates supported game, successful scanned row and membership in known levels. It creates owned cache directory and isolated runtime-write, canonicalizes existing source/archive/base/extra inputs, writes typed request atomically and deletes owned work directory on creation exception. Launch validator checks owned request location/name/schema, request ID equals directory, expected game, isolated write child and existing data directory. Preserve actual admission; canonical path/existence is not full immutable byte-generation verification.

LevelMetadata1772-1859 builds typed preview request with prepared source/data/mission/HOG/support archive fields and level number/file/secret flag, normal/secret arrays and provenance dates. Actual prepareTarget internals were not reread here; existing immutable scan/source/staging plans govern the remaining generation contract. No second JVM game-format parser.

## Open automap metadata adoption

automap_metadata_begin records displayed route revision and clears readiness text; update_route runs from actual paired EVENT_WINDOW_DRAW. It disables adoption during input demo record/replay and multiplayer. Useful/complete background progress with incomplete nonfailed readiness polls pending cache at approximately1second intervals, then increments refresh count only when route revision changes. Count is cumulative static state rather than a per-assertion difference; don't turn >0 into proof of every requested generation.

secretarea4319-4427 rejects absent canonical snapshot/terminal failed background result, loads current keyed route cache, bounds adoption retries after reported publication and marks failed with diagnostics if it cannot adopt. Improved plan applies canonical state/summary, classifies complete/next-ready/partial/failed and increments route revision. Complete background notification is separate from successful cache adoption. The fixture's D2 refresh/no-adoption-failures predicates intentionally cover this boundary.

Cache load3325-3396 derives current generation/topology key and filename, requires exact record size, reads and decodes against key, counts misses/rejections/io errors and rejects partial route for isolated analyzer that must continue planning. Preserve current key/size/partial-plan guards. Selected read uses short-circuit read && close && decode; do not claim unconditional close on failed read, and reconcile with existing checked I/O/resource owners rather than create a duplicate speculative root.

android_route_metadata1-50/180-247 keeps atomic progress and request generation, resets on invalidation and accepts only current-generation finished/progress callbacks under progress lock. Finished success makes calculating useful, failure only changes calculating, then notifies background result. Native policy owns progress clamping/state transition; current fixture does not prove all possible invalid/stale callbacks.

## Readiness text, candidates and actual pixels

Shared readiness848-955 clamps displayed incomplete progress0..999, hides note for off/complete (or usable NEXT), labels partial usable versus calculating versus failed, and draws text/bar. Thus cold D2 note/progress<1000 is a meaningful intermediate-state assertion; D1 may already be ready and is correctly excluded from that cold predicate.

draw_labels resets secret/objective counts separately, so secret labels remain independent of route readiness. Objective visible_step_count increments for selected non-start route steps before projection; collect candidate count increments before behind/overflow/project checks. The script's positive steps/candidates show plan availability, not on-screen labels/pixels. It does not assert projected count or framebuffer content for this fixture. Preserve accurate distinction from other GPU/preview tests.

Secret drawable_label_count in introspection is logical found-or-reveal eligibility. draw_secret_labels requires enabled/reveal and records candidate before actual draw_text_label result increments projected. Paired automap draw calls supply shared connectors/labels/readiness and independent secret rendering. Current test checks drawable/candidate/visible-edge fields, not exact label positions/pixel parity or complete objective geometry. Existing renderer/projection acceptance remains separate.

## Consolidate next

Retain seven distinct profile, abort, shared save/load, SDK lifecycle, Anniversary import, objective readiness and missing-pilot autosave scenarios. Preserve stronger current repeated-restore simulation/input check, pause reason2, paired D1/D2 cold-versus-ready behavior and shareware save path. No fixture merge, arbitrary delays or broad inherited refactor.

Reconcile whole material owners/prior plans for command-result propagation, immutable source/executed script identity, missing-pilot deletion outcome, snapshot/request generation, complete-attempt budget, checked I/O/publication/independent cleanup and engine-thread/native-format ownership. Current native codecs/transactions/backup/descriptor repairs, pause sessions, owned preview directories, typed requests and route-cache admission must remain.

No new finding/remediation/status/rating/runtime/original savings. One combined native output clipped and was recovered in bounded reads before credit; guessed LevelMetadataAnalyzer.kt locator corrected to LevelMetadata.kt. No whole parser/progress-policy/prepareTarget implementation credit from locators.
