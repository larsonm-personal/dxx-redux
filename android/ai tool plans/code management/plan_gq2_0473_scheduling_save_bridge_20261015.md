# Chunk 0473 scheduling, save and metadata bridge checkpoint, 2026-10-15

Diagnosis and plans only. Previous turn advanced native consumer checkpoint PASS133. This turn traces actual scheduling, quick-save slot entry, special-goal serialization, radial/preference delivery and native metadata bridge. Four later primary gates remain open; no canonical mutation, product edit or runtime acceptance.

## Scheduling corrects source-level timing assumptions

STEP_META_ACTION dispatches press in phase0, then on a subsequent automation tick releases/advances when phase1 delay satisfied. Even zero post_delay does not dispatch the following meta action in the same tick. advance_step only changes index/state, with no tail dispatch of next action. In event_process automation runs before event_poll; event_poll sends EVENT_IDLE when no other input events, and gamecntl consumes pending Guide-Bot commands there, returning after each. Spawn-before-goal is therefore normally separated by idle opportunities. Continuous other events/front-window changes can defer consumption; no receipt ties these two commands to completed application. Do not infer a proven reversal from the pending-slot priority alone.

Important correction to earlier source/native-plan wording: ordinary STEP_SET_DEBUG ends with immediate advance_step, and STEP_SELECT_RADIAL advances immediately after selection. Their supplied post_delay_ms values are parsed but these branches do not wait on them. The quick-save750ms and overlay500ms annotations are not actual waits in these handlers. Later waits/assertions and intervening ticks are what give engine work an opportunity. Do not carry forward the weaker characterization of750ms as an honored timing heuristic. No product change here; reconcile with existing automation/save test owners before root classification.

The quick-load waits for active mode/front/unpaused/released can already hold before load, particularly Enhanced after setting Enhanced. Special-goal13 checks after deliberately selecting Next supply a stronger conflicting-state intent, but exact Next dispatch and completed restore generation remain unacknowledged. This is incomplete test evidence, not proof load always fails or a request always reorders.

## Actual native save behavior retained

android_game_request quick_save/load sets pending meta flags. EVENT_IDLE eventually consumes them after Guide-Bot slots. Single-player save checks player/game admission and synchronously calls state_android_save_to_slot, then emits success/unavailable HUD message; load calls state_android_restore_slot and emits errors for missing/failure. Automation does not inspect those return outcomes or HUD completions.

Slot save prechecks and validates slot/description/multiplayer, drains older checkpoint work through state_android_save_to_path, writes .stage, checks native serializer result, validates game/kind, publishes, restores temporary metadata/thumbnail fields. Slot path additionally uses .tmp, validates it, calls selected-slot publisher and writes last-save-set after success. Preserve these current repairs. Selected-slot restore drains outstanding work, rejects invalid/missing slot/secret-level path and invokes ordinary restore without second confirmation. These wrappers do not prove all-file crash durability or every pause balance; whole pair publisher/native writer remains prior owner scope.

D2 runtime serialization explicitly includes Escort_special_goal, goal fields, buddy release identity, persistent message and clocks plus framed route/metadata state. Native GB_FIELD reader checks encoded signed-width consistency and only applies successful reads; bounded counters/text have additional limits. Normal runtime restore invokes same schema with apply enabled unless secret restore, and preserve-runtime rebuild returns without discarding it. Earlier current-preference-after-restore behavior remains deliberate; changing active routing may reset paths. Preserve distinct saved goal and current routing policy semantics.

Prior whole0471save_publication plan read and retained: BR0190 required transport/reset outcomes, GQR0277 completed Setup outcome, BR0235/GQR0236 coherent save generation, GQI0006 recovery; GQF0256/GQR0242 pause balance distinct. It documents existing .stage/checked close/publication fixes and remaining transaction/index/recovery boundaries. This is prior-plan evidence, not a fresh whole BR/quality read.

## Radial and preference authority

Native radial selection attaches JNI, invokes MainActivity, clears exceptions to failure and releases local references. MainActivity executes directly on UI or posts with a2s latch wait. TouchOverlay locates menu case-insensitively, selects first visible label containing requested text, dispatches binding. Default Guide entries bind Scram! to1010, Next to1041, Recall1045. Meta bindings emit press/release callbacks. This tests configured-visible-menu dispatch, not touch hit-testing or actual human radial geometry. Timeout does not cancel posted UI work.

MainActivity reads dxx_prefs and applies persistent-goal/routing defaults at startup and later settings reapplication. JNI forwards D2 setters, with no-op D1 branches. Exception swallowing here is existing boundary requiring owner reconciliation; script debug setters exercise native state independently of persisted launcher defaults.

## Metadata bridge preserves generation

Whole android_route_metadata.c read. Request captures current engine mission asset context JSON, engine/content game, mission/level/file sets/readiness and increments native request generation under progress lock. Callback arguments include that snapshot. MainActivity cancels previous job, launches computeMission on Dispatchers.IO and passes captured context and contentGame. Completion checks request generation before changing CPU duty; native finished/progress notifications also use generation. Native finished rejects obsolete generation, updates progress state and then notifies level_metadata background result. Retain these repairs; do not claim no generation guard.

Completion publication occurs after native progress unlock, and assets/context validation, worker extraction and live adoption still require actual computeMission/worker/native metadata readers. Captured context is stronger than rereading only mutable launcher paths, but not alone immutable loaded-byte identity or reader lifetime. Native request JNI allocation/exception behavior remains an existing owner boundary, not newly executed acceptance.

## Evidence and next

Exact read contexts and identities are packed in gq2-scheduling-save-progress-0473-20261015.json. No full serializer/event-loop/metadata-worker claim. guidebot_save_io1-160 plus160-161 reaches whole161 across contexts; event300-355 request reaches EOF351, actual300-351 credited. Prior0471plan requested first65 reached whole EOF. Failed guessed symbols/absent root and wildcard locators earn no source credit.

Next bounded metadata computeMission/worker/native active generation and actual run_test/launcher handoff/fixture/outcome/deadline boundaries; then reconcile whole material owners/prior plans, direct unpooled disposition and publication. Do not indefinitely expand unrelated serializer/AI scope. Sourceclosed/fourlateropen, canonical0473TODO;472DONE155TODO627 unchanged. No newfinding/remediation/status/rating/runtime/originalsaving. Ledger unchanged and prior0472Driveproofhistorical. Broad goal active, diagnosis/docs/inlineevidence/retention only.
