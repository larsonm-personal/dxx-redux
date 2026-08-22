# Guide-Bot convergent current-state design audit

- [x] Inventory current objective, completion, routing, and accessibility state authorities.
- [x] Identify correctness dependencies on notification order and retained history.
- [x] Define a deterministic decision model from static level semantics plus a live world snapshot.
- [x] Separate objective selection, route planning, and local movement recovery responsibilities.
- [x] Propose multiplayer, automap, migration, diagnostics, and test strategies.
- [x] Summarize hard limits and recommended next implementation slice.

## Findings

- The full shared route planner is already close to a convergent state function. It builds a fresh snapshot containing the start pose, key mask, control-center state, current walls, trigger flags, objects, and automap state, then solves dependencies from that snapshot.
- The canonical live-reuse path is not convergent. It advances a cached level-start step sequence using both mutable wall-state predicates and `Level_metadata_completed_canonical_steps`, which is an event-history overlay.
- Escort notifications currently combine cache invalidation with objective completion. A matching event marks the active step complete, so correctness depends on which goal was active when the notification arrived. The trigger-specific workaround also records out-of-order trigger history.
- Objective completion based on whether a linked wall is currently open can regress when a door closes. Remembered completion avoids that regression but introduces the event-history dependency the proposed design should remove.
- The planner snapshot starts at the Guide-Bot object when available, but uses the player ship radius rather than the Guide-Bot radius. The semantic planner and classic AI path executor also use related but separate accessibility rules and separate path searches.
- The automap and Guide-Bot already consume the same live plan summary. A single convergent live decision can therefore fix both consumers together.

## Recommended model

Treat objective completion operationally: an action is complete when it is no longer required from the current Guide-Bot pose to reach the requested endpoint. Do not ask whether a switch was fired in the past. Ask what currently blocks the best route to the endpoint and which currently usable action removes that blocker.

The decision should be a pure function of:

1. Immutable level semantics: segment topology, trigger sources and effects, exits, and object roles.
2. A fresh synchronized world snapshot: Guide-Bot pose and radius, player or team capabilities, current wall and door state, trigger enabled state, live objects, reactor state, and explored segments when relevant.
3. An explicit target policy such as end of level or unexplored.

The result should contain a stable decision identity, selected action or objective, target pose or segment, and the proven reachable path prefix. Candidate ties must use stable identifiers after semantic cost and path distance.

Events may invalidate a cached answer for performance, but must not provide semantic facts. A periodic snapshot fingerprint check provides recovery if an event is missed.

## Responsibility boundaries

- Semantic decision: pure and history-free. It selects the currently required player action or terminal target.
- Route proof: uses the same actor profile and passability oracle as execution, or is continuously checked against it.
- Movement controller: may retain short-lived steering and stall recovery state, but it must not change which objective is considered complete. Replanning always starts from the Guide-Bot's current pose.
- Presentation: automap and Guide-Bot labels consume the exact same live decision.
- Multiplayer: ownership controls who moves the Guide-Bot, not who owns objective truth. A new owner recomputes from synchronized world state.

## Hard limit

If two engine states are byte-for-byte equivalent but the intended answer differs because of a past event, no state-only algorithm can distinguish them. Prefer operational reachability semantics. If a rare mission truly requires remembering an irreversible narrative fact that has no present-world consequence, represent it as an authoritative synchronized and saved world-state field, not as an escort notification journal.

## Efficiency architecture

The convergent design must not mean running the full level analyzer on every door animation or polling every frame. Split the work into three layers.

### Level-scoped preparation

Build or load this once per level and actor profile:

- Immutable segment topology, side clearance, wall and trigger relationships, stable target identities, and trigger effect links.
- A compact action graph whose edges mean that a key, trigger, hidden door, blastable wall, boss, or reactor can enable travel between regions.
- Candidate activation poses and visibility samples for switches. Preserve several ranked candidates rather than only the canonical winner so live planning can reject a newly blocked candidate without starting a global visibility search.
- Static or topology-keyed distance tables between progression regions, action sources, and terminal targets. Segment-level tables are acceptable if memory measurements stay small; otherwise retain only distances among action graph nodes.
- Persistent route and visibility cache records keyed by game variant, level topology hash, behavior version, and actor profile.

This preparation may use the existing background analysis and persistent cache. It is allowed to be expensive because it is amortized, resumable, and never required in one gameplay frame.

### Live decision certification

When a decision may be stale:

1. Read the current Guide-Bot segment and actor profile.
2. Read only the mutable domains needed by the action graph: current wall passability and lock state, trigger enabled state, team keys, relevant live objects, reactor state, and automap state for unexplored mode.
3. Compute current reachable progression regions using a preallocated queue and compact edge-state arrays.
4. Check whether the terminal target is reachable. If not, rank the reachable frontier actions using precomputed distances and stable identifiers.
5. Validate only the selected action's activation pose and path prefix against current collision state.
6. Fall back to the bounded shared dependency solver only when the compact certificate cannot decide.

This path must avoid full-level visibility sampling, heap allocation, file I/O, and rebuilding immutable vectors. Reuse buffers sized at level load and use generation stamps instead of clearing maximum-sized arrays.

### Movement execution

Classic movement may retain its ordinary path and stall state. Semantic replanning happens only when the selected decision's validity certificate changes, the Guide-Bot crosses into a different progression region, the requested target mode changes, or a periodic audit discovers a missed invalidation. Ordinary movement within the same region does not trigger semantic analysis.

## Cache correctness rules

- Caches memoize a pure result. Cache warmth must never select a different objective.
- Every cached decision stores the input fingerprint domains it actually depended on, its selected action identity, and a small validity certificate.
- A cache hit is accepted only after checking the certificate against current authoritative state. Otherwise it is a miss.
- Events may dirty fingerprint domains but may not set completion facts.
- A missed event may delay recomputation until the periodic audit, but cannot permanently preserve a wrong objective.
- An exhausted work budget returns `calculating` or keeps the last still-valid decision while the same keyed job resumes. It must not substitute a cheaper but semantically different objective.
- Background results are installed only if their input fingerprint still matches the current snapshot.
- Deterministic tie breakers are part of the planner contract and cache behavior version.

## Initial performance constraints

These are implementation gates to measure and tune on the slowest supported Android profile, not assumptions that the current code already satisfies:

- No new FVI visibility rays on the game thread during the common live-reuse path.
- No dynamic allocation, persistent-cache I/O, or immutable topology rebuild in the gameplay polling path.
- No semantic polling every rendered frame. Use narrow action-certificate checks at the existing four-Hz completion cadence and a staggered broad audit no more often than once per second unless profiling supports a shorter interval.
- Common decision reuse should be effectively constant time: inspect the active certificate and a small set of fingerprint generations.
- A compact live replan should be linear in the action graph plus currently reachable region edges, not in the number of visibility samples.
- Any fallback that can exceed the per-frame work slice must be resumable across frames or run through the existing background preparation path.
- Add counters for snapshot fields read, regions visited, action edges evaluated, FVI calls, allocations, cache hits, certificate failures, fallback work, and maximum work in one frame.
- Establish memory caps after measuring current records. Prefer bounded per-level caches and retain persistent visibility samples on disk instead of allowing an unbounded in-memory cache.

## Detailed implementation plan

### Step 0: Baseline cost and define the decision contract

- [x] Implementation complete.
- [x] Define `guidebot_route_decision` with target policy, decision status, objective kind and stable identity, activation kind, target segment or pose, reachable path terminal, input fingerprints, and a validity certificate.
- [x] Define equality separately for semantic identity, guidance target, and route proof so proof churn does not look like objective or movement-target churn. Execution-path equality remains pending until the live solver returns a full route proof rather than only the current terminal summary.
- [x] State explicitly which input fields may influence each output field. Exclude active-goal history, notification generations, completion bits, avoidance history, wall-clock time, cache readiness, and simulation RNG from semantic identity.
- [x] Add stable tie rules for equal action cost and distance: objective kind priority, then trigger, wall, object, segment, and side identity.
- [x] Instrument canonical reuse and full end-of-level live fallback counts and Android elapsed time. Existing snapshot-phase, FVI, and classic path counters remain separate and are exposed alongside the new measurements.
- [x] Capture cold and warm measurements on Counterstrike level 20, Obsidian level 2, KCXF2 switch and hidden-door cases, one large mission level, and a slow Android emulator or device profile.
- [x] Record median, 95th percentile, maximum main-thread time, FVI calls, allocations, and peak cache memory. Use these measurements to set numeric budgets before behavior cutover.

Efficiency result: this step prevents a correct but too-expensive design from becoming authoritative and provides evidence for action-graph and memory sizing choices.

Step 0 implementation notes:

- Added a C ABI decision contract and pure projection, hash, semantic-equality, and guidance-equality functions in `guidebot_route_decision.h/.c`.
- The semantic hash identifies the normalized requested objective. The guidance hash adds the activation pose or partial frontier, while the route terminal remains certificate or proof data. The decision hash combines the guidance result with relevant snapshot fingerprints.
- Added live decision and reuse or fallback timing diagnostics to Android introspection. The projection is diagnostic-only and does not change Guide-Bot or automap behavior.
- Added a focused host test covering equivalent snapshots, input-only changes, semantic versus guidance changes, and terminal statuses.
- Scoped code quality passed. D1 and D2 Windows builds passed. The focused contract test passed under MSVC with `/W4 /WX`. Android debug assembly passed for arm64-v8a, armeabi-v7a, and x86_64.
- The maintained Counterstrike level 20 Guide-Bot integration passed unchanged. Its warm run performed two canonical live-reuse attempts and two hits in 102 microseconds total, with a 96 microsecond maximum, zero full live fallbacks, and one visibility FVI miss. The later final baseline below supersedes this initial sample.
- Added one shared lexicographic objective-identity comparator. Missing identifiers sort last, and exact planner ties now resolve by stable segment or objective identity rather than heap or input-vector order. Reversed trigger, exit, and key candidate tests produce the same selection.
- Added a fixed 32-sample live-reuse timing window. Introspection computes median and p95 only on request; the game-thread path performs one bounded array write and no allocation. Visibility-cache diagnostics now report current and resize-peak allocated bytes.
- The final Counterstrike level 20 run recorded eight reuse samples: 29 microseconds median, 377 microseconds p95 and maximum, 1,224 maximum evaluated edges, zero full fallbacks, 4,587,520 current visibility-cache bytes, and a 5,046,272-byte peak. The integration gate allows 4 milliseconds, 3,000 edges, 19 MB current cache, and 26 MB transient peak, leaving substantial margin for slower hardware.
- The certifier uses only caller-owned fixed workspaces and fixed-size state copies, so its live-reuse allocation count is structurally zero. The shadow full planner remains opt-in and outside this budget.
- The nine-level RelWithDebInfo metadata benchmark passed at 0.936 aggregate CPU seconds versus the retained 0.869-second optimized baseline, a 7.7 percent difference below the existing 10 percent and 0.25-second regression thresholds. Stable tie-breaking changed only Obsidian level 11's equal-cost firing segment from 118 to 117; the reviewed benchmark digest now records that deterministic lower-segment choice.
- A separate three-run Obsidian level 2 check recorded 0.167 seconds cold and 0.127 to 0.144 seconds warm. The x86_64 Android emulator covered Counterstrike level 20 plus KCXF2 blastable-wall, hidden-door, level 5 switch, and 597-segment level 6 cases. This is the available slow device profile; the fixed work and memory gates remain conservative for older physical phones.

### Step 1: Add a shadow live-decision API

- [x] Add a bounded no-allocation current-state certifier over the prepared canonical action sequence, publish its decision, and validate it against level 20 before authoritative use.
- [x] Reuse immutable prepared topology and action metadata without rebuilding topology vectors during live certification.
- [x] Correct the actor profile to use the actual Guide-Bot radius and companion door or buddy-proof behavior.
- [x] Add an engine-neutral C entry point that accepts immutable preparation, a read-only live scan view, actor start, and target policy inputs and returns certified live route state for one `guidebot_route_decision` projection.
- [x] Initially implement it using the existing full shared planner to establish semantics, but call it only in shadow fixtures and opt-in diagnostics.
- [x] Add a second fast path that rebases the precomputed canonical actions against current reachability and produces the live decision.
- [x] Compare the authoritative fast result with the opt-in full-planner shadow in the level 20 integration fixture and cover mismatch-reason fallback classification in host tests.
- [x] Preallocate reachability, queue, live-state, plan-summary, certificate, and decision storage. Avoid `std::vector` reconstruction in repeated live calls.
- [x] Make expensive visibility validation lazy. Validate only the chosen switch candidate and try precomputed alternatives before requesting new analysis.

Efficiency gate: after warm preparation, ordinary shadow decisions perform no global FVI scan and allocate no memory on the game thread.

Step 1 completion note: the authoritative certifier has no visibility callback and therefore performs zero FVI validation. It rejects unusable or unreachable actions while walking the prepared action sequence and selects a reachable prepared alternative before the caller may request full analysis. A focused regression disables the nominal switch and verifies selection of the reachable prepared reactor alternative in the same bounded pass.

### Step 2: Add decision and input fingerprints

- [x] Split the world fingerprint into start, progression, navigation, trigger, progression-object, automap, topology, actor-profile, and target-policy inputs.
- [x] Build a dependency mask for each cached decision so an unrelated object or automap change does not invalidate an end-of-level switch decision.
- [x] Compute the semantic decision hash only from normalized input identity and output identity. Do not include pointers, timing, cache counters, notification history, or current AI path storage.
- [x] Store a validity certificate containing the selected source identity, target object identity, and prepared path frontier. Source usability and completion are rechecked directly from live state at four Hz.
- [x] Expose hashes, dependency masks, certificate results, certifier or fallback provenance, and work counters through introspection.
- [x] In host tests, recompute from equivalent final states reached through different event orders and assert byte-identical normalized certifier results.

Efficiency gate: the common poll reads only generations and the small certificate. Full domain fingerprints are rebuilt only when a domain is dirty or during the staggered audit.

Step 2 implementation notes:

- Decision fingerprints now project only the domains required by the selected objective type. Trigger decisions depend on trigger state but not unrelated progression objects or automap state; key, reactor, and boss decisions depend on progression objects; unexplored decisions depend on automap state.
- Unresolved non-complete decisions conservatively retain both trigger and progression-object dependencies until the objective is known. Host contract tests verify both the dependency masks and that excluded-domain mutations leave the input and decision hashes unchanged.
- Navigator radius now has a separate actor-profile fingerprint and generation. Every projected route decision depends on it, so changing Guide-Bot geometry changes input and decision identity even when the world snapshot and selected semantic objective remain equal. The domain is one scalar read and adds no level scan.

### Step 3: Shadow-compare against canonical reuse

- [x] Run the current canonical answer and new live answer from the same Guide-Bot state without changing movement or automap output.
- [x] Log only mismatches or sampled performance summaries. Do not write per-frame success logs on Android.
- [x] Classify mismatches as stale completion history, reversible wall state, out-of-order action, actor accessibility, target ranking, activation visibility, cache readiness, or planner defect.
- [x] For each mismatch, serialize a compact normalized fixture containing only an immutable game, level, and topology reference plus the live state required to reproduce it on the host.
- [x] Exercise all existing Guide-Bot integration scenarios and the route corpus. The maintained Counterstrike level 20 fixture enables the shadow after the out-of-order switch activation and requires the full live result to match canonical reuse.
- [x] Require the compact fast semantic and guidance decisions to equal the full live decisions. Track route-proof differences separately until the compact solver returns a full route proof.

Efficiency gate: shadow mode is test-only or sampled on debug builds. Release gameplay never pays for two planners.

Step 3 implementation notes:

- Added an opt-in full-planner shadow run for end-of-level live rescans. It is disabled by default and never replaces the canonical answer.
- The shadow saves and restores the normal FVI count, budget-exhausted flag, and cancellation flag, so diagnostic work does not consume or alter the authoritative planner budget.
- Added semantic, guidance, and availability mismatch counters, FVI and elapsed-time measurements, normalized primary and shadow decisions, and Android introspection fields.
- Added pure mismatch-classification coverage. The focused level 20 integration and performance validation passed, followed by the final mission-corpus and maintained-scenario coverage listed below.
- The first Counterstrike level 20 shadow run converged semantically and on guidance for trigger 12 in segment 44, but exposed a route-proof mismatch in the reachable path terminal after reversible wall state changed. The opt-in fresh solve cost 9,570 FVI calls and about 70.6 milliseconds on the x86_64 emulator, confirming that the full solver is a correctness oracle rather than a viable common gameplay path.
- Mismatch capture now builds a debug-only normalized replay fixture. It references immutable geometry by game, level, and topology hash, then records the current start, navigator radius, progression state, per-side navigation and automap state, effective wall and trigger state, and indexed progression-object state. The release path performs no capture or allocation.
- The host replay test rejects a mismatched topology and proves that applying the captured mutable state to matching immutable topology produces the same route plan. The maintained level 20 fixture validates a 63 KB normalized replay artifact on-device. Its first rerun exposed a pre-existing timing-sensitive certificate-counter assertion in the combined shadow scenario, so that counter assertion remains in the dedicated narrow-certificate scenario rather than the shadow artifact scenario.
- Final coverage passed for Counterstrike level 20 shadow, narrow-certificate, and dropped-notification cases; Obsidian level 6 switch and grate; KCXF2 blastable wall, hidden door, switch, key carrier, and objective overlays; unexplored guidance; solo save/load; co-op ownership, disconnect adoption, and slot-remapped restore; all 15 input demos; and all 1,509 reviewed mission routes.

### Step 4: Add event-order permutation tests

- [x] Create pure planner tests that construct one final snapshot through multiple permutations of trigger, wall, key, object, and reactor mutations. The small fixture represents reactor state through its control-center target rather than a second reactor-specific field.
- [x] Assert identical semantic decision, guidance target, route certificate, and decision hash for identical final snapshots. The certificate is currently unchecked by design, so equality covers its normalized unchecked state.
- [x] Include activate-before-selected, activate-after-selected, repeated trigger, door-open-then-closed, key-carrier destruction, save/load, and owner-migration variants.
- [x] Randomize mutation order in a bounded property test while retaining a stable seed and minimized failure fixture.
- [x] Assert warm-cache, cold-cache, precomputed, and forced-fallback results converge to the same final decision.
- [x] Add a performance assertion based on planner work units rather than host wall-clock time so CI remains stable.

Efficiency gate: permutation count and fuzzing stay in host tests. Device integration retains only representative high-level sequences.

Step 4 implementation notes:

- Added 32 stable-seed mutation permutations to the pure route snapshot test. Every permutation begins with a transient opening state and ends with the same wall, trigger, key, object, automap, and navigation state.
- Each result is independently snapshotted and fully planned, then compared across all fingerprint domains plus semantic, guidance, input, and decision hashes.
- Added explicit activate-before-selection and select-before-activation histories. The latter projects guidance before applying the trigger twice, cycling the door open then closed, acquiring the carried key, and destroying its carrier. Both histories and all randomized orders finish with identical fingerprints and decisions.
- Added a production-codec convergence fixture that compares a cold full plan, direct prepared certification, decoded persistent-cache certification, and an explicit full-planner fallback from one immutable scan view. All four paths must project the same semantic and guidance decisions.
- Added deterministic search-work assertions. Repeated planning must report identical search, visit, considered-edge, and evaluated-edge counts and remain below fixed fixture-specific work ceilings.
- Restored the parent `BUILD_TESTING` value after configuring TagLib so project test targets are no longer silently disabled by dependency setup.
- Extended the maintained solo quick-save and quick-load fixture to require a reconstructed end-of-level decision with a valid certificate after restore. Extended the co-op slot-remap fixture to require the new owner to publish the restored unexplored target policy from a fresh metadata rescan.
- The event-order history checks remain bounded host-only work. The focused snapshot test completes in about 0.1 seconds on the Windows host and adds no game-thread code or device cost.

### Step 5: Add dropped-notification and audit recovery tests

- [x] Add a test switch that suppresses Escort route notifications while ordinary game state mutations still occur.
- [x] Verify the narrow published-decision certificate poll notices completion when the selected action changes state, including while the Guide-Bot is idle.
- [x] Verify a staggered broad audit notices a current-state change without a notification. The active level 20 switch case is covered on-device, while the event-order host fixtures cover out-of-order final states.
- [x] Prove that the audit reads state and dirties domains but never marks an objective complete. Audit and notification pending masks are tracked separately, and the legacy completion ledger has been removed.
- [x] Test that repeated audits with unchanged state cause no semantic solve, FVI work, allocation, or path replacement.
- [x] Test that the audit is divided across frames or domains if a full object and wall scan exceeds the measured device budget.
- [x] Assert bounded convergence time, initially one broad-audit interval plus one planner work slice.

Efficiency gate: common four-Hz checks are proportional to the active certificate. The broader safety net is staggered and stops early when fingerprints match.

Step 5 implementation notes:

- Added allocation-free domain fingerprint functions for progression, navigation, triggers, progression-relevant objects, and automap. Full-snapshot parity tests cover the shared domains.
- The Guide-Bot checks one domain per existing four-Hz completion tick, so a five-domain cycle completes in at most 1.25 seconds. End-of-level mode skips automap work.
- Progression-object auditing ignores ordinary robots, the player, and the Guide-Bot, preventing movement from creating false dirtiness. No audit path performs FVI work or file I/O.
- Added counters for checks, discoveries, total work units, maximum work in one slice, and pending audit domains. The level 20 dropped-notification fixture converged from trigger 18 to trigger 12 via `state_audit`; its largest slice was 2,999 simple side or wall work units.
- Added a narrow certificate over the currently published objective at the existing four-Hz cadence, even while the Guide-Bot is idle. The level 20 notification-free trigger helper converged from trigger 18 to trigger 12 with one recorded certificate failure; the check read three linked walls at most. The test asserts the durable failure counter rather than the transient last-replan reason, which a later broad audit may legitimately replace.
- Added an introspection-only counter reset. A proposed level 20 quiet-window assertion was removed after runtime evidence showed the active bot continued crossing genuinely accessibility-changing triggered doors, so that scene was not an unchanged-world fixture. Deterministic host coverage now verifies that raw flyable animation on an ordinary wall-free connection leaves the shared certifier-access fingerprint unchanged. The audit path remains allocation-free fingerprint work and performs no FVI calls.
- Corrected that host accessibility fixture so the wall-free connection is actually ordinary: its reverse side no longer carries the fixture's hard block or exit marker. This makes the test isolate animation-only flyability changes instead of accidentally comparing two different authoritative accessibility states.
- The dropped-notification fixture asserts durable audit-discovery and work counters instead of the transient last-replan reason, which asynchronous cache publication may legitimately replace after the audit has already converged.

### Current implementation checkpoint

- [x] Pure decision-contract tests pass on the Windows host, including mismatch classification and release-build-safe assertions.
- [x] Snapshot tests pass with 32 stable-seed event-order permutations and lightweight/full domain-hash parity checks.
- [x] The maintained Counterstrike level 20 shadow fixture passes and records the expected semantic and guidance match plus reversible-wall route-proof mismatch.
- [x] The Counterstrike level 20 dropped-notification fixture passes and converges through the bounded state audit.
- [x] The Counterstrike level 20 narrow-certificate fixture passes with route notifications suppressed and advances the published decision from trigger 18 to trigger 12 while the Guide-Bot is idle.
- [x] D1 and D2 Windows builds pass.
- [x] Android debug assembly passes for arm64-v8a, armeabi-v7a, and x86_64.
- [x] The no-allocation current-state certifier is authoritative for end-of-level live reuse and performs no FVI, allocation, cache I/O, or immutable topology rebuild.
- [x] On Counterstrike level 20, the authoritative certifier reached trigger 12 in segment 44 with `certifier` provenance, zero full live fallbacks, 211 visited segments, and no more than 1,266 evaluated edges on the x86_64 emulator. The latest dependency-mask and live-requirement run evaluated 1,224 edges and passed all 36 fixture assertions.
- [x] Solo save/load reconstructs a valid end-of-level decision from the restored world state.
- [x] A slot-remapped co-op restore reached the new-owner route assertions. The first run restored owner slot 1 and failed only because the fixture incorrectly expected an end-of-level certificate for unexplored mode; a subsequent rerun hit the existing no-owner restore flake before route validation.
- [x] Broader mission corpus and final performance baselines pass.

### Step 6: Cut Guide-Bot and automap consumers over together

- [x] Stop automap entry and rendering updates from replacing the Guide-Bot-start decision with a player-start rescan.
- [x] Add a feature flag for authoritative live decisions while retaining the full current-state planner for rollback diagnostics.
- [x] Have both Guide-Bot goal selection and automap `next objective` read the same published decision object and live route summary.
- [x] Publish decisions on the game thread at a game-tick boundary. Automap entry and rendering invoke no planning.
- [x] Retain the existing Guide-Bot path when semantic identity and guidance target are unchanged and the prior narrow certificate still validates.
- [x] Rebuild the classic movement path only when a command adopts a changed target, an invalidation causes replanning, or existing stall recovery requests it.
- [x] If a new decision is still calculating, retain the previous decision only while its certificate remains valid. Otherwise stop pursuing the invalid target and return to the player path until current guidance is available.
- [x] In co-op, let only the owner schedule planning and movement, but make the decision entirely reproducible from synchronized world state. Recompute immediately after ownership migration.

Efficiency gate: opening the automap has zero planner cost, unchanged decisions create zero classic path requests, and multiplayer nonowners do no duplicate live planning.

Step 6 implementation notes:

- Added a pure retain, replace, or stop adoption policy to the shared decision contract. Retention requires current certificate validity plus semantic and guidance equality; changed ready guidance replaces the path, and complete, failed, unavailable, or invalid calculating guidance stops the stale path.
- Event handling now validates the narrow certificate even when a notification is pending. This allows a redundant event to retain the existing path after a current-state rescan while a real objective change still replaces it.
- Added retained, replaced, and invalid-stopped counters. In the maintained Counterstrike level 20 fixture, a redundant trigger 12 notification retained the active path twice during polling, with zero replacements and zero invalid stops. The live decision remained trigger 12 in segment 44 with `certifier` provenance and zero full-planner fallbacks.
- Normal save restore now clears transient Guide-Bot target and movement fields, preserves only the saved target policy, and immediately republishes guidance from restored world state on the local authority. Replay checkpoints retain their exact recorded AI state, and co-op nonowners return before planner work.
- The existing owner-handoff path already clears local navigation, marks route state dirty, and immediately refreshes only on the new owner. The slot-remap fixture now checks target policy 1 plus at least one owner-local rescan; nonowners still do no duplicate live planning.
- Added an enabled-by-default authoritative-certifier flag exposed through automation and introspection. Disabling it bypasses compact reuse and routes the next decision through the existing full current-state planner; it does not restore notification history or completion-ledger semantics.
- Scoped formatting and lint passed. Focused decision, snapshot, certifier, metadata-scan, and route-cache host tests passed. D1 and D2 Windows builds and Android debug assembly for arm64-v8a, armeabi-v7a, and x86_64 passed.
- The maintained Counterstrike level 20 dropped-notification integration passed after advancing from trigger 18 to trigger 12 through the broad audit with notifications and narrow certificate checks suppressed. Maximum audit work remained 2,999 units.

### Step 7: Make notifications invalidation-only

- [x] Remove notification-to-completion translation after the current-state certifier became authoritative.
- [x] Change wall, trigger, object, reactor, key, and automap notifications to mark relevant state dirty and optionally enqueue a bounded decision check.
- [x] Remove calls that translate a notification into active or out-of-order objective completion.
- [x] Coalesce repeated notifications per domain and process them once at the next allowed planner slice.
- [x] Prioritize the selected objective's narrow certificate above unrelated broad-audit domains so completion is detected within one four-Hz tick.
- [x] Keep the periodic audit enabled in release builds at a low staggered cadence so missed hooks cannot create permanent stale state while a goal is active.
- [x] Add counters for notifications coalesced, audit-only discoveries, redundant dirty domains, and time from authoritative state change to published decision.

Efficiency gate: event storms such as door animation updates cause at most one certificate check or replan per cadence, not one per notification.

Step 7 implementation notes:

- Dirty-event enqueueing now records notifications folded into an already-pending mask, redundant event-domain bits, and discoveries found only by the staggered audit. The bookkeeping is fixed-size integer state and adds no allocation or planner call.
- The first relevant notification records the engine fixed-point tick. Successful publication records last and maximum notification-to-publication latency plus a sample count; repeated notifications retain the earliest outstanding timestamp.
- The maintained level 20 fixture resets these efficiency counters after a complete warm audit cycle, proves the next unchanged cycle is inert, then verifies that a redundant trigger notification publishes within the measured path and contributes one latency sample without replacing or stopping the path.

### Step 8: Remove the completion ledger and obsolete reuse semantics

- [x] Delete `Level_metadata_completed_canonical_steps` and its getters.
- [x] Delete pending-event consumption that marks the active objective completed.
- [x] Reduce canonical route reuse to immutable prepared actions, path hints, current reachability, and live world predicates. The first pending step is recomputed from current state on every certification.
- [x] Remove completion predicates that infer historical trigger completion from a currently open linked wall. Keep current wall passability as ordinary live route state.
- [x] Remove obsolete completion introspection and replace it with decision fingerprints, certificate state, path proof fields, per-decision provenance, and certifier work counters.
- [x] Re-run save/load, co-op host migration, demos, Windows D1 and D2, Android ABIs, route corpus, and all maintained Guide-Bot integration tests.
- [x] Compare final warm gameplay costs against Step 0 and reject the cutover if common-path CPU, allocations, FVI work, or memory exceed the agreed budgets.

Efficiency result: the final system retains precomputed semantic and visibility work while eliminating historical completion bookkeeping and repeated full live analysis.

Step 8 implementation notes:

- Replaced the historical-looking step-completed predicate with an operational `step_required_by_world_state` predicate. The certifier now asks only whether the prepared action is still necessary in the current world.
- A multi-link trigger remains required while any linked wall still blocks the route. An unlock trigger remains required while any linked wall is still locked; ordinary door animation and passability remain live navigation state rather than evidence that an action happened in the past.
- Focused metadata, certifier, decision-contract, and snapshot tests pass. The maintained Counterstrike level 20 runtime fixture also passes with route notifications suppressed, one expected certificate failure, a maximum of three narrow-certificate work units, and zero full-planner fallbacks.
- Final warm Counterstrike level 20 certification measured 29 microseconds median and 377 microseconds p95 and maximum, with zero allocations, zero FVI calls, zero full fallbacks, and 1,224 maximum evaluated edges. Current and peak visibility-cache memory were 4.59 MB and 5.05 MB. The nine-level metadata benchmark remained within its retained regression policy, so the cutover stays enabled.
- Final validation passed D1 and D2 Windows builds, all 43 D2 host tests, Android debug assembly for arm64-v8a, armeabi-v7a, and x86_64, 15 replay demos, 1,509 mission routes, solo Guide-Bot fixtures, and both two-emulator co-op ownership fixtures. The ownership harness now checks durable post-handoff rescan state instead of a transient last-replan reason that later cache publication may replace.

## Suggested delivery slices

1. Measurement and contract only: Step 0.
2. Shadow correctness foundation: Steps 1 and 2 using the full planner, with no gameplay change.
3. Efficient live certification: action graph fast path and Step 3 comparisons.
4. Convergence coverage: Steps 4 and 5.
5. Behavior cutover: Step 6 behind a feature flag.
6. Simplification: Steps 7 and 8 only after release-profile performance and integration gates pass.

Each slice has a rollback boundary. No slice should make an incomplete background result authoritative or replace a still-valid cheap decision with a different answer merely because more cache data became available.
