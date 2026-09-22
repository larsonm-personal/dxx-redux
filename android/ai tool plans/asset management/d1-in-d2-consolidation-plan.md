# D1-in-D2 fidelity and implementation boundaries

Date: 2026-09-21

Status: implementation in progress; see the [implementation ledger](d1-in-d2-implementation-ledger.md)

The first requested step is implemented: all 30 D1 compatibility source/header files now live in `d2/main/d1_in_d2/`, including custom assets, PIG validation, checkpoint translation and replay adapters. The move preserves their contents; external includes, CMake source paths and source-inspecting tests use the new location. See the ledger for verification

Planning revision: section 0 is the authoritative finishing sequence and current stocktake. Section 16 supplies detailed ownership/removal contracts for its work packages. Earlier phase/package lists explain the design and are not additional queues of work. Existing implementation is a starting point to review, not a reason to preserve an unsuitable boundary

This plan develops the [fidelity audit](d1-in-d2-fidelity-audit.md). It supersedes the architectural assumption in the [June overlay design](plan_d1_in_d2_full_support_design_20260613.md) that a D1 game must first initialize D2 assets

Reading guide: start with section 0. Section 3 assigns source ownership; section 16 defines detailed operation boundaries and the D/E removal contracts. Unqualified compatibility filenames throughout this document now refer to `d2/main/d1_in_d2/`. Historical audit/ledger observations describe their dated revision, not necessarily the current source state. Implementation results belong in the ledger

The central acceptance rule is change locality: a correction to D1 artwork, briefing commands, homing or secret routing should normally touch its D1 owner and tests. A call in an original D2 file is acceptable; D1 constants, decision trees and sequencing behind that call belong to the owner. Review the removed original-file body alongside every new abstraction

## 0. Stocktake and plan to finish

### What exists, and what is still unproven

The implementation has moved well beyond the original D2-first overlay. Registered D1 data can populate the engine independently, and substantial gameplay/presentation algorithms have their own owners. The remaining work is integrated fidelity, strict replay evidence, optional-feature completion and removal of the second loading architecture. Passing isolated comparisons does not yet establish exact reproduction of complete D1 recordings

| Area | Current implementation/evidence | Remaining completion obligation |
| --- | --- | --- |
| Source ownership | Thirty compatibility files grouped under `d2/main/d1_in_d2/`; external callers explicitly include its domain headers | Review each remaining original-file hook; eliminate transitional bodies and private-owner dependencies where no longer needed |
| Original resources and D1-only play | Independent base/custom generations, original texture/effect/model/sound definitions, original cockpit and presentation. Ledger records isolated registered-PC Android play through First Strike's exit to level 2 | Complete lifecycle/error/edition coverage; prove optional D2 presence cannot alter baseline data. Preserve unbreakable ordinary D1 lights, native lava/reactor wreck and the retained destroyed-light conversion |
| Native gameplay | AI frame/path/world operations, weapons, small collision/physics rules, triggers and single-player campaign operations have owners and focused native comparisons | Whole-recording exactness, checkpoint frame-zero equivalence, unexercised interactions and broader campaign/network behavior |
| Presentation | Owned briefing session and cockpit/camera layout; ledger records 84 native/imported rendered-frame comparisons | Wider editions, audible output, interruptions/custom resources and full campaign presentation |
| Optional Guide-Bot | Private source reader, dependency maps and combined publication are present. Normal session preparation still does not attach an optional package; old capture/append code remains | Coherent source selection, working actor and all hardcoded sound/flare/morph consumers, restore identity, safe retirement and removal of legacy capture |
| Replay infrastructure | Existing `-D1InD2` runner, native checkpoint translator, result/state/RNG tooling and a five-demo D1 corpus | Strict native-versus-imported comparison, D1-only runner staging and explicit failure classification. Current runner success is not sufficient evidence of exact reproduction |
| Supported scope | Strongest evidence is registered PC content on Windows host and Android x86-64; recordings originate on Android arm64 | Declare and exercise required shareware/OEM/Mac, platform and network scope before claiming full support or retiring native D1 |

### Delivery order and exit gates

| Milestone | Work and ownership | Exit gate |
| --- | --- | --- |
| F0. Relocation | Completed mechanical move, explicit caller includes, build/test path updates; no gameplay edits | Byte-identical moved sources, ordinary/headless D2 builds, native-D1 shared-consumer build, host suites and Android compilation |
| F1. Establish a strict replay oracle | Extend existing scripts/shared diagnostics; D1 format interpretation stays in `d1_save_translate` and `d1_in_d2_input_demo`. Use all five D1 demos through native D1 and D1-in-D2 | Reproducible corpus manifest; independent recorded/native/imported verdicts; frame-zero, per-frame and terminal comparisons; failures/skips cannot appear as a fidelity pass |
| F2. Close baseline D1 divergences | Diagnose the earliest mismatch, then fix its asset/AI/weapon/level/semantics/restore owner. Companion off; use original D1 settings/data | Every supported corpus case has exact native/imported simulation reproduction; native-versus-recording failures separately resolved or explicitly left open. Add small integration cases for each root cause |
| F3. Complete optional enhancements | Finish section 16 D1-D4 as one load/spawn/behavior/restore/retire lifecycle. Cameras retain cockpit ownership; assets owns optional content | D1 alone and D1 with D2 present produce the same baseline; cameras do not change simulation. Companion works from cold launch and after restore; isolated native enemies remain unchanged |
| F4. Retire migration paths and close persistence | Section 16 E1-E5: remove old overlay/capture/backup APIs and test-only production paths; finish namespace/trigger/travel consumers | One resource lifecycle, documented serialized identities, actual save/reload/rewind and applicable network travel tests, no unexplained transitional owner/hook |
| F5. Release qualification | Section 16 E6 plus presentation/resource comparisons and platform matrix | Strict corpus passes on declared targets; D1-only cold startup/error/menu/shutdown; custom/stock and D1/D2 switching; ordinary D2 regression; explicit remaining unsupported scope |

F1 is the next implementation priority. Do not postpone whole-game evidence until after optional features. Serialization and lifecycle checks run alongside F2/F3, not only in F4. Each fix keeps the source-owner boundary and removes its superseded body in the same review. Native D1 remains the reference engine; deleting it is a separate decision after F5

### Exact reproduction using the D1 regression demos

Use the existing `android/tests/test_input_demo_regressions.ps1` and `run_input_demo_replay.ps1` entry points. Do not build a second replay engine or a demo-specific approximation of D1. The imported run must execute the D2 binary, restore the D1 checkpoint through the existing adapter and use the normal D1 session lifecycle

The current D1 corpus contains five version-4, `lcg_state`, save-checkpoint recordings:

| Recording suffix | Level | Frames | Recorded build / architecture |
| --- | ---: | ---: | --- |
| `20260920_184354` | 14 | 5,696 | 22740 / arm64 |
| `20260617_154210` | 15 | 2,006 | 16520 / arm64 |
| `20260618_201843` | 16 | 2,696 | 16520 / arm64 |
| `20260618_202117` | 18 | 2,634 | 16520 / arm64 |
| `20260616_202713` | 5 | 2,430 | 16490 / arm64 |

The 167 MiB level-14 recording is now admitted by the shared 256 MiB ceiling. The stocktake reran all five recordings in both engines after relocation:

| Level | Native versus recorded result | Imported versus recorded result | Native versus imported result fields |
| --- | --- | --- | --- |
| 5, 15, 16, 18 | Pass | Pass | Equal after separately identifying executable/mission labels |
| 14 | Fail | Fail | Differ in player, position and world-summary fields |

These are today's result-summary comparisons, not strict per-frame certification. Both runs used byte-identical D1 HOG/PIG files, with D2 assets additionally available to the imported run. No native repeatability or per-frame/RNG comparison was performed in this stocktake. The native level-14 failure predates relocation; its additional cross-engine divergence remains a concrete F1/F2 investigation target. Logs and archived actual results are recorded in the ledger

Level-14 follow-up: the [divergence investigation](../input%20demo,%20replay,%20determinism/level14-20260920-divergence.md) identifies a replay-only homing search as the native failure. A controlled rendered run using ordinary acquisition matches all recorded frame-state fields and the final result. Imported playback has a separate earlier positional drift and first angle mismatch at frame 3654; removing the homing substitution alone does not establish imported parity. The experiment was reverted pending a complete solution that also preserves targeting without rendering

Implement F1 in this order:

1. **Pin the inputs.** Emit a manifest containing demo/checkpoint and source-asset hashes, mission/edition, executable revision/build/architecture, player configuration, RNG mode, render mode and feature settings. Stage identical D1 base/custom resources for both engines. Run sequentially in distinct clean sandboxes; archive results even when comparison fails
2. **Prove the native reference.** Replay each demo twice in native D1 with the same fixed input/frame timing. Compare native run A with repeat A, and separately compare A with the recording's expected state/available trace. Classify a native mismatch before attributing anything to D1-in-D2. An unstable native run blocks an exactness claim
3. **Run the imported path.** Feed the unchanged demo and embedded checkpoint to D2 with `-D1InD2`. Do not use `-D1InD2StartFromLevel` for parity: replacing the checkpoint with a fresh level changes the experiment. Preserve player settings, simulation clocks, allocator/link state and both RNG stream states/counters
4. **Compare frame zero.** Observe normalized semantic state immediately after restore, before advancing simulation. Include objects and links, player/inventory, doors/triggers/reactor, AI/path/boss state, timers, RNG state and source definition identities. This separates translation errors from later algorithm errors
5. **Compare every frame.** Compare fixed-point position/orientation/velocity, controls/timing, object creation/deletion, AI, weapons, pickups/damage, world/trigger state and simulation RNG values/order/counts. Extend the shared state emitter where the existing trace is incomplete. Use frame checksums for routine passes and expanded records at the first mismatch. Final score/position alone is insufficient
6. **Compare terminal state and transitions.** Capture a full semantic snapshot before world retirement and compare exit frame, destination, inventory/score and world state. Replace the current terminal-exit subset shortcut in strict mode; it can currently copy actual player/position into the expectation. Timing normalization must use checkpoint metadata, never values from the actual run
7. **Define the cross-engine schema explicitly.** Permit only declared representation mappings: executable label versus content identity, verified mission aliases, typed resource IDs, unused D2-only capacity and diagnostic source-path labels. Check the expected identity before canonicalizing it. Never copy gameplay values from actual into expected, relax numeric tolerances, drop populated native fields or ignore RNG differences to get a pass. Keep original records beside canonical records
8. **Report three separate verdicts.** Recording versus native A, native A versus repeat A, and native A versus imported B. A=B is useful engine-parity evidence even if the old recording disagrees; it is not exact reproduction of the recording. Correct root causes before deliberately replacing stale recordings, retain their history, and never automatically bless new actual output as expected
9. **Make the gate fail closed.** Require all selected demos and required traces to execute. Missing references/assets/results, timeout, unsupported checkpoint, size rejection or reduced comparison are failures/incomplete coverage, not success. Unit-test this orchestration with wrong mission identity, first-frame divergence, missing reference and terminal-state mismatch before relying on its green result

The equality target is exact canonical D1 simulation state and event order for identical inputs, not binary equality of different engine structs. Pixel/audio fidelity needs the separate matched-renderer/resource checks below. Record expected non-simulation metadata differences explicitly, and do not call a partial diagnostic projection a complete state comparison

### Existing runner gaps to fix in F1

| Current source behavior | Required change |
| --- | --- |
| `Get-D1InD2GameConfig` still requires D2 HOG/HAM/PIG plus D1 files | Add a D1-only asset selection/staging path and explicit D1 startup selection; separately run the both-installed invariance case |
| `Normalize-D1InD2ExpectedResult` takes expected `game` and `mission` from actual | Validate expected engine/content/mission mapping from the manifest, then canonicalize deterministically |
| `Get-TerminalExitExpectedSubset` can replace expected player/position with actual | Strict parity captures and compares the terminal snapshot; no such substitution |
| A missing `ReferenceResultRoot` entry prints SKIP and returns zero | Required parity references fail the strict gate; optional matrix rows remain visibly unverified |
| Reference-result replacement happens after imported identity normalization | Route recorded and native references through the same explicit schema; passing a native result directory alone is not a valid cross-engine comparator |
| Trace comparison currently targets recording-derived traces | Add an explicit native-run trace reference for A/B comparison, keep recorded comparison separately, and report the first differing frame/field/RNG event |
| Existing `run_all_tests.ps1` D1-in-D2 row compares to embedded expected results; graphics rows compare to the same engine's fast run | Add the native-D1 versus imported-D1 parity relationship explicitly; preserve ordinary D2 and rendered-versus-fast checks |

Existing baseline commands from the repository root (these exercise today's comparator, not the planned strict gate):

```powershell
.\android\tests\test_input_demo_regressions.ps1 -RecordedGame d1 -Game d1 -ResultArchiveRoot temp/d1-parity/native
.\android\tests\test_input_demo_regressions.ps1 -RecordedGame d1 -D1InD2 -ResultArchiveRoot temp/d1-parity/imported
```

Retain prior artifacts with the repository helper before each new run. The strict paired runner should reuse these entry points and their result/trace producers. Keep comparison/orchestration in `android/tests`; game-format interpretation and fixes stay in the compatibility folder. The native/imported pair should not share mutable sandbox files

### Closing the gaps beyond the current corpus

Start F2's restore review with source-confirmed omissions: `d1_save_translate_skip_weapon_fidelity_state` discards per-weapon state, `d1_save_translate_skip_morph_state` discards active morph payloads, and effect timing is read into unused locals. Audit each against the corresponding native save writer and the engine's existing restore services. Restore the necessary state through the proper domain boundary; do not add a special case for a particular demo or assert these are the cause of a measured mismatch before locating its first divergence

All five recordings begin at saved checkpoints. Add native-recorded cases for a fresh level, reactor/normal exit, secret entry/return/death, save/restore and rewind, compound triggers, custom definitions, boss/matcen behavior, morph state and camera toggles. Choose cases by uncovered operations; do not record a large random corpus as a substitute for diagnosing existing divergence

Keep enhancements off for baseline parity, then repeat with cameras and with unused optional assets available. An active companion can intentionally affect collisions, awareness and combat; test its behavior separately and compare native enemies in an interaction-free fixture. Do not demand equal world traces after deliberately adding a new actor, or mask those differences inside the baseline comparator

Use the existing native/imported rendered briefing/cockpit checks, decoded bitmap/model/sound provenance checks and isolated Android interaction runner alongside replay. No-render replay cannot prove original lava colors, monitor destruction, reactor wreck appearance, camera layout or audible samples. Repeat available host runs on Android arm64 for the architecture on which the corpus was recorded; list unavailable platforms/editions as gaps

Completion means both fidelity and consolidation: declared D1 content works without D2 assets; strict supported-demo parity and original audiovisual checks pass; optional features have independent resources and a complete lifecycle; native/save/custom/transition consumers use the same owners; ordinary D2 remains stable; and every retained original-file hook has a concrete phase/service contract. A folder move alone satisfies F0, not these later gates

## 1. Contract and architectural decision

Run the original D1 game using the D2 executable's engine facilities. Preserve D1 artwork, palettes, animation, sound, gameplay, mission progression, and supported custom content. Guide-Bot and HUD cameras are explicit enhancements. D1 must eventually boot and play with D2 resources absent

D1 compatibility implementation belongs in the existing D1 compatibility source family and a few cohesive additions. Original D2 files should contain only the integration necessary to invoke that implementation or consume correctly populated tables. Moving a condition into a one-line helper while leaving the surrounding D1 algorithm in `ai.c` or `laser.c` does not satisfy this objective

Use three mechanisms, in this order:

1. **Correct data:** install complete D1 tables so existing engine consumers naturally do the right thing
2. **Small policy operations:** centralize a genuinely small decision or calculation that cannot be expressed in data
3. **Complete D1 operations:** where execution order or algorithms differ substantially, dispatch to a D1 implementation at a coherent operation boundary

Keep the existing C-style engine interfaces. Do not introduce a general game-plugin framework, event bus, class hierarchy, or a large table of callbacks for hypothetical games. Do not rewrite ordinary D2 routines just to make them symmetrical with D1

Some original files will still need calls into the compatibility modules. Zero touched engine files is not a realistic target. The target is that D1 decisions, algorithms, asset ownership, and lifecycle ordering can be understood and changed in the D1 source family, without having to edit a network of interleaved branches

## 2. Starting point and migration accounting

The initial survey found useful boundaries:

- `d1_in_d2.c/.h`: asset loaders, validation/application lifecycle, Guide-Bot preservation, and mode queries
- `d1_in_d2_semantics.c/.h`: several small gameplay policy functions
- `d1_custom.c/.h`: custom bitmap and sound replacement
- `d1_pig_validation.c/.h`: source validation helpers
- `d1_save_translate.c/.h`: D1 checkpoint decoding and application
- `d1_in_d2_input_demo.c/.h`: D1 replay integration

At the initial survey, substantial D1 policy remained in `gamemine.c`, `gameseq.c`, `ai.c`, `ai2.c`, `aipath.c`, `laser.c`, `physics.c`, `collide.c`, `fvi.c`, and `titles.c`. Much of that has since moved into the owners described below. The current source scan finds `d1_in_d2|EMULATING_D1` in 36 files under `d2/main/` with extension `.c` or `.h` whose filenames do not begin with `d1`. This includes headers, diagnostics and legitimate integration hooks; it is a scope indicator, not a count of architectural defects. Some format-dependent behavior uses other conditions. Classify the contents of each hook rather than trying to drive this count to zero

Before changing behavior, create a short integration ledger in this plan or an adjacent document. Record each original-file hook's caller/function, D1 owner, purpose, side effects, verification, and whether it is permanent or transitional. Inventory version-based D1 branches as well as explicit mode checks. Record the current commit and working-tree changes as the refactoring baseline; do not compare against pristine 1996 code and accidentally remove unrelated Redux improvements

The working tree has since begun this extraction: `d1_in_d2.c` is a lifecycle facade, with asset, bitmap, AI and weapon owners. The facade now publishes independently prepared registered D1 assets before level decoding; individual transitional `apply_*` operations are private. The complete native robot frame, navigation and world awareness now belong to AI, with frame-only helpers private. Native hide/follow checkpoint restoration has direct comparison evidence; broader companion/network and campaign evidence remain open. The weapon owner includes target acquisition/reacquisition, placement, fire-awareness and speed operations as well as steering; the ledger records unit 4's base-operation verification and wider limitations. Independent base data and headless level loading do not establish D1-only cold startup or complete fidelity

Acceptance for each implementation slice:

- D1 bodies moved out of original files are actually removed there
- New hooks reuse existing operation boundaries where possible
- Transitional hooks have an explicit removal phase
- A reduction in grep matches alone is not evidence of a better design
- No broadly applied formatting or renaming obscures the functional diff

## 3. Source ownership

Keep this engine-specific implementation in `d2/main/d1_in_d2/`. These modules use D2 runtime types and should compile on every supported platform. Shared automation and platform-independent tooling can continue in `android/app/src/main/cpp/shared`; Android launcher policy stays in Kotlin. Do not move D1 binary-format knowledge into Kotlin. Keep the existing filenames and parent CMake source list; a folder move does not need a new library, extra public include directory or forwarding headers

The following is a target responsibility map, not a requirement to create every file immediately. Add a domain file when extracting a substantial body. Keep small related operations together

| Owner | Responsibility | What should leave original D2 files |
| --- | --- | --- |
| `d1_in_d2.c/.h` | Small public facade, active D1 session, startup/mission/level lifecycle, publication of validated data, cleanup and diagnostics | Ordered D1 `apply_*` orchestration in `gameseq.c`; mode inference spread across callers |
| Existing `d1_in_d2_assets.c/.h` in the working tree | D1 base-data readers, bitmap/sound registration, complete table conversion, resource ownership and provenance | D1 PIG parsing/replacement and palette conversions in `piggy.c`; D1-to-D2 slot assumptions in the faithful load path |
| Existing `d1_in_d2_bitmaps.c/.h` in the working tree | D1 bitmap decoding and owned image preparation; private collaborator of the asset owner | Remaining legacy replacement bookkeeping; generic engine paging and GPU caches remain engine services |
| Existing `d1_in_d2_guidebot.c`, private asset collaborator | Optional companion dependency discovery, namespace mapping and generation-owned publication/retirement | Live D2 table capture and append-on-spawn resource repair; no separate public header or session lifecycle |
| Existing `d1_custom.*`, `d1_pig_validation.*` | Custom overlays into the same D1 asset generation; shared format validation | New custom-format special cases otherwise destined for `bm.c` or `gamesave.c` |
| Existing `d1_in_d2_semantics.*` | Small stateless rules and calculations for collision, pickups, damage, and other limited differences | Local D1 helpers such as explosion-position policy in `collide.c`; repeated difficulty/mode predicates |
| Existing `d1_in_d2_weapons.c/.h` in the working tree | Projectile selection, parent-sensitive children, homing behavior, weapon-specific runtime rules | Substantial D1 sections in `laser.c`; avoid growing semantics into another monolith |
| Existing `d1_in_d2_ai.c/.h` in the working tree | D1 robot scheduling, behavior interpretation, targeting, firing, path decisions, and boss policy as validated | Interleaved D1 algorithm bodies in `ai.c`, `ai2.c`, and `aipath.c` |
| Existing `d1_in_d2_presentation.c/.h` in the working tree | Text/resource mapping, briefings/endings selection, coordination with cockpit drawing | New D1 resource and command branches in `titles.c` and startup UI |
| `d1_in_d2_cockpit.c/.h`, presentation collaborator | Substantial native-D1 cockpit layout, gauge/weapon-panel drawing, window masks and viewport geometry | One dispatch per drawing/setup phase; shared renderer and camera execution remain in the engine |
| Existing `d1_in_d2_levels.c/.h` in the working tree | D1 level-format interpretation, triggers, progression/secret lifecycle, object fixups; texture references and reactor binding are now owned | D1 texture conversion policy, trigger conversion, secret transition policy currently spread across level/save callers |
| Existing `d1_save_translate.*`, `d1_in_d2_input_demo.*` | Serialized format adapters using the common D1 conversion and lifecycle APIs | Repeated D1 restoration rules in live loaders or demo callers |

Keep the facade header small; engine callers include only the relevant domain header. Private staged-data structures and maps must not become a public grab bag of mutable globals. Reuse engine `object`, vector, and table types where appropriate rather than inventing parallel representations for every runtime type

If physics extraction becomes substantial, split a `d1_in_d2_physics.c` at that point. Do not pre-create one file per engine source or per bug

The same rule applies inside AI: the working tree now has `d1_in_d2_ai_path.c` for native path construction, requests and following. It belongs to the AI domain and uses the existing path storage; it is not a separate navigation framework. Source behavior meanings shared by the two implementations live in a private `d1_in_d2_ai_internal.h`. Keep internal declarations private to that domain when splitting requires them. Do not add a public header per implementation file, or create matching compatibility files for every original D2 filename

## 4. How hooks should work

### Data eliminates hooks

For ordinary imported D1 lights, install `TmapInfo.destroyed = -1` and the correct D1 effect metadata. D1 monitors remain destructible through their real effects. Once both direct-hit and nearby-explosion paths are verified, remove the temporary static-light guards from `collide.c` and `wall.c`

Likewise, import the correct reactor live/wreck definition before object verification instead of relying permanently on a late repair of objects verified against D2 tables. Load all D1 effect frames so `effects.c` can continue consuming an ordinary complete table

The destroyed-light palette conversion remains available as explicitly requested. Move its ownership with the retained-asset conversion code; it does not determine which art the default D1 game selects

### Small operations express a rule, not the current patch

Example interface shapes, to finalize during their implementation slice:

```c
int d1_in_d2_primary_projectile(int primary_weapon, int default_projectile);
fix d1_in_d2_quad_damage_scale(fix default_scale);
int d1_in_d2_smart_child(const object *parent, int default_child);
```

Call these at a single weapon-selection/calculation boundary where possible. Preserve D1 Spreadfire's distinction between accounting record 12 and projectile record 20. A global substitution of weapon record 12 is incorrect

Small pure helpers may return the supplied D2 default when inactive. The default argument must be a side-effect-free value: do not evaluate D2 RNG or mutations merely to provide a fallback. Functions that mutate state must be named and documented as actions; the current player explosion-position helper, for example, also changes `weapon->pos` and should not be disguised as a pure getter

### Divergent algorithms get an operation boundary

For a cohesive D1 operation, prefer this shape over interleaving dozens of predicates:

```c
if (d1_in_d2_try_robot_frame(obj))
    return;

/* Existing D2 operation */
```

This is illustrative, not an instruction to insert the call blindly at the first line. Establish which existing prelude/postlude is shared, which is D2-specific, and which must execute exactly once. The D1 operation owns its complete specified phase. An inactive or inapplicable `try_*` returns without mutation, random draws, resource loads, or diagnostic side effects

Use an explicit result enum when an operation can fail, block, or request another transition. Do not overload a boolean to mean both "not D1" and "failed to load D1"; a failure must never fall through into D2 loading

Do not move an entire D2 routine into the D1 module merely to reduce a call count. A modest, bounded D1 algorithm copied/adapted from native D1 can be easier to verify than a generalized routine with dozens of mode switches. Preserve provenance and document the intentional duplication; do not duplicate the renderer, physics world, object storage, or entire engine

### Shared engine services stay narrow

Compatibility modules can call existing collision, rendering, object creation, path geometry, audio, and serialization primitives. If extraction encounters a private engine helper, expose the smallest useful operation through its existing header. Do not export all file-static state or introduce a large callback context solely to make extraction possible

Keep decisions and mutations at their original simulation phase. Preserve fixed-point arithmetic order, RNG draw order, frame scheduling, and visibility-cache lifetime. Do not precompute stateful decisions in a policy struct and consume them later

## 5. Complete D1 data, independent of a D2 initialization

### Source identity and runtime tables

Prefer D1 source indices for the active D1 profile wherever the engine table capacity and semantics permit. Existing global runtime arrays can remain the engine-facing representation. This avoids a resolver on every draw or collision and does not require converting the whole engine to objects or registries

Where indices must move, allocate them deterministically and translate every reference through maps owned by the asset module. The faithful D1 level path must not use "nearest D2 equivalent" mappings. Retain the old conversion only for identified legacy serialized/conversion consumers that still need it, behind a named adapter

Explicitly distinguish bitmap IDs, texture IDs, vclip/effect IDs, model IDs, robot IDs, and weapon IDs. Numeric equality across these spaces is not a valid mapping. Audit D2 hardcoded weapon and robot constants before preserving D1 IDs; route semantic operations through the weapon/AI boundaries rather than assuming a table copy alone resolves their meaning

The generation must include:

- Original bitmap pixels, dimensions, RLE/transparent flags, palette and model flat colors
- Texture lighting/damage/flags/effect references; deterministic neutral values for D2-only fields
- Complete vclips, effects, critical/destination links, timing, sounds, and destroyed images
- Wall animation records and their full texture references
- Robots, joints, models, model textures, live/dying/dead models, reactor and player ship
- Weapon data, direct projectile bitmaps, HUD pictures, powerup definitions, gauges and cockpit assets
- Sound samples and logical/alternate sound maps
- Resource names and source provenance needed by presentation and custom-content loading

Validate actual capacity before publication. Never clamp a valid D1 animation to an unrelated D2 frame count. If an engine capacity is genuinely too small, make one justified generic capacity adjustment or report an unsupported input; do not silently truncate

### Paging and ownership

Move D1 decoding out of `piggy.c`. Retain shared registration, page-in/page-out, RLE and graphics-cache facilities there. First prove the D1 generation with owned resident data using the existing validated loaders; then assess peak memory on Android. If paging is needed, attach explicit source-offset/owner information to registrations and add one generic source-read boundary. Do not scatter D1 file tests through page-in code

Runtime arrays may borrow generation-owned buffers. Document the owners of model bytes, bitmap bytes, sound bytes, name tables, caches, and optional feature resources. A generation cannot be released while any renderer, audio channel, replacement table, or object still references it

### Lifecycle and ordering

The coordinator manages a small explicit lifecycle: inactive -> prepared -> active -> released. Preparation failures leave the prior active session valid or return to a known unloaded state; partial tables must not appear active

At bootstrap, select the base game before asset-dependent text, fonts, palette, menu, HAM/PIG initialization, and game tables. An explicit launcher request wins. With only D1 assets available, select D1; preserve existing D2 default behavior when a D2 installation is selected. Mission selection can request a controlled profile switch

At mission/level load:

1. Resolve the requested game, edition, base files, mission archive, and enabled extensions
2. Prepare and validate base plus mission/level custom definitions and all translated references
3. Quiesce users of the previous generation and publish definitions before `load_level` verifies objects or initializes AI
4. Decode the level against those definitions, then perform the D1-specific object/trigger fixups at the loader boundary
5. Initialize simulation and presentation once; activate optional features after core D1 definitions exist
6. On failure, discard unpublished data, or use the normal unload/error path after publication; do not claim full world rollback without implementing it

The original `gameseq.c` integration loaded and verified objects before its D1 `apply_*` block. The current implementation publishes base and custom PG1/DTX/HX1 definitions before decoding the level and verifying objects. The ledger records custom-to-stock transitions and native custom checkpoint comparisons. Saved object state is restored after definitions are loaded; it must not be replaced by definition defaults

On exit or profile switch, stop referencing sounds, release level objects/caches, restore or reload the requested destination profile, and invalidate all generation-dependent caches. D1-only shutdown must never try to reload `descent2.ham`. Support the same lifecycle on desktop and Android; the existing Android reset entry point should delegate to it

## 6. Domain-specific extraction decisions

| Domain | Proposed original-engine boundary | D1-owned work and important constraint |
| --- | --- | --- |
| Startup | Small selection/initialization branch in `inferno.c` and existing data initialization boundary | D1 resource prerequisites, bootstrap order and D1 text/font mapping. D1 text indices cannot simply be substituted for D2 text indices |
| Assets | Registration/release/page-in service in `piggy.c`; mission/level entry in `gameseq.c` | All D1 parsing, mappings, complete tables and lifetime. No D2 asset preload for D1 |
| Level decoding | D1-format adapter at `gamemine.c`/`gamesave.c` boundary | Identity-preserving texture decode, object references, trigger semantics; use the same conversion for fresh levels and checkpoints |
| Weapons | Selection, object initialization, homing-step and child-creation boundaries in `laser.c` | Accounting/projectile distinction, quad damage, robot smart children, complete D1 homing algorithm. Avoid one hook for each firing case |
| AI/pathing | Robot-frame/init and a small number of targeting/path-policy boundaries | Extract complete D1 scheduling/behavior phases. Share geometry/path storage where valid; no wholesale D1 second engine |
| Physics | Rotation integration and rotational-hit response boundaries | Move the existing D1 integration body and skip-count policy out of `physics.c`; preserve math/RNG order |
| Collisions/pickups | Existing damage, explosion and pickup-calculation boundaries | Move rule calculations into semantics. Eliminate table-driven exceptions after proper import; do not combine unrelated collision handlers |
| Progression | Normal exit, secret entry/exit, death and restore entry points | One D1 transition policy in `d1_in_d2_levels.c`; callers execute a defined result, without rediscovering D1 routing |
| Presentation | Cockpit/HUD draw phase and resource/layout selection | Original D1 coordinates, art and gauge behavior in presentation module; reuse drawing primitives and camera rendering |
| Save/replay/network | Existing format/profile validation and restore boundaries | Persist/validate effective game rules and asset identity; invoke the same conversion and generation lifecycle |

AI requires particular care. A mission-wide D1 flag must not route the optional D2 Guide-Bot through ordinary D1 enemy AI. Centralize selection by actor role in the AI dispatcher. Keep Guide-Bot path services available without allowing its D2 rules to leak into D1 enemies. Initial extraction should move existing D1 bodies without changing behavior; larger replacement of `do_ai_frame` should follow only after its preconditions and deterministic traces are covered

## 7. Original presentation and optional enhancements

Original D1 cockpit art requires original layout and masking, not merely copying pixels into D2 cockpit slots. Keep layout constants and D1 gauge drawing together in the presentation module. Reuse existing renderer/texture primitives. When D1 has only a low-resolution asset, scale it consistently rather than silently replacing it with D2 art

Audit startup/menu strings, fonts, palettes, briefings, endings, escape sequence resources, and music lookup as part of D1-only boot. D2 UI code's text IDs need an explicit mapping or a D1 presentation path. Build this in the D1 module; do not add per-string mode checks to dozens of menu callers

Keep optional feature assets separate from base D1 resources. Guide-Bot's D2 model/textures/sounds must be imported explicitly into extension slots and translated references; installing D2 assets must not silently change the rest of the D1 game. The robot's D2 artwork remains optional content, not a bootstrap dependency. If unavailable, report that feature unavailable while the D1 game remains playable

HUD cameras should use the existing engine camera renderer with a D1-owned layout choice. Enabling a camera does not authorize replacing the whole cockpit. Do not impose a new choice of camera placement or Guide-Bot replacement artwork in this plan; those decisions belong to the feature slice after the faithful baseline works

Keep extension settings and asset availability separate from the base-game identity. Verify the no-enhancement baseline first, then the combinations with cameras and Guide-Bot enabled

## 8. Custom content, persistence, and compatibility

Apply `.pg1`, `.dtx`, and `.hx1` through the same source namespace and generation builder, with precedence matching native D1. Test two distinct source names/indices that previously shared a D2 destination. Expose unresolved custom entries as precise diagnostics; never fall back silently to unrelated D2 content

Choose the supported D1 editions explicitly. Registered data is the first implementation target, not a claim that PC shareware, early releases, OEM, and Mac formats are already equivalent. Track each format through parser, readiness, runtime and fixture support. The launcher should distinguish file presence from engine-supported content

Preserving D1 source IDs changes assumptions made by existing D1-in-D2 saves/replays that serialize converted D2 indices. Inventory those formats before switching the runtime representation. Keep native D1 and existing desktop D2 format contracts; version or translate D1-in-D2 state explicitly. Do not reinterpret old bytes under new ID semantics. Android-specific pre-release metadata can be replaced according to repository policy, but that does not authorize breaking original game save/demo formats

Repair trigger and morph/weapon-state checkpoint omissions in the existing translator using common conversion routines. Fresh start and checkpoint restore must choose the same rules and effective tables. Record deterministic content/rules identity for new replay and multiplayer validation at existing metadata boundaries, without serializing pointers or platform-dependent hashes

Exact legacy D1 `.dem` playback is a separate compatibility milestone, not a prerequisite to the first playable D1-only slice. Preserve native D1 as a behavioral reference until campaign, save, replay and relevant multiplayer acceptance is established

## 9. Implementation sequence and exit gates

Each numbered phase is a reviewable workstream and may require several small changes. Separate mechanical extraction from behavioral correction wherever practical

| Phase | Work | Evidence required to finish |
| --- | --- | --- |
| 0. Baseline and hook ledger | Inventory original-file hooks and existing regression evidence; record working-tree fixes; define source edition and fixtures | Named baseline, owner for every known D1 body, known failing parity cases recorded without treating them as desired behavior |
| 1. Establish ownership | Extract D1 PIG code from `piggy.c`, local D1 helpers from collision/AI/physics, and `apply_*` orchestration into their owners; keep transitional load behavior | Existing D1-in-D2 and D2 tests unchanged; original-file bodies removed; public/private boundaries documented |
| 2. Build complete D1 generation | Implement source registry, all tables/references, neutral D2-only fields, custom overlay staging, ownership and deterministic allocation | Synthetic integration fixtures for full references/frames plus local registered-data comparison; no dependence on D2 tables in builder |
| 3. D1-only playable slice | Add early base-game selection, original bootstrap resources and minimal complete D1 cockpit path; publish tables before level verification; wire launcher readiness | Cold launch First Strike, fly/fire, use a door, destroy a monitor and reactor, reach level 2 with D2 archives absent; no-enhancement baseline |
| 4. Complete visual/content fidelity | Finish HUD/presentation, all campaign effects and destruction, music/briefings/endings, custom `.hx1`, additional supported editions | Original-art/animation comparisons and per-edition/custom fixtures; remove obsolete overlay assumptions and light guards |
| 5. Consolidate and correct gameplay | Complete domain extraction as needed; fix weapon gaps, D1 AI/physics/collision/progression differences in the owners | Targeted native-D1 comparisons with preserved RNG/math order; D2/Guide-Bot regression coverage; no new dispersed rule bodies |
| 6. Save/replay and campaign gate | Repair state translation, IDs and identity validation; test secret/death/restore and broader campaigns | Fresh-load/restore parity, transition matrix, supported-format round trips and replay diagnostics; no silently discarded relevant state |
| 7. Optional features | Explicit Guide-Bot asset import/AI dispatch and D1 camera presentation | D1-only remains playable without feature assets; optional features work with no base-art or rule substitutions |
| 8. Consolidation readiness | Cross-platform runs, supported edition/custom/network matrix, inspect remaining ledger entries | Remaining hooks small and justified; transitional hooks removed; separate decision on retiring native D1 |

Dependency order: 0 -> 1 -> 2 -> 3. Phase 3 needs a complete enough generation and presentation to exercise a real level; a menu-only launch is insufficient. Phases 4 and 5 can then proceed independently, with fixes to the known checkpoint issues brought forward before using those checkpoints as parity evidence. Phase 6 depends on the final runtime ID contract; phase 7 depends on stable generation and actor dispatch. Do not postpone all D1-only testing until every behavior difference is resolved

The first implementation change should be Phase 0/1 boundary work, not more isolated lava/cockpit exceptions. Preserve the recent fixes through extraction. Each behavioral change then happens in the appropriate owner and removes the transitional workaround it replaces

## 10. Verification and review criteria

Use existing native CMake/CTest targets, including `test_upstream_compat`, and existing Android automation/introspection. Add meaningful integration scenarios to those facilities rather than unit tests that merely assert a helper returns its implementation constant. Update `d2/main/CMakeLists.txt` for extracted sources and relevant test link sets; verify Android uses the same implementation

| Verification | Required distinction |
| --- | --- |
| Mechanical extraction | Preserve current behavior, RNG traces and D2 outcomes; no opportunistic fidelity fixes in the same diff |
| Asset generation | Decode original pixels/flags/palettes, all frame counts/timing, full reference graph and table values; cover malformed/truncated input and failed preparation |
| D1-only run | Isolated search paths with D2 HOG/HAM/PIG/S11/S22 and loose substitutes absent; trace file opens and test sound enabled; no hidden installation fallback |
| Visual fidelity | Native D1 comparisons at matching resolution/settings; automated state/resource inspection plus deliberate visual review of cockpit, sprites, lava, monitors, doors and reactor wreck |
| Simulation | Focused weapon, AI, collision, pickup and transition scenarios; compare relevant outcomes and RNG/state traces under controlled inputs, not screenshots alone |
| Lifecycle | Cold boot; repeated levels; failed load; D1 -> D2 -> D1; custom -> stock; app/session reset; cache and ownership checks |
| Persistence | Fresh start versus translated checkpoint; active weapon/morph/door state; supported old IDs and new identity; secret entry/death/restore/return |
| Enhancements | Baseline, cameras, Guide-Bot, both; correct actor dispatch and stable D1 asset provenance |
| Platforms | Windows builds/tests and serial Android automation for each substantial runtime slice; Linux/macOS compile coverage through available CI before final consolidation |

For actual code changes, run the repository's scoped mixed-language quality command and relevant builds/tests. Use sanitizers where the existing host targets support them for generation ownership changes. Do not report unavailable platform or runtime tests as passed

Architecture review questions for each slice:

1. Is the D1 behavior defined in its owner, or merely hidden behind a helper while still implemented in the original file?
2. Could complete data remove this hook altogether?
3. Does the boundary represent a meaningful operation with explicit effects and ordering?
4. Does the inactive D2 path preserve its established behavior and avoid D1 allocations/mutations?
5. Does this expose only the engine services actually needed, with no large new shared mutable context?
6. Does another D1 correction in this domain now fit inside the owner without changing many original files?

## 11. Main risks and bounded responses

- **Global table/cache lifetime:** use staged ownership and explicit unload order; do not attempt simultaneous live D1 and D2 worlds as part of this project
- **Hardcoded IDs:** inventory consumers by semantic category; fix at selection/interpretation boundaries rather than applying a global numeric substitution
- **AI extraction changing timing:** start with mechanical moves, preserve state/RNG order, and introduce whole-phase dispatch only with traced preconditions
- **Frontend dependency on D2 assets:** include text/font/cockpit work in the early D1-only slice, not as final polish
- **Over-abstraction:** use the existing source family and direct function calls; split modules by substantial responsibilities, not by one helper per file
- **Under-abstraction:** do not approve a phase complete while its D1 bodies remain interleaved in legacy files without an explicit temporary justification
- **Misleading green tests:** change the destroyed-monitor fixture's retained-D2-art expectation when the faithful destination is implemented; keep prior regression coverage but use native D1 data/behavior as the intended oracle
- **Premature engine deletion:** native D1 remains available for comparison throughout this plan; retiring it is a later outcome of demonstrated fidelity

## 12. Concrete boundary contracts

These contracts refine the phases above. They describe responsibilities rather than freezing function names before their callers have been extracted

### Public surface and dependency direction

`d1_in_d2.h` should expose session selection/lifecycle, read-only diagnostics, and the few mode queries required at dispatch boundaries. Move the individual `apply_*`, parser, and table-editing functions into private asset headers as callers migrate. Ordinary engine code must not choose their order. Domain headers expose complete operations and small value calculations; only compatibility implementation and integration tests include the staged-generation layout

The allowed dependency direction is engine entry point -> D1 facade/domain operation -> existing engine primitives. Asset readers must not call back into mission loading, and presentation must not reopen the PIG to build its own mappings. Save/replay adapters consume the same asset and level conversion contracts as live loading. Avoid cyclic orchestration where an asset import starts a level that imports assets again

Mode is selected for the session before bootstrap and remains stable during a simulation step. Actor role is a separate decision: a D1 enemy and an optional Guide-Bot can share a level without sharing AI policy. Do not add independent global switches for every D1 correction

### Prepare, publish, retire

The asset owner should provide three internal operations with explicit ownership:

| Operation | Inputs and output | Required guarantee |
| --- | --- | --- |
| Prepare | Resolved source edition, base resources, ordered custom overlays, requested extensions -> owned generation or diagnostic | No live table, palette, sound, object, or renderer mutation; validate the complete reference graph |
| Publish | Validated generation, with old consumers quiesced -> active definitions | Install counts, definitions, names and palette together before object verification; initialize engine caches in their existing subsystem |
| Retire | Inactive generation after objects, audio and caches release references | Free only owned buffers; no implicit D2 reload; safe after partial preparation |

The facade owns the transition, while the asset module owns buffers and table publication. Keep source definitions distinct from mutable runtime animation state so advancing a level cannot corrupt the next load's source. Use explicit success/failure results: an invalid D1 generation must not trigger an accidental D2 fallback

Do not promise rollback of a live world after publication. Preparation failure can preserve the previous generation; a failure after the old world is released follows a defined unload/error path. Test both stages separately

### Function-level migration targets

| Current location | Final boundary | What the caller must stop knowing |
| --- | --- | --- |
| `gameseq.c`: `LoadLevel` preparation/finalization | Base prepare/publish now precedes object verification; extend preparation to custom definitions | Order of individual D1 asset loaders and repairs to objects verified against D2 tables |
| `gamesave.c`: `verify_object` and `gamemine.c`: texture conversion | D1 level adapter resolves source references against active definitions | Nearest-D2 texture choices and D1 object-ID repair rules |
| `piggy.c`: shared registry/replacement cleanup | Generic registration/cache operations plus a bounded compatibility cleanup hook if still needed | D1 PIG layouts, palette selection and ownership of D1 source bytes |
| `laser.c`: selection, homing updates and smart children | Weapon-domain operations at those existing phases | D1-specific record numbers, parent rules, homing constants and algorithm branches |
| `ai.c`, `ai2.c`, `aipath.c`: repeated scheduling/visibility/path predicates | D1 robot phase dispatch, with shared geometry and explicit actor-role routing | D1 scheduling algorithm and its interpretation of behavior codes |
| `fvi.c`: local `d1_in_d2_transparent_wall_*` bodies | D1 collision interpretation in semantics, calling a narrow existing geometry/texture-sampling service | D1 transparent-wall crossing policy; the collision search itself stays shared |
| `physics.c`: rotation and rotational-hit dispatch | Keep the extracted operation boundary; assess remaining surface rules separately | D1 integration arithmetic and hit-response scheduling |
| `switch.c` and `gameseq.c`: secret routing | Level-domain transition decision followed by existing shared load/save operations | Whether a D1 secret exit, death or return uses D2 secret-save behavior |

For each extraction, write down the prelude, owned phase and postlude before moving code. For example, robot firing must consume visibility and RNG at the same point as before; an apparent early return must not skip shared network bookkeeping. Expose a private engine helper only when its contract is useful and narrow. Do not export a file's internal variables to avoid deciding where the boundary belongs

## 13. Reviewable delivery packages

Use these packages to implement phases 1-3 without letting the first runnable result depend on completing the entire fidelity project

1. **Finish ownership extraction.** Classify the remaining original-file D1 bodies in the ledger, move substantial bodies, and narrow public headers. Preserve existing behavior, including the recent light/lava/reactor fixes. Acceptance: old bodies removed, builds and relevant regression tests pass, every retained branch has a named owner and purpose
2. **Complete independent data preparation.** Finish the owned definitions/bitmap/sound generation, including gauges, cockpit IDs, texture properties, full animations, model references and neutral D2-only fields. Acceptance: preparation works with empty D2 registries; malformed references fail without changing live state; real registered D1 data retains original counts and pixels. This package does not claim playable D1-only support
3. **Publish and load against D1 definitions.** Add publication/retirement, move it ahead of object verification, and route faithful level loading through source IDs. Acceptance: monitor destruction, non-blowable lights, lava animation and reactor wreck use original definitions; repeated loads and D1 -> D2 -> D1 do not leak stale tables. Remove superseded late repairs and overlay mappings from this path
4. **Close the bootstrap dependency.** Select D1 before D2 archive/text/font initialization, provide original resource mapping and a usable D1 cockpit, and adapt launcher readiness. Acceptance: an isolated D1-only installation launches First Strike with sound, completes level 1 and enters level 2. Record attempted resource opens so local D2 installations cannot hide missing dependencies
5. **Expand fidelity in domain-sized changes.** Weapons, AI, presentation, progression and persistence each get their own changes and native-D1 comparisons. Optional features follow stable data and actor boundaries. No package can call itself complete solely because existing overlay-era tests remain green

Package 1 should not become a prerequisite to extracting every AI branch before package 2 can begin. Establish the shared boundaries first, then migrate the remaining domain bodies alongside their fidelity work. Conversely, package 4 must happen early enough to expose real bootstrap dependencies; avoid months of improving a D2-dependent overlay before attempting it

Each review includes a small architectural delta: original-file hooks added/removed, D1 bodies moved, engine services exposed, and temporary hooks retired or still pending. There is no arbitrary maximum number of touched files. A dozen thin, stable integration points can be appropriate; a dozen files each implementing part of the D1 rules is the failure this plan is intended to prevent

Reject new patches that scatter D1 constants through draw/collision loops, remap source art to the nearest D2 equivalent, inherit D2 defaults without a defined contract, or conceal large algorithms behind many tiny predicate helpers. Prefer complete data and cohesive operations. The practical success criterion is that the next D1-only correction normally changes a D1 owner and its tests, with an original engine caller changed only when a genuinely new boundary is needed

## 14. Execution plan from the current working tree

### What an original engine file may retain

Classify each remaining integration in the ledger before changing that domain. Use these dispositions instead of trying to achieve a particular file count:

| Disposition | Permitted engine-side code | Example and completion rule |
| --- | --- | --- |
| Consume data | Ordinary table access without D1 selection | Original lava frames, reactor wreck and non-blowable light metadata; remove superseded D1 branches once the data path is verified |
| Dispatch an operation | One selection at an existing phase, arguments, and handling of its result | D1 rotation or cockpit draw; no D1 arithmetic, coordinates or behavior-code interpretation remains beside the call |
| Provide an engine service | A narrowly useful primitive with ordinary engine types | Geometry sampling, bitmap registration or camera rendering; the service does not decide D1 policy |
| Adapt a format | Route a recognized format to its adapter | D1 save/level decoding; conversion bodies and ID maps belong to D1 owners |
| Transitional implementation | A named D1 body still awaiting extraction | Record its owner, removal package and missing verification; a renamed mode predicate does not retire it |

For each substantial operation, document its inputs, mutations, shared prelude/postlude, failure result, and inactive behavior in the domain header or adjacent implementation comment. Keep this documentation local and short. Record the caller and verification scenario in the ledger; do not build a second configuration system to describe hooks

### Next changes, with explicit removal targets

These are review-sized packages, not instructions to redo already completed work. Verify the existing working-tree changes first, then proceed in this order. Mechanical extraction and fidelity corrections should be distinguishable in review

**A. Establish the current baseline and close ownership ambiguities**

- Reconcile the ledger with actual callers, including format/version conditions, existing local D1 helpers, and diagnostics. Record present behavior separately from behavior verified against native D1
- The facade's former `prepare_mission_assets` / `prepare_level_assets` cycle has now been removed: mission and level entry use `d1_in_d2_load_mission_assets`, and the D2 HAM service never calls back into preparation. Preserve that direction while completing custom staging and failure handling; asset preparation must not initiate mission loading
- Keep low-level generation publication/release private to the facade and asset owner where possible. Classify the legacy `apply_*` family separately from native publication and name each remaining consumer before removal
- Acceptance: a caller can request a profile/level transition without knowing the asset loader order; every remaining D1 body has an owner and disposition. Existing tests establish the refactoring baseline, not fidelity acceptance

**B. Finish the original-resource and presentation boundary**

- Review and verify the current text/font switching work, then extend selected-profile resource resolution to titles, briefings, menu backgrounds, endings and music. Define mission override precedence explicitly; isolating base archives must not accidentally suppress legitimate custom resources
- Implement D1 cockpit/gauge drawing as a coherent operation in the presentation owner. The caller supplies the current view/cockpit mode and existing drawing context; the owner selects original coordinates, masks and gauge behavior. Keep D2-only afterburner/layout assumptions out of this path
- Extract D1 briefing screen tables, palette choices and command interpretation from `titles.c` at a complete presentation phase. Share drawing/input primitives; do not add another mode branch for each briefing command
- Acceptance: matching native-D1 visual checks, D1 -> D2 -> D1 resource restoration, and custom -> stock resource cleanup. No new D1 layout constants or per-resource mode checks in original draw loops

**C. Demonstrate the first D1-only playable installation**

- Connect launcher readiness to the engine's supported D1 resource contract, with no D2 prerequisite. Keep archive/edition interpretation in native code and launcher staging in its existing owner
- Run an isolated installation with sound enabled through startup, First Strike level 1, ordinary movement/fire, door use, monitor destruction, reactor destruction and entry to level 2
- Verify original lava animation/colors, persistent reactor wreck and ordinary lights remaining intact. Retain the legacy destroyed-light palette conversion without selecting D2 destroyed-light art for ordinary D1 lights
- Acceptance: resource-open evidence shows no D2 base assets or loose substitutes were needed. A headless level load or `-norun` bootstrap alone does not finish this package

**D. Complete the bounded weapon and collision operations**

- Extend the existing weapon owner to cover the remaining homing target/eligibility, awareness and speed rules at their actual simulation phases. Preserve already extracted projectile accounting, damage initialization and smart-child behavior
- Move the `d1_in_d2_transparent_wall_*` bodies out of `fvi.c`. Keep collision traversal shared; expose only the UV/texture sampling operation required by D1 interpretation. Preserve sampling and transparency semantics before attempting simplification
- Collect related damage/drop/pickup and surface rules into meaningful calculations or actions in semantics. Do not force unrelated collision handlers through one oversized dispatcher
- Acceptance: native-D1 scenarios cover player/robot projectiles, homing and transparent walls; D2 results remain stable. Original callers no longer contain the extracted rules or D1-specific diagnostic formatting

**E. Consolidate AI by phase, with actor routing first**

- Establish one actor-policy selection that distinguishes ordinary D1 robots from an optional Guide-Bot. The current `d1_in_d2_use_d1_robot_aiming()` is only a mission-wide alias and is not sufficient for this contract
- Identify shared setup, owned behavior execution and shared completion in the robot-frame/firing/path callers. Move D1 scheduling and behavior bodies together with their visibility and firing decisions, retaining shared geometry/path storage
- Bring the AI-specific predicates currently in semantics into the AI owner when their callers move. Keep the AI header centered on phases rather than publishing every internal decision
- Acceptance: trace-controlled native-D1 comparisons cover sleeping/alerted robots, firing, chasing, doors, bosses and multiplayer bookkeeping where applicable. Demonstrate that shared work executes once and RNG draws remain in the intended order

**F. Add the level/progression owner and complete custom preparation**

- Create `d1_in_d2_levels.*` when extracting the substantive level/trigger/transition bodies. Preserve original texture IDs and orientation, and keep legacy converted-ID support inside explicitly named format adapters
- Have exit, secret entry/return, death and restore ask one D1 transition operation for the required action. Execute existing load/save primitives at their established boundary; do not let each caller reconstruct D1 routing
- Stage `.hx1` definitions before object verification, alongside the existing base and custom resource preparation. Use the same effective definitions for fresh load and restore
- Acceptance: source-ID/custom-content fixtures, malformed preparation, secret entry/death/restore/return and native checkpoint comparisons. Remove late repairs and duplicate conversion policy after their replacement is exercised

**G. Restore optional enhancements on top of the faithful baseline**

- Import Guide-Bot's explicitly required optional assets into owned extension slots and route that actor through its intended AI. Missing optional assets disable that feature, not D1 startup
- Add camera layout through the presentation owner and existing renderer. Keep original cockpit behavior as the baseline
- Acceptance: test neither feature, each feature separately, and both together. Merely installing D2 assets must not change D1 base pixels, sounds, tables or enemy behavior

**H. Retire the transitional paths and assess engine consolidation**

- Resolve every transitional ledger entry; remove unused overlay state, obsolete helpers and misleading public APIs after checking save/replay consumers
- Run the edition/custom-content, lifecycle, save/replay and relevant network/platform matrices. Keep unsupported combinations explicit
- Acceptance: a new correction to an existing D1 domain normally changes that domain owner and its tests. Native D1 retirement remains a separate decision after fidelity evidence, not an automatic consequence of this refactor

Package C is the first end-to-end milestone; it must not wait for complete AI extraction. Packages D-F can be undertaken domain by domain after it. Package G depends on both stable resource ownership and actor routing. Review each package's original-file diff alongside its behavioral evidence: the architectural requirement is removal of distributed D1 implementation, not merely creation of more `d1_in_d2_*` files

## 15. Detailed extraction blueprint

### Current baseline and what this revision changes

The working tree already contains independent registered-D1 asset publication, early desktop profile selection, original text/font/menu resource selection, a substantial D1 cockpit owner, and partial weapon/AI/physics extraction. The ledger records their verification and limitations. Preserve and review this work; do not restart the project by building a second loader or cockpit implementation

The remaining architectural problem is concentrated in algorithms and lifecycle ownership. For example, `ai.c` uses extracted helpers while still implementing D1 scheduling and behavior transitions itself. The complete `d1_in_d2_transparent_wall_*` bodies formerly in `fvi.c` have now moved into semantics; the ledger records that bounded extraction and its native-engine comparison. Naming code after D1 is not the same as locating it in the D1 owner

The following contracts govern packages A-H. Function names identify current callers; proposed new API names can be finalized during extraction. Each slice must remove the corresponding original-file body in the same change that installs its replacement

### A1. Make the session transition unidirectional

Removed chain: `prepare_level_assets` called `load_mission_ham`, which invoked `prepare_mission_assets`. Although the latter retired the active D1 generation rather than recursing indefinitely, this made ownership of a profile switch depend on re-entering mission code. The implementation ledger records its replacement and verification; the wider preparation/publication contract below remains the target for custom staging and failure handling

Use one ordered transition owned by the facade:

1. The existing startup/mission/level caller supplies the requested content profile and resolved level/custom sources
2. Prepare the destination data using that profile's loader. D1 preparation includes definitions used by object verification
3. Stop old audio and release old world/cache consumers at the established unload boundary
4. Retire old resources and publish the destination definitions
5. Return a clear result so the caller can decode the level and execute normal shared initialization

For the D2 destination, expose a narrow D2 definitions-loading operation if needed; it must not itself decide whether to switch profiles. Keep mission enumeration, UI, world decoding and their failure handling in their existing callers. Preparation and publication should not be hidden in an ambiguously named boolean query

Document the exact ownership transfer of model bytes, bitmap bytes and sample buffers in the private asset header. The facade orders release; the subsystem owning each allocation frees it. A reset must not initiate a destination-game reload. Consolidate diagnostics around the active profile and generation rather than maintaining public overlay counters indefinitely

Evidence: instrument one D1 -> D2 -> D1 transition, a same-profile next-level load and failures before/after publication. Check initialization counts, resource provenance and cache invalidation, not merely successful return codes. Remove the nested preparation chain and redundant public entry points once their callers migrate

### B1. Complete presentation without per-command engine branches

Retain the existing cockpit dispatches in `game.c`, `gamerend.c` and `gauges.c`. They are a reasonable boundary: D1 owns layout and masks, while the engine owns rendering a view. Review fullscreen HUD, reticle and camera handoff as adjacent operations; do not fold the whole renderer into the cockpit module

The next substantial extraction is `titles.c`: D1 briefing screen tables, level/message selection, D1 command interpretation, screen advancement and ending selection. Start from `do_briefing_screens` and its event-handler lifetime. A dispatch must own the complete D1 briefing session, including state cleanup when the window closes; selecting an initial image alone will leave the algorithm distributed

Keep generic screen drawing, input/window delivery, sound playback and model rendering as engine services. Move the D1-specific message/coordinate/palette decisions together. Use `d1_in_d2_presentation.*` initially; split a private `d1_in_d2_briefing.*` collaborator only if the extracted state machine warrants it. Do not create one source file per resource type

Evidence: original level-1 briefing, a later-level briefing, secret-level text, final ending, skip/close/reopen, custom briefing replacement and D1 -> D2 -> D1. Compare resource identity, command effects and matching-resolution captures with native D1. Include a missing optional high-resolution resource while D2 is mounted to detect fallback

### C1. Separate engine choice from content requirements

The working tree now has `GameLaunchTarget`, separating executable from content, and `d1InD2Readiness.ready` follows D1 file readiness without requiring D2 files or an enabled mission ZIP. Preserve that separation. File presence, plus the existing Test Flight rejection, is not yet a complete native supported-edition/format preflight and does not establish a successful D1-only launch

Represent the launch request internally as executable choice plus content profile. For example, D2 executable + D1 content requires supported D1 resources; D2 executable + D2 content requires D2 resources. Keep the native D1 executable available as a reference. Use the existing selection UI and launch owner where possible; do not let every screen reinterpret a string called `game`

One readiness/staging owner should derive requirements from that request. Native code remains responsible for supported editions and format validity; Kotlin presents availability and stages files. Use the same result for the launch button, setup automation and mission filtering. An unavailable Guide-Bot asset set is a feature-availability result, not a missing base-game dependency

Evidence: launch D1 content in the D2 executable with only registered D1 files staged; also test both installations present, D2-only, missing/corrupt D1 data, and an unsupported D1 edition. Complete the sound-enabled First Strike level-1-to-2 scenario from package C. Save attempted resource opens and selected executable/profile in the test result. Existing bootstrap and renderer harness evidence does not replace this gate

Include Android's mission mount/reset lifecycle in this slice. The unconditional `groupa.pig` / `groupa.256` baseline in a D2 build has now been replaced by `d1_in_d2_restore_base_resources`; the ledger records an isolated Android First Strike launch and host restoration checks. Keep Android mount ownership in the existing Android service. Complete reset-before, activation failure, free-mission, menu return and shutdown coverage as well as successful mission entry. Do not repair additional gaps with separate D1 resource branches at every reset caller

### D1. Extract weapon phases and wall interpretation

| Current integration | D1 operation to own | Shared engine work retained |
| --- | --- | --- |
| `Laser_create_new` | Player projectile initialization and D1 creation/awareness rules | Allocation, placement and ordinary object lifecycle |
| `track_track_goal`, `find_homing_object*` | D1 target eligibility, reacquisition schedule and target choice where divergent | Object iteration/visibility primitives that do not encode game-specific eligibility |
| `Laser_do_weapon_sequence` | Existing extracted steering plus remaining D1 speed/lifetime rules at their current phases | Lifetime bookkeeping, collision integration and object removal where behavior is shared |
| `create_smart_children` | Parent-sensitive child definition and D1 child behavior | Shared object creation and sound services |
| `fvi_sub` / extracted semantics operation | D1 wall flags and point transparency interpretation, including native closed-door pixel behavior | Intersection traversal, segment geometry, UV calculation and texture services |

For homing, first trace acquisition -> eligibility -> reacquisition -> steering -> speed handling. Move connected D1 decisions together; a single steering hook does not own targeting. Preserve the positions of frame counters, fixed-point operations and random draws. Do not evaluate a stateful D2 fallback before entering the D1 operation

For transparent walls, `find_hitpoint_uv` is already declared in `fvi.h`, and bitmap paging, texture merging and RLE expansion have callable services. Begin by moving the existing D1 wall functions using those services; a new sampler abstraction is not automatically necessary. Keep the exact pixel/wrapping semantics during extraction. Move D1 diagnostic formatting with the owner or the existing replay adapter, retaining a small observation hook where shared traversal supplies needed facts

Evidence: closed/open doors, transparent and opaque pixels, overlaid/rotated textures, RLE textures, player/robot homing projectiles and retargeting. Compare controlled native-D1 outcomes separately from before/after extraction traces. Remove the local D1 wall functions and superseded homing branches

### E1. Own robot behavior across all its callers

The current `do_ai_frame` prelude already contains route-confirmation and network companion ownership handling. Do not bypass it by inserting an early D1 return without classifying it. Likewise, a mission-wide `use_d1_robot_aiming` check would incorrectly classify an optional Guide-Bot

Work through these smaller slices:

1. **Actor routing and initialization:** define ordinary D1 robot versus optional companion using active definitions/role, not a magic robot index. Review `init_ai_object`, `init_ai_objects` and calls outside `do_ai_frame`, including hit reactions and path requests
2. **Scheduling and perception:** move D1 skip-count handling, awareness/visibility decisions and frame gating as a coherent phase. Keep visibility cache state local to the operation and preserve the exact point at which it is populated
3. **Combat and movement:** move D1 firing readiness, gun selection, burst timing, aiming and behavior-state movement through the AI owner. Bring related semantics helpers into that owner; wrappers around each existing branch are transitional only
4. **Path and boss policy:** move D1 destination choice, path-following behavior and boss-specific rules. Reuse path storage, allocation/garbage collection and genuinely shared geometry. If random traversal order differs, the D1 owner must control that order rather than delegating to a helper that silently uses D2 RNG sequencing
5. **Close the dispatch:** review all original AI/path callers again, remove redundant D1 predicates and make helper functions private once only the D1 owner calls them

For every phase, classify prelude/postlude as shared, D1-owned or D2-owned before editing. Record which side performs timer advancement, animation, network position updates and early-return accounting. Prefer ordinary arguments and existing runtime structures. A context struct is justified only for state genuinely shared across the extracted phase, not to export all of `ai.c`'s local variables

Evidence: dormant and alerted robots, skip-return recovery, still/chase/run-from behavior, cloaked player, doors, firing, bosses and companion ownership. First compare pre/post extraction on identical controlled input and RNG state; then compare intended D1 behavior with the native engine. Keep those two claims separate so an existing fidelity bug cannot become the acceptance oracle

### F1. Put progression, doors and format conversion under explicit ownership

Create `d1_in_d2_levels.*` for substantive level/trigger/progression behavior. The facade owns asset/world transition ordering; this module owns D1 rules about which transition is requested. This prevents two competing session coordinators

| Original callers | Target owner and retained boundary |
| --- | --- |
| `gamemine.c` texture decoding and `gamesave.c` verification | Level adapter owns D1 reference interpretation; shared reader still handles ordinary fields/version framing |
| `switch.c`, `EnterSecretLevel`, `AdvanceLevel`, death/restore paths | One D1 transition operation selects destination and required score/save/briefing actions; existing services execute those actions once |
| `wall.c` door-hit and `wall_frame_process` rules | Level owner owns substantive D1 door timing/blocking policy; retain shared animation/storage. A small isolated calculation may remain in semantics if it does not split that policy |
| `d1_custom.*` and asset preparation | Stage custom definitions and references before verification, preserving source namespace and native precedence |
| `d1_save_translate.*` and replay adapter | Decode serialized state and invoke the same level/reference/lifecycle rules as a fresh load |

`EnterSecretLevel` currently calls `state_save_all` before its D1 branch. The D1 dispatch must occur before the first D2-only side effect, while preserving required shared window/audio handling. Moving only `StartNewLevel` selection leaves the progression defect in place

Do not relocate every historical file-version comparison. Shared structural decoding can stay in the reader; D1 behavioral conversion and repair policy belongs in the adapter. Legacy converted IDs need an explicitly identified serialized-format consumer before their conversion map can be retained or removed

Evidence: normal exit, secret entry, death in secret, save/reload there, return, ending, custom -> stock and malformed custom definitions. Check destination, player inventory/score, triggers, doors and active morph state. Fresh load and checkpoint restore must use the same effective definitions

### G1. Keep small rules cohesive and enhancements optional

Audit remaining D1 rules in `collide.c`, `fireball.c`, `object.c`, `powerup.c` and `physics.c` by operation: damage scaling, impact response, drop generation, pickup amount and surface response. Prefer correcting imported metadata where it fully specifies behavior. Move substantial drop construction into a D1-owned action if needed; do not leave a D1 algorithm in the caller surrounded by helper predicates

Keep semantics limited to small related rules and the currently bounded physics operations. Split another module only when an extracted body earns it. There is no requirement for a D1 counterpart to each original file

For Guide-Bot, first make its resource closure explicit: model, texture references, robot definition, required sounds and supporting clips. Import it without initializing the entire D2 game. Keep availability/spawn ownership separate from base asset publication, and route that actor using E1. The facade can expose a feature operation without becoming the implementation home for model rendering or companion AI

Evidence: no enhancements, cameras only, companion only, both, and missing companion assets. D1 base resource identities and ordinary enemy rules must be identical across those combinations. Retain the destroyed-light palette conversion as a supporting asset conversion; ordinary D1 lights remain unbreakable through imported metadata

### Review and stopping rules

For each slice, add a concise row to the implementation ledger: original caller, new owner, moved body, retained service, state/RNG effects, test evidence and remaining temporary integration. Review the original-file diff before the new module: it should show the D1 implementation disappearing, leaving an understandable call at the relevant phase

The first end-to-end milestone is C1 after the lifecycle boundary is sound and the existing presentation is sufficient. It does not depend on completing all AI/briefing extraction. Follow with domain-sized D1/E1/F1/B1 improvements; do not interrupt every domain change to invent another global framework

An architectural slice is complete when its caller no longer implements the extracted D1 rules, its API describes a meaningful operation, and the relevant tests exercise that boundary. The full project is complete only after the fidelity, D1-only installation, lifecycle and persistence gates pass. Native D1 retirement remains a subsequent decision

## 16. Concrete ownership decisions and review sequence

### Current review sequence

These are the detailed D/E review units, subordinate to section 0's finishing sequence. Establish strict baseline replay evidence in F1/F2 before treating optional-resource completion as the next fidelity milestone. The current working tree already contains substantial extraction and additional uncommitted optional-resource work. Review and finish those boundaries; do not recreate them from the earlier audit. Source inspection establishes that an implementation exists, not that it passes its acceptance gate

| Change / dependency | Owner and concrete work | Original-file boundary and removal | Acceptance before proceeding |
| --- | --- | --- | --- |
| 1. Review staged extension publication / D2-D3 | `assets`, private `guidebot` and `bitmaps`: validate deterministic remapping, base/custom count stability, palette conversion, sample rates, reference widths and all ownership transfers | Review the existing `digiobj.c` translation hook and generic robot-record changes. No optional package discovery in audio or model drawing | Publish -> retire -> stock -> publish; malformed/capacity rejection leaves the active generation unchanged; native definitions, sound maps and pixels unchanged. Rebuild the latest source, not only an earlier revision |
| 2. Connect one production load path / 1, D1-D3 | Facade resolves an optional package and attaches it after custom preparation, before combined validation/publication. Asset collaborator owns source identity and mounts needed to read that package | Keep startup/mission callers using their existing facade operation. Remove the need for a prior D2 session or live-table capture; do not add another caller-level loader sequence | Cold D1 with optional content produces the same definitions as D2 -> D1. Missing, corrupt and oversized optional input leaves a playable baseline and an explicit unavailable-feature result |
| 3. Complete a working companion / 2, D4 and E4 | AI owns spawn/actor policy; assets supplies prepared IDs; levels owns exit meaning. Specify goal sound, flare and morph behavior together with the actual actor flow | `ai2.c` creates an object from a prepared identity; `ai.c`/`escort.c` invoke narrow owned operations for the remaining semantic decisions. Remove fixed source-slot assumptions and obsolete `ensure` mutation | Spawn, morph, animate, draw, navigate, open a door and play goal/robot sounds. Native enemies and player weapons retain their D1 behavior; compound exit queries work |
| 4. Close saved identities / starts during 1-3, E3 | Existing save/demo/rewind adapters and assets record the definition identity needed to interpret runtime IDs, then restore through the same load path | No new restoration policy in AI, drawing or audio consumers. Remove namespace guesses based on numeric range | Actual save/load and replay/rewind execution with companion present/absent, custom definitions and missing optional source. Original D2 format contracts remain explicit and verified |
| 5. Delete the superseded architecture / 2-4, E1-E2 | Assets/custom/bitmap owners retire old `apply_*`, live capture, backups and append-on-spawn state after migrating each caller and its tests | Remove old model/draw/reset bookkeeping in the same connected change; `piggy.c` retains generic registry/cache work and only a justified lifetime notification | Consumer inventory shows one production loading path; retained destroyed-light conversion has a direct test; repeated profile/custom transitions and teardown have no stale users |
| 6. Close the remaining matrix / 1-5, E4-E6 | Existing owners finish travel/network consumers, lifecycle failures, public-interface review and declared edition/platform support | Original-file diff contains dispatch, format framing or neutral services; remaining transitional bodies each have an explicit unresolved status | Native-D1 comparison, D1-only interaction, optional-feature matrix and ordinary-D2 regression. Native engine retirement remains a separate decision |

Keep changes 2-4 as one connected delivery milestone: package loading without a usable, restorable actor does not complete Guide-Bot support. Each review may be smaller, but it must leave the existing base game usable. Run relevant serialization checks while changing representation, before relying on that representation elsewhere

For every change, present the original-file diff first, followed by its owner implementation and evidence. The review must answer: which D1 decision tree disappeared, which permanent engine call remains, and where would a future correction to that rule be made? An increase in compatibility-file line count is acceptable when it buys a complete, understandable operation; an increase in scattered policy is not

### Keep the architecture small

The intended shape is direct C calls, existing engine tables and a few domain owners:

```text
Startup / mission / level entry -> d1_in_d2 session facade
                                    -> assets + bitmaps + custom adapters
                                    -> presentation + cockpit
Simulation phase entry          -> D1 weapons / AI / semantics / levels
Save and replay entry           -> format adapter -> same lifecycle and rules

All owners use existing engine primitives for rendering, audio, geometry,
object storage and serialization. Those primitives do not select D1 policy
```

The facade coordinates profile transitions, not every per-frame action. Weapons and AI call their domain operations directly. Presentation receives the active profile through the common session contract; it must not infer a different game from whichever archive happens to contain a filename. This is a dependency guide, not a requirement for an interface object or a callback registry

Make these specific ownership choices before further extraction:

| Decision | Owner and rule |
| --- | --- |
| Requested versus active game | Facade owns the startup preference and committed content profile. A mission descriptor can request a change, but allocating/freeing a descriptor must not accidentally change active rendering or reset behavior. Identify any transition interval where `Current_mission` and live tables disagree; do not cache a second mode flag in every subsystem |
| Content identity versus executable identity | Native facade selects content; Android `GameLaunchTarget` selects executable/content and passes the request. Engine identity still selects the library and engine settings. Mounted mission ownership is a separate Android concern |
| Profile versus actor role | AI owner routes a robot by its definition/role in the active generation. An optional companion does not make ordinary D1 enemies use D2 policy, and a D1 mission does not make the companion use D1 enemy policy |
| Definition versus mutable state | Asset owner retains validated source definitions; engine arrays hold active state. Animation advancement, custom replacement and restore must not mutate the source used to prepare the next level |
| Transition policy versus transition execution | Level owner decides D1 destinations and required actions. Facade owns resource activation; existing level services perform world loading. Do not create a second resource coordinator in the level module |
| Diagnostics | Owners supply provenance/state at existing introspection or replay boundaries. Diagnostic code must not introduce another interpretation of content identity or another live data owner |

Do not introduce a generalized session object holding every engine global. Add explicit state only for an actual lifecycle distinction, and document who changes it. Existing engine objects, AI state and canvases remain the working types

### Final integration boundaries, not just extraction destinations

The deliverable is a smaller amount of D1 knowledge in the original files. Moving arithmetic behind a function call is insufficient when the original caller still chooses and orders the D1 algorithm. Review the following final shapes alongside the removal register. These are responsibility boundaries, not a requirement for exactly one function per row

| Domain | Intended stable engine integration | D1 implementation that stays behind that boundary |
| --- | --- | --- |
| Session and resources | Startup, intro/level preparation and retirement notify the facade at their existing lifecycle boundaries | Edition/resource choice, staging order, publication, active identity and cleanup ownership |
| Robot simulation | Initialization dispatch; one ordinary-enemy frame dispatch after required shared entry guards; separately callable AI operations only where real external callers need them | Frame ordering, native behavior interpretation, perception, awareness reaction, scheduling, movement and firing decisions |
| World AI updates | Dispatch at the existing awareness/boss update boundary where that work is outside the per-object frame | D1 propagation depth, agitation and boss policy; explicit treatment of the optional companion |
| Weapons | Calls at creation, acquisition/retention, steering and other actual weapon lifecycle phases | Source weapon meanings, arithmetic, eligibility and D1 ordering within each phase |
| Briefings and endings | Select the D1 presentation session before creating a D2 briefing session | D1 screen table, command interpreter, palette/resource selection, animation state and close behavior |
| Cockpit and cameras | Viewport/setup and drawing dispatches at their existing renderer phases | Original layout, masks, gauge rules and camera-window placement |
| Level decoding and progression | Recognize source format, call its adapter; select transition policy before game-specific side effects | Source-ID interpretation, object/trigger conversion, secret routing, scoring/inventory decisions and D1 transition sequences |
| Small collision and physics differences | A calculation or complete response operation at the point where the engine needs its result | D1 scaling, position selection, damage/drop rules or rotational integration; no second collision/physics loop |

For example, the robot-frame destination should have this structural shape, with names finalized during implementation:

```c
/* Existing route/replica guards that must run before either policy */
if (d1_in_d2_ai_run_frame(obj))
    return;

/* Ordinary D2 frame implementation */
```

This sketch is a target, not a patch to insert above the current function. First account for every early return, position/fire publication, diagnostic and shared completion action. Here, handled means the D1 frame, including required completion, has finished; inactive means no mutation and no RNG use. If a genuinely common postlude must remain in the caller, branch to that common completion instead of returning. Avoid returning a dozen local variables for the caller to resume the D1 algorithm

The current `prepare_frame`, `prepare_behavior`, `schedule_frame` and perception operations make bounded migration possible. Once the connected frame has moved, preparation/scheduling/reaction helpers should become private to AI unless another legitimate engine entry point uses them. The public header should describe engine integration, not expose a transcript of the D1 frame's internal steps. Perception or firing entry points with independent callers can remain public; their presence needs a caller, not speculative reuse

Do not apply the robot-frame pattern indiscriminately. Weapon operations happen at different times in the engine lifecycle, and combining them would change behavior. Likewise, one giant `d1_in_d2_update_everything()` would hide lifecycle dependencies rather than simplify them

### What to share, and what to keep as a D1 implementation

Prefer a contained D1 implementation of a substantially different algorithm over a shared algorithm with a mode test at every decision. Adapt the native D1 implementation to current engine types and established Redux services; preserve relevant upstream comments and existing determinism/diagnostic improvements. Do not copy the whole D2 routine and then disable its D2 branches

Sharing is appropriate for mechanisms that do not encode the game distinction: object allocation/relinking, vector math, ray traversal, model drawing, audio playback and path-buffer storage. A D1 owner may call those services directly. It must not call an apparently shared helper that silently reselects D2 behavior or mission-wide AI policy. Classify such helpers during extraction and either make the mechanism neutral or retain a D1-specific algorithm in the owner

Apply that distinction concretely:

- `ready_to_fire` and D1 hide/follow behavior belong with AI decisions; replacing their checks with public `is_d1_*` helpers would leave the original problem intact
- AI door eligibility belongs to AI; serialized door/trigger interpretation belongs to levels; a small runtime door timing rule belongs to semantics. Reusing the word "door" does not make these one subsystem
- Generic path storage and garbage collection can stay in `aipath.c`. D1 destination selection, randomization and follow/replan decisions must move with their callers, including helpers currently selected through mission-wide predicates
- Native briefing command/state logic belongs to presentation. A generic bitmap/model draw primitive can stay shared, but a renderer helper must not secretly select a D1 screen number or palette
- The existing destroyed-light conversion belongs to retained optional-asset preparation. It neither enables ordinary D1 light destruction nor chooses the baseline D1 artwork

Do not extract services just to avoid a few duplicated lines. Require a real consumer and a small, stable contract. Do not introduce parallel D1 copies of live engine objects, a callback table for every static helper, an include of a `.c` file, or preprocessor-generated versions of the same engine source. A private frame-local structure is acceptable inside the AI owner if it improves readability; exporting all frame locals to the caller is not

### Original-file removal register

These are concrete review targets, not a claim that every remaining branch has already been classified. Use the ledger for the complete caller inventory as each domain is taken up

| Source and current implementation | Required removal / destination | Legitimate retained integration |
| --- | --- | --- |
| `ai.c`, `ai2.c`, `aipath.c`: scheduling, firing readiness, visibility, behavior and path branches | Move connected phases into `d1_in_d2_ai`; absorb AI-only predicates from semantics and make internal helpers private | Actor dispatch at phase entry, shared setup/completion, geometry/path services |
| `laser.c`: weapon phase dispatches | Current source already delegates acquisition/reacquisition, placement, awareness and speed in addition to steering/children. Verify this boundary and evidence; do not redo extraction from the earlier steering-only baseline | Object lifecycle, phase dispatch and handling of the operation result |
| `fvi.c`: transparent-wall functions and D1 diagnostics | Extracted into semantics using existing UV/paging/merge/RLE services; the unsupported blanket closed-door rejection was removed after a native-D1 comparison | Collision traversal, one wall-policy decision and replay observation at the crossing boundary |
| `titles.c`: D1 briefing table and command/session branches | Presentation owns a complete D1 briefing session, including window-close cleanup | Dispatch at briefing entry and reusable drawing/window/input services |
| `gamemine.c`, `gamesave.c`, `switch.c`, `gameseq.c`: source-reference, trigger and progression policy | Substantive extraction earns `d1_in_d2_levels.*`; reuse it for live load and restore | Shared binary framing, world-loading services and transition dispatch |
| Android `android_mission_asset_reset.c`: D2-build baseline PIG/palette selection | Baseline resource selection now delegates to the facade; remaining teardown/legacy-overlay ownership still needs review | Android mount lifecycle notifications and genuinely shared cache services |
| `piggy.c`: direct D1 bitmap-owner dependency | Inspect the exact remaining service before removing it; parsing and D1 source lifetime stay in assets/bitmaps | Generic registry/page/cache operations; a justified bounded ownership hook if necessary |
| `game.c`, `gamerend.c`, `gauges.c`: existing cockpit hooks | Keep these boundaries; move any remaining D1 layout/state bodies into cockpit rather than replacing sound dispatches | Canvas setup, drawing-phase dispatch and shared camera execution |

For an operation extraction, the review must show both the removed body and its new home. Adding a domain helper while leaving the decision tree in the caller does not complete the row. Conversely, do not move ordinary D2 code merely to reduce the number of files containing a D1 call

### Small changes in dependency order

Record the baseline against commit `7fe0516ade4d24c51cfc056210d391d20de6623f`, including the existing working-tree changes. Preserve unrelated edits. The table below is a sequence of review units, not a demand to finish every domain in one patch. A unit may need several changes, but each change must retire a named body or close a named behavioral gap

| Unit / dependency | Concrete work and owner | Original-file reduction required | Evidence before closing the unit |
| --- | --- | --- | --- |
| 1. Session identity and lifetime / existing asset generation | Facade distinguishes a requested profile from committed runtime content; trace startup, selection, cancellation, failed activation, baseline reset and menu return | Remove caller-level inference from archive presence or a temporarily allocated mission descriptor. Preserve the existing Android baseline delegation rather than adding another reset path | D1 -> D2 -> D1, failed selection before publication, failure after retirement, no-mission reset and shutdown. Check active tables, palette, fonts, sounds and live consumers together |
| 2. Isolated playable gate / 1 | Finish the existing registered-D1 Android runner; use graphics, sound and ordinary game actions through First Strike exit to level 2 | Repair discovered dependencies in their D1 owner; do not add a resource exception at every failing caller | Only D1 base files staged; attempted resource opens recorded; normal doors, lights, monitors, lava, reactor wreck and level transition checked. Controlled poses are interaction coverage, not proof of a complete maze traversal |
| 3. Transparent-wall operation / baseline captured | Implemented in semantics, including D1 diagnostics; see ledger for the separately evidenced closed-door correction | Original bodies removed; crossing dispatch and shared intersection traversal retained | Shared native-D1/D2 fixture covers closed/open doors, transparent/opaque pixels, rotated overlays and RLE; real-data and Android checks recorded in ledger |
| 4. Weapon operation closure / 3 for collision-sensitive cases | Named base operations implemented and verified; see ledger. Weapons owns creation policy, target eligibility/reacquisition and sequence-phase speed/lifetime behavior | Connected D1 decision trees removed from `Laser_create_new`, targeting helpers and `Laser_do_weapon_sequence`; phase dispatch and ordinary object lifecycle retained | Player/robot projectiles, Spreadfire accounting versus projectile, smart children, homing loss/reacquisition and fixed-point/RNG ordering. Wider collision/network/edition gates remain separate |
| 5. Robot role and initialization / 1 | AI owner identifies ordinary D1 enemies versus an optional companion from active definitions and owns D1 initialization | Retire the mission-wide `d1_in_d2_use_d1_robot_aiming` alias as the actor selector; remove behavior-code interpretation from original initialization callers | Same D1 enemy behavior with companion disabled/enabled; correct role after load/restore; shared network and route bookkeeping executed once |
| 6. Robot phases and frame closure / 5 | Extract scheduling/perception, combat/movement and path/boss policy into AI; then consolidate the native-enemy frame and privatize migration-only phase helpers | Remove each extracted phase across `ai.c`, `ai2.c` and `aipath.c` in the same slice. Close with an owned D1 frame, not a D2 frame still ordering many D1 helpers | Dormant/alerted/cloaked-player cases, skip returns, chase/run-from/still/hide/follow behavior, firing, doors and bosses; native comparisons, completion/RNG evidence and final caller/header review |
| 7. Level and custom-definition adapter / 1 | Create levels owner for substantive reference/trigger conversion; assets and custom adapter stage `.hx1` before publication and object verification | Remove faithful-path nearest-D2 mapping and late object repair; retain only explicitly identified serialized-format conversion consumers | Original texture IDs/orientation, custom robot/model definitions on fresh load and restore, malformed custom data, custom -> stock cleanup |
| 8. D1 progression / 7 | Levels owns normal/secret exit, death/return and ending decisions; facade remains the sole resource coordinator | Remove D1 transition algorithms from `switch.c` and `gameseq.c`; move dispatch before D2-only secret-save side effects | Destination, score/inventory, triggers, secret entry/death/save/reload/return and final campaign ending |
| 9. Briefing and presentation sessions / 1, with 8 for ending coverage | Presentation owns the D1 briefing session, resource/layout/command decisions and close-time cleanup; retain cockpit owner | Remove D1 tables and command branches from `titles.c`; preserve generic drawing/window/audio services | First/later/secret briefing, ending, skip/close/reopen, mission override, missing optional hires resource with D2 mounted, D1 -> D2 -> D1 |
| 10. Remaining small gameplay rules / relevant 3-8 operations | Assign damage, drops, pickups and surface response to semantics or the already established domain owner; audit hardcoded IDs as well as named D1 checks | Remove local D1 calculations/actions in collisions, explosions, objects, pickups and physics. Correct imported data first where sufficient | Focused native-D1 scenarios for each changed rule and stable D2 results; no unowned cluster of D1 predicates remains |
| 11. Optional feature resources / 1, 5-6 and stable presentation | Assets owns the explicit Guide-Bot resource closure; AI owns actor routing; cockpit owns camera layout; shared engine renders cameras | Remove reliance on a prior D2 initialization and fixed D2 slot contents. Do not scatter companion exceptions through enemy logic | Neither feature, cameras, companion, both; missing optional assets; unchanged D1 base pixels, sounds and ordinary enemies with D2 installed |
| 12. Persistence and final retirement / all affected owners | Save/replay adapters consume the same definitions, conversion and lifecycle; complete edition/platform and applicable network checks | Delete obsolete overlay state, unused public helpers and transitional repairs after naming all remaining consumers | Fresh versus restored state, supported serialized IDs, repeat profile/custom transitions, supported editions and available platform CI. Resolve every transitional ledger row |

Units 3-10 should remain domain-sized. Units 7-9 can move ahead of AI completion when campaign testing exposes a dependency; a progression fix that blocks unit 2 belongs in the levels owner immediately. Unit 12's relevant save/replay checks run alongside each earlier change, not only at the end. Native D1 retirement is a separate decision after this evidence exists

### Operation cards and concrete change boundaries

Each card is a bounded implementation task. Its removal target belongs in the same change as the new owner operation. Proposed operations describe responsibilities; choose final signatures from the actual caller rather than treating these cards as a framework specification

### Current delivery plan: finish operations, then retire migration interfaces

Planning refresh: 2026-09-21. This is the remaining delivery sequence within units 7-12, not a new parallel project. Preserve the implemented asset, weapon, wall, cockpit and AI boundaries. Their wider validation gaps stay visible in the ledger; they do not justify restarting extraction or declaring the whole feature complete

Implementation update: A1-A3's native record conversion, action execution, crossing completion and connected single-player campaign operations now belong to levels. The ledger records the integration map, 3,072 flag/layout round trips, shared native action scenarios, actual checkpoint evidence and 19 matching campaign observations. D2's ordinary save path retains the native trigger record. Legacy single-type D1 saves still reach the retained fallback exit branches, and network/co-op interception, companion queries and ordinary-D2 secret travel require further closure. Do not repeat the implemented extraction or treat these narrower results as completion of units 7-8 in every mode

B's owned session is now implemented: `d1_in_d2_briefing.c` owns native screen tables, interpreter, animation, resources and cleanup, while `titles.c` dispatches at briefing/ending/order-form entry. The native-only title-option compile error is resolved without adding a D2 option. The ledger records 84 exact native/imported rendered frame comparisons with D2 absent and present, malformed-input cleanup, host suites and campaign/profile regressions. Rendering exposed and corrected indexed-model color loss and loose-art mount precedence. B1-B4 below remain the implementation/coverage contract; continue with C rather than re-extracting presentation. Shareware/OEM/Mac, native-only cheats, audible output and network briefing behavior still require separate coverage or explicit scope decisions

The architectural acceptance question is concrete: after this work, changing D1 trigger behavior, secret routing or briefing placement should normally require editing its D1 owner and tests. Original engine callers should neither interpret D1 flags nor order a D1 sequence of small helpers. Necessary dispatches can remain in several engine files because loading, simulation, restoration and rendering occur at different times

| Delivery slice | Implementation owner | Original-source changes allowed | Completion gate |
| --- | --- | --- | --- |
| A. Lossless trigger interpretation, execution and connected travel / units 7-8 | `d1_in_d2_levels.*`; format adapters delegate to it | Replace conversion/dispatch bodies in `gamesave.c`, `switch.c`, `gameseq.c`; narrowly expose existing engine services where necessary | Compound triggers survive fresh load and restore; native normal/secret/death/end transitions work without D2 secret snapshots; removed bodies and all retained hooks reviewed |
| B. Complete briefing lifetime / unit 9, implemented with wider limits in ledger | `d1_in_d2_presentation.*` and private `d1_in_d2_briefing.c` | Session-entry dispatch in `titles.c`; genuinely reusable drawing services | Owned tables/interpreter/cleanup; native/imported pixels, custom art, completion, interruption, failure and reopening evidence |
| C. Remaining rule inventory and closure / unit 10 | Existing weapons, AI, levels or semantics owner according to the rule | Calculation or response dispatch at its actual simulation phase | Every remaining D1 policy body has a named owner; native comparisons cover changed behavior and ordinary D2 remains stable |
| D. Optional enhancement resources / unit 11 | Assets for resource closure, AI for companion behavior, cockpit for camera placement | Existing feature activation and drawing boundaries | D1 base resources remain identical with D2 absent/present; missing companion resources do not prevent D1 play |
| E. Lifecycle, serialization and interface retirement / unit 12 | Facade plus existing format/domain owners | Remove obsolete hooks; retain justified format and lifecycle integration | Save/replay/rewind, supported editions, platform and applicable network checks; no unexplained transitional hooks or public phase helpers |

Run relevant persistence and ordinary-D2 checks in every slice. E closes the remaining matrix; it is not permission to defer save correctness until the end. Keep native D1 available as the comparison implementation throughout

#### A1. Preserve trigger meaning at the format boundary

The pre-extraction source had two incompatible reductions of D1 flags to one D2 trigger type: the old-level conversion in `gamesave.c::load_game_data` and `d1_save_translate_trigger_type`. Native D1 can execute several action flags on one activation. Fresh loading and restoration now use the same lossless level-owner conversion; the requirements below document its contract and remaining consumer review

The level owner should accept a decoded source record plus an explicit format identity, validate counts and links, and produce the engine-facing representation. Binary readers can stay with the loader/save adapter. Validate the original wide link count before narrowing it. Preserve action combinations, value, timer and source state; do not silently translate an unsupported combination to an ordinary door

Choose and document the representation in this first change, after tracing all consumers. Prefer a representation that travels with the existing trigger record, if it can preserve the established serialized contracts clearly. Do not casually repurpose padding or introduce a parallel global trigger table: either choice requires explicit save, rewind, legacy writer, network, level-reset and failure handling. If auxiliary storage is necessary, it belongs solely to levels and needs those lifecycle hooks in the same change. Runtime action semantics must not depend on a representative D2 `type` used by a UI or route query

Required consumer review: `trigger_read`, `trigger_write`, swapped reads, raw trigger saves, native checkpoint translation/validation, demo exit queries, multiplayer trigger delivery/disabled state, and Guide-Bot route queries. Old D2 v29/v30 records also use flags; file version alone must not label them native D1. Preserve their existing conversion through an explicit legacy path

Removal target: the duplicated D1 action-priority trees and source-state interpretation leave both live loading and save translation. The checkpoint adapter reads bytes and invokes the common conversion; it does not gain another gameplay policy

#### A2. Own activation and crossing completion together

Use one owned activation operation, callable from both local crossing and replay/network delivery. Keep crossing-only eligibility and paired-wall completion in a cohesive owned crossing operation where required. These are distinct entry points because remote activation need not have a local crossing object; they are not one public helper per action flag

Before the first dispatch, write a short phase contract accounting for existing player/trigger bounds, connection state, Android co-op travel, route confirmation, demo recording, diagnostics and notification. Shared guards and required completion run exactly once. The D1 dispatch must precede D2-only disabled/one-shot/type semantics. Use an explicit result when the caller needs to distinguish not applicable, handled, transition and rejected input; a failure must not execute the D2 fallback

Native source executes local shield damage, normal exit, secret exit and energy drain in order, followed by linked doors, matcens and illusion actions, with an early return on secret travel. Preserve that order and its local/remote restrictions. Call neutral wall/matcen primitives; a D2 convenience routine that adds a D2 sound or HUD message is not automatically neutral

One concrete fidelity trap: current native `check_trigger_sub` does not gate execution on `TRIGGER_ON`, while crossing one-shot bookkeeping clears that flag on the paired trigger. Do not infer runtime semantics from the flag's name or translate it automatically to D2's `TF_DISABLED` gate. Compare repeated activation and paired-side state with native D1 before deciding the mapping. Retain bounds safety rather than copying native unchecked accesses

Removal target: native action interpretation, ordering and one-shot bookkeeping move to levels. `switch.c` retains shared entry/completion and ordinary D2 execution. No new D1 action switch should appear in the engine caller

#### A3. Move the connected campaign transition, including death

Treat A1-A3 as one connected delivery slice, with smaller reviewable changes only where they leave a working path. Native secret-trigger execution calls level completion with a secret destination; D2 `PlayerFinishedLevel` currently asserts that this request is false. Connecting the trigger to that existing D2 routine without its D1 completion operation is incomplete

| Entry to inspect | D1-owned operation | Engine work to retain or expose |
| --- | --- | --- |
| `PlayerFinishedLevel`, `AdvanceLevel` | Hostage/score ordering, secret versus ordinary destination, campaign completion and D1 transition sequence | Existing level start, score display, window/timing and network synchronization services |
| `EnterSecretLevel`, `ExitSecretLevel` and their callers | Route any native entry through the same D1 progression owner; select policy before D2-specific audio or snapshot operations | Ordinary D2 secret implementation remains for D2 |
| `DoPlayerDead` | D1 consequence of death in an intact or destroyed ordinary/secret mine, destination and inventory/life treatment | Common death teardown, network authority, respawn/world-load services and final sound/window completion |
| `DoEndGame` and ending entry | D1 ending/high-score/menu sequence, with presentation owning the actual briefing session | Shared window, input and score services |

Do not remove secret-entry snapshots while leaving death or return dependent on them. Move that connected lifecycle together. The levels owner calls existing services directly; it does not produce a generic list of commands for `gameseq.c` to interpret, and it does not become a second asset loader. The facade still owns resource preparation/publication

Establish the smallest services needed by reading native D1 progression beside the current D2 implementation. Keep original D2 functions intact where possible; expose an existing neutral operation through its natural header only for a real caller. Do not export every file-static variable or copy the entire D2 progression file

Acceptance uses the same scenario inputs in native D1 and imported D1: each supported secret entrance, repeated trigger activation, exit from a secret, death before/after reactor destruction, last life, normal level completion and final campaign ending. Compare destination, score, lives, inventory, hostage credit, trigger state and required completion effects. Save/reload before and after secret travel must match uninterrupted play. Check that D2 secret snapshot files are neither read nor written by the native path

Add compound/non-exit trigger cases, malformed link counts and missing paired sides. Exercise fresh level load, native checkpoint import and the D2 executable's own save/restore or rewind path. Run an ordinary D2 secret-level regression. Network and companion behavior need explicit cases or a recorded coverage limit; a single-player trace does not close those consumers

#### B. Finish one owned presentation session

Use the existing `d1_in_d2_presentation.h` as the engine-facing interface. `d1_in_d2_briefing.c` is a private implementation within that domain; it does not need a public header or one API per briefing command. Presentation chooses resources, briefing owns its window/session, and cockpit owns gameplay layout. These lifetimes justify the three implementation files

| Step | Concrete work | Required result |
| --- | --- | --- |
| B1. Close the port/build gap | Resolve the native-only title-skip option against the actual D2 entry contract. Audit other imported native globals, helpers and edition assumptions, including Mac palette handling and native-only cheats | A compiling bounded port with explicit supported behavior; no new flags scattered through original D2 startup code |
| B2. Review session ownership | Trace entry, first screen, each command, page changes, robot/portrait animation, skip, close and reopen. Keep command position, timing, screen selection, palette, canvases and owned bitmap/text buffers private | `titles.c` invokes a complete session and handles the result. It does not interpret native commands or manage native session fields |
| B3. Close failure paths | Decide required versus optional resources once in presentation. Missing optional hires art falls back to original D1 art. Truncated commands, failed text/image loads and interrupted animation use the same owned cleanup path | No D2 resource fallback, half-created D2 session, stale canvas or dangling generation reference after failure |
| B4. Prove presentation and removal | Compare native D1 and imported D1 at controlled briefing pages/frames; exercise real window entry and cleanup. Inspect the original-file diff and remaining public declarations | Original D1 tables/interpreter bodies absent from `titles.c`; successful/failed session ownership and ordinary D2 briefings verified |

Use the existing window/input and rendering services for comparison fixtures rather than exporting parser internals solely for tests. Cover first, later, secret and final screens, mission text/art overrides, portrait/robot animation, skip/close/reopen, and D1 -> D2 -> D1. Match renderer, resolution and palette when comparing pixels. Keep timing/input observations separate where frame scheduling makes a pixel comparison unsuitable. The existing campaign runner advances briefings but does not verify their appearance

Resource precedence is part of this boundary: explicit mission/loose overrides, then original resources of the selected D1 edition. An unrelated mounted D2 archive must not supply a same-named background. Preserve the original low-resolution asset when an optional high-resolution D1 variant is absent

#### C. Remove remaining distributed gameplay policy by coherent operation

Implemented rule groups: native automatic-door obstruction/wait behavior, pickup amounts, contact/lava/blast difficulty scaling, replacement drops, exploding-robot contact and bounce bookkeeping belong to semantics. Camera awareness and flash/boss-blast rules belong to AI; native bouncing-projectile wall eligibility belongs to weapons. The source texture generation proves water/force-field mode guards redundant, so they are removed. The reusable gameplay runner compares the first group's 64 door, 30 pickup, 30 damage and 2,160 replacement-drop cases plus 584 surfaces, 32 motion cases, eight robot contacts and 96 robot blasts against native D1. It also exercises ordinary D2 after installing its real asset bank. See the ledger for exact evidence and limits

The named unit-10 extraction inventory is implemented. Semantics also owns native robot-pair eligibility and the final-physics diagnostic; rotation/hit/bounce select by actor inside the owner, preserving optional companion physics. Correct robot metadata removes the extra native death-blast guard. The independent resource-suppression helper remains intentionally small. Added comparisons cover 16 robot-pair intersections, 36 actual resource drops, 24 native secondary explosions and companion physics across content switches. The remaining transparent-wall dispatch invokes an existing complete native operation. Broader simulation/content/network fidelity remains explicit; these focused comparisons do not claim exhaustive behavior. Continue with D's optional generation, and E's lifecycle/serialization/network/interface closure. The cards below retain operation contracts; completed bodies do not need another extraction

First classify the remaining bodies, then extract one connected rule at a time. These are concrete source-review targets from the current tree, not a claim that each predicate is wrong. Search format/version branches and hardcoded IDs as well as explicit D1 predicates

| Current source/body | Destination and intended boundary | What stays in the engine / evidence |
| --- | --- | --- |
| `powerup.c::pick_up_energy` and shield pickup: trainee boost arithmetic | One small pickup-amount calculation in semantics, called by both consumers with their actual inputs | Inventory caps, messages, scoring and network publication stay at the existing pickup phase. Compare amounts across difficulties and near-cap inventory |
| `collide.c`: trainee collision/lava damage scaling and exploding-robot hit eligibility | Small named calculations/eligibility rules in semantics; use weapons or AI when the decision already belongs to their operation | Collision detection, damage application and shared publication stay shared. Preserve arithmetic order, invulnerability and local/remote effects |
| `collide.c` water response and `physics.c` force-field response | First prove correct native metadata excludes D2-only surface flags. If data suffices, remove redundant guards. Otherwise own the surface interpretation in semantics | Shared contact traversal, velocity integration and collision ordering remain. Exercise lava, ordinary walls, transparent surfaces and custom textures |
| `physics.c`: bounce bookkeeping plus rotation/hit dispatches | Retain already-owned rotation/hit operations; move any remaining connected D1 bounce decision as one rule at the bounce phase | Do not copy the physics loop. Compare velocity/orientation, repeated collisions and RNG state with native D1 |
| `wall.c::do_door_close` and door wait handling: native doorway-object scan and timing decision | A cohesive door eligibility/timing operation in semantics, using existing geometry services. Keep animation storage and advancement shared | Remove the inline D1 scan, not just its mode check. Cover both connected sides, linked door parts, blockers and close/reopen timing |
| `object.c::wake_up_missile_camera_robots`: mission-wide early return | AI owns the camera-awareness policy at this existing world-update boundary | Camera rendering stays shared and must not alter ordinary D1 enemy behavior merely by being enabled. Specify the companion's role separately |
| `switch.c` legacy single-type exit fallbacks and companion exit/door queries | Levels owns source-format interpretation and travel; AI/route consumers ask the level owner for the semantic result they require | Retain actual transport/route notifications. Old serialized records must be classified explicitly; do not reconstruct lost compound flags from one type |
| Remaining drops, explosions and reactor helpers | Review against existing semantics/weapons/levels ownership; keep a genuinely small independent helper small | Existing named hooks need no rewrite merely for uniform naming. Record mutations hidden by getter-like names and fix misleading contracts when touched |

For each row, read native D1 beside the current D2 path before choosing a signature. State whether the change preserves an already-correct extraction or intentionally fixes a fidelity difference. Avoid a giant `d1_policy` structure computed before the frame; calculations with RNG, mutable state or cache effects must run at their original phase

The closure test is source ownership, not number of files: after each change the original caller may supply existing context, invoke the operation and apply its result. It must no longer encode the D1 constants, scan order or paired decisions just moved into the owner. Preserve common prelude/postlude exactly once, including route/co-op guards and network events

#### D. Make optional enhancements independent of base content

Source preparation is implemented in the private `d1_in_d2_guidebot.c` asset collaborator. It reads explicit registered-v3 HAM/PIG/palette/S11-or-S22 paths, traverses the dependency set, validates selected model/image/sample payloads and owns unpublished storage. Source preparation does not capture or change live tables. Synthetic dependency/failure cases and registered-source comparisons are recorded in the ledger

The working tree now also contains extension remapping/publication, optional bitmap conversion, mixed-rate sample preparation and extended logical sound translation. Treat these as implementation awaiting complete integration and current-revision validation, not as pending code to write again or as completed actor support. `d1_in_d2.c::prepare_assets` still prepares base/custom definitions and publishes without attaching an optional package. Automatic package selection, actual actor integration, persistence and old-path retirement remain open

The old `d1_in_d2_prepare_guidebot_assets` still delegates to `save_d2_guidebot_assets`, which searches live robot/model tables. No production caller of that preparation entry point was found. `ensure_spawnable_guidebot` now checks the native generation first, but its fallback still contains the old append/capture architecture. Finish replacing that architecture rather than setting a legacy active flag or initializing D2 as an intermediate state

| Step | Implementation contract | Exit evidence |
| --- | --- | --- |
| D1. Establish resource identity | Assets reads optional companion definitions from an explicitly selected source. It must work on a cold D1 launch, without first playing D2 or publishing D2 base tables | Same feature availability from cold D1 start and D2 -> D1 transition; no dependence on leftover live tables |
| D2. Prepare the dependency set | Resolve robot/model/joints, model bitmaps, samples and required effect/weapon references into owned staging. Translate each referenced namespace and validate capacity before publication | Every reachable reference has documented provenance; missing/corrupt optional input leaves baseline D1 valid |
| D3. Publish and retire with the generation | Allocate extension slots deterministically without replacing native slots. Publish the feature atomically; release its users and buffers through the normal facade lifetime | Repeated enable/load/disable/profile switches produce no stale references, duplicate slots or base-table changes |
| D4. Route behavior by actor role | Existing AI owns the companion-versus-native-enemy choice; cockpit owns camera placement; shared engine draws the extra view | Native enemy traces remain unchanged with cameras/companion enabled, except intentional gameplay interactions with the companion |

The base product contract is D1 play and HUD cameras with D1 assets alone. The familiar Guide-Bot model/sounds require an available optional source; absence disables that feature with a clear availability result, never the D1 game. A new D1-only companion appearance is a separate asset/product decision. Do not silently substitute a D1 robot or require all D2 assets to unlock the engine's camera capability

Run the feature matrix with neither enhancement, cameras only, companion only and both. Repeat baseline cases with D2 archives absent and present. Compare original texture pixels/palette, animation references, sounds, robot/weapon definitions and source provenance. Optional assets may add resources but must not change the original lava, ordinary lights or reactor wreck

##### D implementation boundaries and removal targets

Keep this a resource feature owned by assets. A private `d1_in_d2_guidebot.c` collaborator is justified if the dependency reader and remapper would otherwise obscure the base D1 reader. It must not become another session coordinator or a public interface included by every companion caller. Keep its staging type opaque to original engine files; declare collaborator operations in the existing private asset interface. AI and cockpit retain their current responsibilities

| Work item | Concrete implementation | Original-file result and removal target |
| --- | --- | --- |
| Explicit optional source | Resolve the selected D2 resource package once, including its definition file, bitmap source/palette and sample source. Record source identity and use an isolated archive mount where needed. Prepare from file bytes into private storage | No call to `bm_read_all`, D2 startup or a live-table capture to acquire optional content. Mounted-file order must not select a different package accidentally |
| Dependency discovery | Start from the companion definition and walk models, simpler/dying/dead models, joints, object bitmap indirection, effects/vclips, weapons/children and sounds. Also inventory references made directly by companion behavior rather than by records | Replace `save_d2_guidebot_assets` and its singleton `D2_guidebot_*` copies. Do not spread dependency knowledge across `ai.c`, `escort.c`, `polyobj.c` and sound playback |
| Namespace allocation | Reserve deterministic extension ranges after the prepared base/custom definitions. Maintain typed maps for each referenced namespace. Validate every remapped index, sentinel, count and destination field width before publication | Remove fixed D2-slot reuse. Retain native D1 IDs and contents; do not add a runtime resolver to every render or collision |
| Bitmap/model conversion | Reuse validated bitmap decoding and palette conversion in the bitmap owner, passing explicit source and destination palettes. Preserve transparency markers. Validate model streams before engine normalization | Retain the destroyed-light converter in its current domain. Do not replace D1 lava or other base pixels. D2 model RGB flat colors must not be interpreted as D1 indexed flat colors |
| Sample preparation | Review the new `d1_in_d2_prepare_sound_output` against mixed S11/S22 sources, plain output-rate conversion, mixer source-rate retention and repeated publication. Validate logical/alternate references separately from physical samples | No companion sound remapping in the audio mixer. Original D1 logical sound slots must remain unchanged; the existing translation boundary delegates extended IDs to the owner |
| Publication | Review and connect the existing combined publisher. Assets validates the complete extension; the facade commits profile identity only after successful publication | Native `ensure_spawnable_guidebot` is now an availability check. Remove its remaining legacy append path and give the final query a name that does not imply loading or repair |
| Spawn and availability | Expose a small feature availability/result operation through the existing facade or AI boundary. The spawn path consumes the prepared companion ID; it does not read files or repair asset tables | `ai2.c::create_buddy_bot_at_position` keeps object creation and handles unavailable content explicitly. No second loader hidden inside spawning |
| Retirement | Use generation lifetime for extension bitmap/model/sample buffers and registration metadata. Stop live users before release, then clear feature IDs and availability | Delete old Guide-Bot capture, live-copy and reset bookkeeping once its consumers use the generation. Android reset calls the same lifecycle, without manually freeing another resource owner |

Preparation follows this order: base D1 -> mission/custom definitions -> optional dependency discovery and remapping -> combined validation -> publication. An absent, corrupt or oversized optional package discards the whole extension and yields a usable base generation with an unavailable-feature result. Required D1/custom failures remain errors. A failure after retiring the active world follows the normal unloaded/error path; it does not promise world rollback

Resolve HAM, PIG, palette and sound files as one coherent selected package. An isolated mount is a reading mechanism, not authority to change the base game's resource search order. Release temporary mounts on every success/failure path once resident preparation is complete; retain only the identity needed for diagnostics and restoration. Do not mix a loose HAM from one installation with a same-named palette/sample bank from another merely because both can be opened

Retain native source bounds independently of combined runtime counts. The extension increases `NumTextures`, `N_robot_types` and other engine counts; that must not make an invalid D1 level reference valid by pointing into an appended companion resource. Level/custom adapters validate D1 source IDs against their prepared base/custom namespace. Saved extension references use their explicitly identified namespace. Rendering and simulation can then consume already-validated runtime IDs without another per-use resolver

Do not initially add mid-frame enable/disable or lazy asset publication. Feature settings take effect at an existing safe load boundary. Hiding a companion is distinct from releasing definitions still referenced by objects, saves, audio or rendering. If runtime toggling is already exposed, route it through that safe lifecycle or retain the installed resources until the next load

Three hardcoded consumers deserve explicit treatment during dependency preparation; walking the robot record alone is insufficient:

- `escort.c` plays `SOUND_BUDDY_MET_GOAL`, and `multi.c` also uses that constant. Identify which uses belong to the optional feature and map those through one owned semantic sound operation. Do not overwrite a D1 logical sound entry or globally rewrite unrelated D2 uses
- `ai.c` creates flares with `FLARE_ID`. Check the actual companion paths and the existing weapon owner to decide the intended source definition and mapping. Importing a companion does not authorize changing the player's native D1 flare
- `polyobj.c` queries the legacy spawnable-companion model to suppress external model substitution and logs its draw. Keep provenance/replacement eligibility in the asset owner. Retain a bounded query only if needed; remove the legacy model-index state and move feature diagnostics to an existing observation boundary when possible. Do not create a general model-provider framework for this case

Set the companion's world-facing behavior explicitly: preserve the native D1 flare definition for ordinary flare creation in a D1 session; keep the companion's own appearance and voice from its selected optional source. The AI/weapon owners implement that choice without changing the player's flare. Review morph appearance at the companion creation boundary separately from native matcen/boss morphing. Prune conservatively imported dependencies only after tracing these real consumers, not merely because a field in the companion record is unused

##### Contain necessary shared representation changes

The new sound namespace is a concrete case where an engine change may be justified. The working tree widens five runtime `robot_info` sound fields and adds an explicit writer so the HAM/HXM record remains 480 bytes. This is broader than an asset-file extraction and needs its own review gate before the optional feature relies on it

| Shared change | Allowed engine responsibility | D1/optional responsibility and required proof |
| --- | --- | --- |
| `robot.h`, `robot.c`, `bmread.c` runtime sound widths and disk codec | Hold a runtime logical reference; read/write the unchanged disk format with explicit bounds and byte signedness | Assets allocates extension IDs. Verify every reader/writer and raw-copy consumer, ordinary D2 round trips, unsupported extension export rejection and the affected editor build; do not serialize the widened structure by `sizeof` |
| `digiobj.c` forward/inverse translation | One delegation at the existing logical-to-sample boundary; retain normal D2 playback and linking | Optional owner interprets its own IDs/alternate map and lifetime. Verify normal/low-memory lookup, silence sentinels, linked sound recording, actual source rates and rejection after retirement |
| `polyobj.c` replacement eligibility | Ask whether the active model may use an external replacement, then draw using ordinary engine services | Assets owns provenance of every appended model, including alternate/dying/dead models. Test numeric slot collisions with D2 replacement models; remove legacy draw-only state when obsolete |

Do not spread extended-ID handling into each robot action, mixer backend, renderer or diagnostics formatter. Keep source interpretation at the owner and generic bounds at the consuming format/service boundary. If the proposed width change requires many such exceptions, revisit the representation before adding them. A neutral capacity/type fix can remain in an original file; package selection and source-specific arithmetic cannot

Evidence must distinguish availability, successful publication and a working actor. Exercise cold D1 -> spawn -> animate/render -> navigate -> use a door -> emit feature sounds/projectiles -> save/reload -> unload. Verify actual sample identity/rate and reachable references, not just a nonzero robot count. Include truncated definitions, invalid references, dependency cycles, capacity exhaustion and repeat preparation. Measure extension memory on Android before proposing paging

For base invariance, compare definitions and decoded pixels before world simulation, excluding extension slots. Then compare native enemy traces in a fixture where the companion cannot interact with them. In an interactive fixture, companion-induced collisions or awareness may legitimately change the world; do not mask those differences or demand identical full-world traces

#### E. Retire migration paths and close the lifecycle

Do this by named consumer rather than deleting all old-looking code at once. The current asset header still exposes transitional `apply_*` operations; some are exercised by old integration fixtures, while `d1_in_d2_apply_cockpit(0)` remains in asset teardown. A test-only caller is a reason to migrate the test to the real generation lifecycle, not to preserve a second production loading architecture indefinitely

1. **Private interfaces:** inventory callers of the asset/bitmap/AI internal headers and each exported helper. Keep asset generation structures available only to their facade/custom/asset collaborators. Keep briefing internals private under the presentation interface. Treat `piggy.c`'s bitmap-owner dependency as a specific registration/retirement question, not permission for renewed D1 parsing there
2. **Overlay retirement:** move validation scenarios that still invoke `apply_effects` or `apply_robot_assets` onto preparation/publication. Resolve the cockpit teardown caller, backup data and Guide-Bot dependencies before deleting obsolete overlay operations. Preserve the requested destroyed-light palette converter in the bitmap/retained-asset owner with a real test; it must not re-enable ordinary D1 light destruction
3. **Serialized namespaces:** document which IDs each supported live load, native checkpoint, D2 save, demo and rewind record contains. Route interpretation through levels/assets rather than guessing from index values. Verify actual swapped reads and rewind execution, not only unchanged structure sizes
4. **Remaining trigger/travel consumers:** close legacy single-type records, co-op/network interception, companion queries and ordinary D2 secret travel. A network path may use shared transport but must invoke the same D1 semantic operation. Enumerate unsupported modes instead of marking them covered by single-player tests
5. **Lifetime failures:** exercise selection/cancel, rejected preparation, failure after publication, interrupted presentation, custom -> stock, D1 -> D2 -> D1 and shutdown. Quiesce model, bitmap and audio users before retirement. D1-only error/menu/shutdown paths must never attempt a D2 baseline reload
6. **Edition/platform scope:** record supported registered/shareware/OEM/Mac variants and evidence separately. Build both executables on available host/platform configurations; run the isolated Android runner against the final APK and preserve restoration evidence. Keep native D1 as the reference until the separately approved engine-retirement change

Keep this work in the existing source family. A private implementation split is appropriate when a cohesive algorithm or resource lifetime warrants it; a public file per original D2 caller is not. Do not create a compatibility callback registry, duplicate world/object storage, or a second general engine layer

##### E implementation order and review artifacts

| Slice | Named source work | Completion artifact |
| --- | --- | --- |
| E1. Remove the second loading architecture | Trace every `d1_in_d2_apply_*`, old validation entry point and backup array. Move legacy test scenarios to `read_assets`/custom staging/publication, then remove the obsolete operations and state. Resolve `apply_cockpit(0)` in asset reset and the explicit reset declaration in `android_mission_asset_reset.c` through the existing lifecycle | A before/after consumer list, with no test-only reason left to retain a production overlay loader. A retained converter has a direct test and no authority to select baseline artwork |
| E2. Finish the registry boundary | Review `piggy.c::free_bitmap_replacements`, its call into the private bitmap owner and `piggy_read_level_bitmap_flags`' mode/palette inference. Decide whether the call is necessary registration retirement or obsolete overlay cleanup; keep D1 selection in the owner and shared PIG/POG mechanics in the engine | No original engine file depends on the prepared generation layout or privately owns D1 bytes. Do not generalize paging or add callbacks unless a surviving concrete consumer needs them |
| E3. Specify serialized identities | Inventory native D1 checkpoints, existing imported-game saves, D2 saves, demos and rewind state. For each, record format/version discriminator, source/runtime namespace, extension identity, decoder owner and failure behavior | A compact format matrix in the ledger, actual round trips and restore execution, and explicit handling of incompatible records. Never infer an ID namespace from whether a number fits a table |
| E4. Close trigger consumers | Review `switch.c`'s remaining single-type exit branches and the co-op interceptor that currently precedes native activation. Review `escort.c::side_has_exit_trigger`, which currently tests only `TT_EXIT`, along with route consumers | All source trigger interpretation belongs to levels. Companion routing and network transport use semantic queries/results, including compound native flags. Local and remote activation execute the intended side effects once |
| E5. Close transitions and source interfaces | Review selection/cancel, prepublication rejection, postpublication load failure, presentation interruption, custom -> stock, profile changes and shutdown. Trace caches, object users, model data, sample arenas, mounts and bitmap ownership across each | One lifecycle trace per failure class; every public helper has a production caller or a documented diagnostic role; no redundant cleanup or mode cache remains |
| E6. Publish the supported scope | Run available host builds for both games, D1-only Android scenarios and ordinary D2 regression coverage. Distinguish registered PC evidence from shareware/OEM/Mac and available platform/network coverage | A support/evidence matrix with unverified rows visible. Single-engine retirement remains a separate decision, dependent on the required fidelity and format coverage |

E3 starts while implementing D: an extension referenced by a saved actor must be prepared with the same deterministic identity before restore. Specify separately what happens to an ordinary new D1 game and to a save that requires missing optional content. The former must remain playable; the latter needs an explicit supported restore outcome, never an out-of-range ID or silent substitution of a native enemy

For E4, retain transport and scheduling in their existing services. Put native trigger meaning and travel policy behind the level interface, then have both local and remote paths invoke that operation. A generic notification is acceptable; a second D1 trigger interpreter in the route planner or co-op layer is not. Exercise ordinary D2 secret travel as well as imported D1 travel so removing legacy branches cannot silently change D2 behavior

Before deleting any obsolete path, identify its remaining serialized consumers. A legacy ID translator can remain in `d1_save_translate` or another existing format adapter even when the live overlay loader is gone. Its documented input format is the boundary; it must not reactivate the former D2-first initialization architecture

##### Original-file hook review checklist

Keep one ledger row per stable operation boundary: caller/function, owner, inputs/result, shared prelude/postlude, mutations/RNG, lifetime, and permanent/transitional status. Group callers of the same operation rather than inventing an abstraction per source file

Accept a hook that passes existing context, invokes an owned operation and handles its result. Reject a hook that still chooses D1 asset numbers, repeats source-format interpretation, orders several D1-only algorithm steps, or manipulates an owner's private state. A correction to one D1 rule should normally change one owner and its tests. A newly required neutral engine service is an explainable exception

Do not consolidate by moving hundreds of unrelated conditions into `d1_in_d2.c`, adding a universal policy object, or mirroring every original engine file with a D1 counterpart. Keep the facade for session transitions; keep data, AI, weapons, levels, presentation and small semantics as the existing cohesive domains. Public headers describe real entry points; private helpers describe implementation

Delivery order is D source/dependency preparation -> D atomic publication and behavior integration -> E1/E2 removal -> E3/E4 persistence and travel completion -> E5/E6 final acceptance. Dependency and serialization checks run during D, not after shipping it. Each slice includes its obsolete-body removal and focused verification; avoid a final unbounded cleanup patch

#### Delivery and review protocol for B-E

With B's session and C's named rule inventory implemented, deliver D as one connected optional-resource lifecycle, then E's remaining closure. Run relevant E checks alongside each slice. Preserve the tested owner boundaries; do not broaden a resource correction into unrelated gameplay changes

Each review unit must provide, in the existing ledger:

- The original function/body removed and its D1 destination, with the retained hook's exact purpose
- Inputs, outputs, mutation/RNG/cache effects, resource ownership and failure outcome; inactive dispatch must have no effects
- A public-interface review: each exported function needs a real integration caller, and migration-only helpers become private or disappear
- Native-D1 behavior evidence, ordinary-D2 regression evidence and applicable D1-only/resource-transition evidence, with limitations stated separately
- A list of remaining temporary bodies with their destination card; no unexplained "cleanup later"

Use representative change-locality exercises during final review: lava identity should be repairable in assets/bitmaps, briefing command/layout in presentation, native robot logic in AI, homing in weapons and secret return in levels. A change to one of those rules should not normally require coordinated policy edits across original D2 files. An additional engine hook is justified only by a missing lifecycle phase or a genuinely neutral service

Completion requires both behavioral evidence and the removal of the distributed implementation claimed by that slice. Passing a rendered screenshot does not establish independence from D2 assets; a successful D1-only boot does not establish original gameplay; an empty grep result does not establish tasteful ownership

### Completed AI consolidation contracts and remaining evidence

Implementation update: B4/D frame closure and C1-C4 world delivery, event/hit production, completion and native checkpoint submode preservation are implemented. `ai.c::do_ai_frame` has one native dispatch, and fourteen former phase operations are private to the AI owner. `set_player_awareness_all` dispatches one owned propagation/delivery operation, with explicit native and companion depth. World completion, event production and collision hit response have their own complete-operation dispatches; no actor-role or mission-gameplay queries remain in `ai.c` or `ai2.c`. Source comparison allows event production to reuse a common instrumented engine operation with admission supplied by the owner. Full-frame, world-update and actual native-checkpoint comparison evidence and the retained public caller map are in the ledger. Wider network/companion coverage remains open. The sequence below records the ownership contracts; do not repeat completed extractions

| Change | Concrete source work | Resulting boundary / closure evidence |
| --- | --- | --- |
| A. Native boss operation | Native preparation, spawning, cloak/hit/teleport, super-boss and death operations are now owned and have common native fixture evidence in the ledger. Complete broader on-level, checkpoint and network coverage alongside remaining AI work | Boss updates become part of the owned D1 frame; the temporary phase dispatch disappears in D. Preserve native timelines, RNG and spawned definitions, including death and restored clocks |
| B. Complete native behavior and path decisions | Move chase/run-from/still/hide/follow decisions and late firing/state/gun transitions out of `ai.c`; inspect the mode-sensitive path routines in `aipath.c` together with their callers | Native behavior codes and their execution order are private to AI. Keep path buffers and garbage collection shared. Verify all six native behaviors, blocked routes, recovery and doors, with an optional companion present |
| C. World awareness delivery | Separate the single world event-queue update from each actor's reaction. Replace the mission-wide propagation-depth choice only after defining delivery to native enemies and the companion | One queue consumption, explicit role policy and bounded private scratch state; no persistent duplicate awareness system. Verify depth boundaries and unchanged event/RNG ordering |
| D. Close frame ownership and shrink the header | Assemble the native-enemy frame in `d1_in_d2_ai.c`; remove D1 phase orchestration from `ai.c`. Preserve route/replica guards and account for every completion/early-return effect | One frame dispatch after required shared guards. Preparation, scheduling and frame-only helpers become private. Inspect the original-file diff and run full-frame native comparisons; passing individual phase fixtures is insufficient |

A and B can be reviewed in smaller changes where dependencies require it, but D is part of this consolidation, not a deferred cleanup project. Do not add a second set of phase APIs to make D easier. Use a private frame-local structure only if the connected implementation benefits from it; do not pass that structure back to the D2 frame to continue interpreting D1 behavior

For A, the source incompatibility is concrete: D2 boss helpers calculate indices relative to `BOSS_D2` and use D2 capability tables. Passing native flags 1 and 2 through those helpers is not a fix. The ledger now records owned native preparation and updates, including removal of the skipped frame path, pending-hit persistence and prevention of a second D2 death update. Broader on-level, checkpoint and network evidence remains open. Reuse neutral allocation/geometry/audio services; keep native timer thresholds, random selection and boss rules inside AI. Verify blocked spawning and failed placement as well as successful teleport/spawn. Boss initialization, save restoration and shutdown must use the same clock/state ownership; do not introduce shadow copies of existing boss globals

The former public AI surface had these retirement decisions, now implemented by frame closure. The primary burst-clock operation remains public for actual melee collision callers; perception, relative movement and the other firing phases are private:

| Current interface | Final disposition |
| --- | --- |
| `d1_in_d2_ai_prepare_frame`, `d1_in_d2_ai_prepare_behavior`, `d1_in_d2_ai_schedule_frame` | Private implementation functions, or folded into the owned frame; remove original-frame dispatches and public declarations together |
| `d1_in_d2_ai_update_boss`, `d1_in_d2_ai_update_brain`, `d1_in_d2_ai_run_from`, `d1_in_d2_ai_hide` | Private native-frame operations. The shared hide prelude must move with that frame. Do not retain these dispatches in `ai.c` after closure; initialization, collision, network and persistence boss operations have separate callers |
| `d1_in_d2_ai_visibility_turns_robot`, `d1_in_d2_ai_may_turn_randomly_without_visibility` in semantics | Absorb into the owned AI state transitions; remove these AI-only semantics declarations and repeated caller decisions |
| `d1_in_d2_use_d1_robot_aiming` | Remove after the remaining callers select complete actor-sensitive operations; do not replace every call with another role predicate |
| Perception, relative movement, burst timing and firing operations currently reached through `ai2.c` | Trace actual callers after frame extraction. Retain a public dispatch only where a separate engine operation still needs it; otherwise make it private and remove the migration hook. Existing tests are not, by themselves, a reason to expose an implementation detail |
| Initialization and actor classification | Keep the initialization boundary. Expose actor classification only for a real integration decision that cannot be contained in an owned operation |

The connected AI extraction is implemented. Texture/reference and custom staging, native triggers, single-player progression and the registered briefing session now also have implementation evidence as described in the current delivery plan above. These boundaries remove different kinds of distributed ownership. Preserve completed weapon, bitmap, wall and cockpit ownership rather than re-extracting them under new names

### Implemented AI path and frame boundary contracts

Implementation update: B1-B4 construction, requests, following and connected native behavior now belong to the AI owner. The complete native frame includes chase/still/follow and late state/fire/gun transitions; its former phase helpers are private. No D1 mode query remains in `aipath.c`, and the original `do_ai_frame` contains only its native dispatch. World awareness and checkpoint submode preservation have separate evidence below; broader restored/network/companion cases remain open. The table below defines the completed removal targets, not an instruction to reimplement them

The pre-extraction path implementation demonstrates why data import and predicate extraction alone are insufficient. `aipath.c` chose D1 destination, randomization, point processing and follow behavior. `ai2.c::ai_door_is_openable` also gave ordinary robots D2 permissions without a native-enemy dispatch. Source comparison confirmed that native D1 allows brains and run-from robots to open unlocked keyless doors, while the D2 routine additionally recognizes sniper behavior and player-held keys. Native D1 follow behavior shares a numeric value with D2 sniper behavior. Classifying the actor and interpreting the source behavior must happen together in the owner

Implement B in the following bounded changes. B4 and D form one connected frame-closure change; complete C's separate world-awareness operation before closing unit 6. This avoids another round of public behavior-phase helpers while keeping the world event queue outside the per-object frame. These are subdivisions of unit 6, not a second work queue

| Change | Work in the D1 AI owner | Original-file edits and acceptance |
| --- | --- | --- |
| B1. Door eligibility and native path construction | Own the native eligibility operation and breadth-first path construction, including run-from avoidance, side-order randomization, depth/partial-route selection, reconstruction and safety points | Dispatch at `ai2.c::ai_door_is_openable` and public `aipath.c::create_path_points`. Remove the D1 branches from the old builder and center insertion in the same change. Compare native paths, coordinates, segment identities and SIM draw counts |
| B2. Path requests and committed robot state | Own complete `create_path_to_player`, `create_path_to_station` and `create_n_segment_path` operations: destination, request parameters, path publication, direction, source hide submode, awareness and retry/reset ordering | Remove destination ternaries and D1 polish/outside-point exclusions from those engine functions. Verify normal completion, absent destination, exhausted storage and failed/partial requests. A successfully built point list alone does not prove correct robot state |
| B3. Following, replanning and brain door choice | Own the native `ai_follow_path` decision tree and the brain's source-specific door search, using the same path operations as recovery and behavior execution | Remove D1 follow/sniper reinterpretation from `aipath.c`. Inspect `openable_doors_in_segment` with its actual brain caller; D2's hidden-door exclusion must not silently become a D1 rule. Test path end, reversal, obstruction, run-from avoidance and hide arrival |
| B4. Connected robot behavior | Move chase, run-from, still, hide and follow execution, together with late state/fire/gun transitions, into the owned frame implementation | Remove the remaining D1 decision bodies from `ai.c`, including AI-only semantics predicates. Preserve behavior codes as source meanings rather than renaming them to D2 constants. Complete C's world-awareness boundary and D's frame/header closure before declaring AI consolidated |

B1 and B2 may be one reviewable change if sharing their state and failure contract makes a split artificial. B3 and B4 should likewise move together if keeping them separate would require exporting a new collection of frame locals. Do not turn each row into another permanent public phase API

Path-specific contracts:

- **Actor selection:** native enemies use native operations; optional companions and null-as-companion queries retain the engine's companion policy. Handle the console player's ordinary path query explicitly. A broad mission predicate at the shared builder would incorrectly affect companion routes
- **Specialized routes:** `create_guidebot_route_path_points` and `create_path_to_segment_internal` use `create_path_points_avoiding` directly. Keep that strategic route implementation available to the companion. Do not route every caller through a D1 branch merely because it shares point storage
- **Native algorithm:** preserve the initial six-draw side shuffle without D2's per-node refresh draws, source depth/exhaustion behavior, and native safety-point coordinates and segment assignment. Avoid inheriting D2 point correction or collinear-point reduction behind a supposedly neutral helper
- **State publication:** player pursuit uses the console player's segment; station pursuit uses the robot's hide segment. Native player-path creation sets the hide submode, station creation leaves it alone, and random-path creation clears it. That byte is used as D2 sub-flags elsewhere, so only the native owner may apply its D1 meaning
- **Storage and failure:** keep `Point_segs`, its free pointer, garbage collection and reset services shared. Specify required output capacity, the partial-path result and reset/early-return effects before extraction. Preserve valid-data behavior while retaining bounds checks; do not copy unsafe native indexing or introduce another persistent path pool
- **Results:** distinguish not-applicable from a handled path failure. Preserve the existing engine-facing success/failure contract. An unsuccessful native request must not fall through to D2 and create a different route
- **Diagnostics:** retain existing request, point and RNG trace observations at the same logical boundaries. Instrumentation belongs beside the operation it describes and must not consume random numbers or decide behavior

Use common native-D1/D1-in-D2 fixtures for unlocked, locked and keyed doors, with and without player keys; ordinary, brain, run-from and follow robots; straight and branching corridors; unreachable/depth-limited destinations; safety points; hide submode; and storage reset. Exercise the companion in the same D1 world and ordinary D2 before and after a D1 session. Test full-frame behavior after the path fixtures, because correct helper results do not establish correct caller ordering

The final public AI surface should be explainable by external callers: initialization, one native-enemy frame, the world-awareness boundary, and genuinely independently invoked collision, path, network or persistence operations. Boss preparation, network gating and hit/save adapters already have such callers. Frame-only boss updates, scheduling, preparation and internal behavior helpers do not need to remain public. Trace all production callers before privatizing; do not preserve an API solely because a fixture currently calls it

### Frame closure: one implementation change

Implemented boundary: B4's remaining behavior and D's final dispatch moved together, removing the temporary phase calls without adding public chase/still/state helpers. The native `d1/main/ai.c::do_ai_frame` decisions are adapted into `d1_in_d2_ai.c`, using the owned boss, navigation, perception and firing operations. Native D1 remains the comparison implementation. The contract and review requirements below describe this boundary; world-awareness and checkpoint work are still pending

The current source supports the following concrete ownership contract:

| Order / boundary | Final home and obligations |
| --- | --- |
| Route and replica guards | Keep `route_confirmation_drive_companion` and multiplayer companion ownership guards at the existing engine entry. They must precede either frame implementation. D2 companion retry policy remains in the D2 branch |
| Native actor dispatch | One `d1_in_d2_ai_run_frame(obj)` selects active native enemies. An engine actor returns not-handled without writes, RNG, resource access or diagnostic events; a native actor is handled even when its frame exits early |
| Entry, clocks and belief | Owner handles skip/observer/debug gates, native primary-fire and processed clocks, source behavior validation, player belief, previous visibility and gun/visibility origins. Capture previous visibility before a perception call can change it |
| Recovery and reaction | Owner orders agitation, retries, factory exit, awareness decay and dead-player navigation through the existing private native operations. Preserve their early returns and position publication |
| Animation and bosses | Owner selects when to invoke shared joint-animation services, then runs its native boss update. Preserve the existing Redux animation-disable setting as an explicit engine preference. Native timing, visibility cache and boss decisions remain owned |
| Scheduling | Owner performs native time slicing, process diagnostics and the skewed processed-clock reset. Skipped frames must not emit a processed event or execute later behavior |
| Brain and movement mode | Owner executes native brain, chase, run-from, follow, hide, still and door-opening decisions. Use source behavior meanings; D2 sniper, behind-player, thief and companion phases stay in the ordinary D2 frame |
| Late state and firing | Owner handles visibility-driven awareness, animation-state completion, state transitions, native primary-fire readiness, turning, shot eligibility and gun rollover. Do not interpret the native hide-submode byte as D2 camera/super-mine flags |
| Completion | There is currently no common postlude after gun rollover in `do_ai_frame`. Account for each earlier network/behavior return inside the owner; do not invent an unconditional cleanup block that advances guns or sends positions on previously skipped paths |

Keep visibility origin, direction, computed flag and cached visibility together for the native frame's lifetime. Keep fixed-point expressions and SIM/FX draw order intact. A private frame-local structure is optional; no frame-local structure or collection of output parameters crosses the public dispatch

Two possible shared services already have source evidence: all 196 state-transition table entries match native D1, and the multiplayer fallback-firing eligibility bodies match apart from compilation guards. Prefer a small pure transition lookup in the existing AI service interface over exposing the table or duplicating it. Retain the eligibility helper only if its actual contract is neutral after checking callers. Neither service should inspect the active game or own the native frame's decisions. Animation, turning, geometry and network publication likewise require a caller audit; being an existing helper is not proof of neutral policy

The same patch must retire the public preparation, scheduling, boss-update, brain, hide, run-from and random-turn phase declarations where only the frame calls them. Remove `d1_in_d2_use_d1_robot_aiming` and the two AI-only visibility/random-turn predicates in semantics once their caller decisions have moved. Keep independent initialization, collision, path, save and network boundaries. Update inactivity fixtures to exercise the public frame operation; do not keep migration APIs alive solely for tests

Review and verification for this patch:

1. Inspect the `ai.c::do_ai_frame` diff first: required entry guards, one native dispatch, ordinary D2 body. No native behavior branches, phase sequencing or D1-specific completion remain there
2. Compare actual native and imported frames for chase, still, follow, hide, fleeing and door opening, plus brain/boss cases. Cover visibility 0/1/2, primary-fire readiness, cloak, skip/time-slice returns and blocked movement. Compare object/local AI state, produced projectiles, paths and RNG, not just a helper's return value
3. Exercise a companion and ordinary D2 frames with the native dispatch inactive. Verify outputs/state/RNG are untouched by that dispatch and that shared route/replica work still executes once
4. Run the existing host suites, registered-data graphics integration and isolated D1-only Android interaction runner. These protect integration continuity; they do not replace the native frame comparisons or establish multiplayer fidelity
5. Record any remaining independently called AI operations and world-level D1 decisions in the ledger. C1-C4 below close the world-awareness and hide/follow checkpoint boundaries; unit 6 retains the broader stated persistence/network coverage gaps

World-awareness propagation and delivery now use a separate operation at `set_player_awareness_all`, with explicit native and companion depth, one `Awareness_events` consumption and private temporary maps. Event production is still an independent phase to audit. The global camera-retirement and boss-death passes remain the next completion boundary, so native submode and death state cannot be processed a second time under D2 rules

The checkpoint adapter currently clears the native submode byte. Preservation belongs in the format adapter using the AI domain's source-state contract, with an actual restored hide/follow scenario. Removing frame branches does not repair serialized-state loss; fresh-path tests must not be presented as evidence for restored behavior

### World AI and restored source state

These record the implemented C1-C4 boundaries and their coverage contracts within unit 6, using the existing AI owner and checkpoint adapter. Do not add another domain, another event queue or public helpers corresponding to each original loop. The per-object frame is already extracted and is not part of a second migration

**C1. Awareness propagation and delivery, implemented.** `d1_in_d2_ai_deliver_awareness` dispatches at `ai.c::set_player_awareness_all`, before the existing queue-consuming call. The D1 operation owns propagation and delivery in a D1 world; inactive D2 returns without consuming events or changing state. The mission-wide depth ternary is removed from `pae_aux`, restoring the ordinary D2 body. `New_awareness`, `pae_aux` and `process_awareness_events` remain in `ai.c` without new exports. The following paragraphs specify the implemented contract and its evidence requirements, not a new extraction task

The source contract is precise: native D1 starts at recursion level 1, updates that segment, and recurses while level is at most 4. This reaches four edges from the event; ordinary D2 reaches three. Type 4 is retained at the source and attenuated to 3 on the first edge. Delivery takes the maximum event strength, raises an actor's awareness only when stronger, and then sets the source awareness duration. It does not lower existing awareness or refresh an equal-strength timer

For a mixed D1 world, native enemies receive the native propagation result and the optional companion receives the D2 result. Build these results from the same queue before clearing it once. Any additional map is bounded private scratch with a documented lifetime, not a persistent awareness owner or a new field on every robot. Traversal retains native side order and valid-segment checks. Propagation and delivery consume no RNG; event creation remains a different phase

Use a common native/imported fixture with a branching, cyclic segment graph and actors at zero through five edges. Include several event strengths and overlapping events, existing stronger/equal awareness, an empty next update, and a companion at each depth boundary. Assert final timers, queue consumption, untouched source submode and RNG counts. Run ordinary D2 before and after the D1 case. The shared D2 delivery code currently tests whether awareness increased after assigning the larger value, making its camera-bit clear ineffective in normal cases; do not fold an unrelated correction of that ordering into extraction

**C2. Event creation and hit response, implemented.** Source comparison of `add_awareness_event`, `create_awareness_event` and cloak refresh establishes the same observer rejection, Vulcan ID/filter, queue capacity, agitation clamp and SIM arithmetic/order in both engines. D2 also maintains segment-cache fields needed by engine actors. These are retained in one common instrumented producer; its caller supplies admission, and it does not query the active game. The D1 owner admits production in all game modes before the common observer check; ordinary D2 keeps its multiplayer-with-robots gate. `create_awareness_event` dispatches a complete D1 operation or invokes the common producer with ordinary D2 admission. Native frame calls use the owner directly

This bounded sharing decision follows the source comparison and common native fixtures, rather than copying an instrumented algorithm solely to relocate one policy choice. There is one engine service and one domain entry, without an event context, callbacks, per-stage public helpers or a scalar D1 predicate returned to the original algorithm. Existing source correlation and before/after diagnostics execute once in the common producer. Fixtures cover actual queue payloads, RNG, cloak refresh and agitation in single-player and multiplayer with/without robots; this does not establish network transport compatibility

The hit-response part is implemented: `ai2.c::do_ai_robot_hit` has one actor-sensitive D1 operation and its mission-wide switch is removed. This entry has real collision callers, so it remains public. The native source compares the behavior byte against `AIM_HIDE`/`AIM_STILL` mode constants, rather than valid `AIB_*` behavior codes. For normal source behaviors this is a no-op. The owner preserves and documents this source quirk, while companions continue through their engine hit response. Common native/imported fixtures cover these cases and inactive state/RNG; see the ledger for hit and event-production evidence

**C3. World completion and source-byte ownership, implemented.** The tail of `ai.c::do_ai_frame_all` dispatches one completion phase after its existing diagnostics. The two actor filters are removed from the original body: the owner preserves native hide submode during camera retirement and prevents a second native boss-death update. Ordinary D2 completion remains the fallback. The owned mixed-world operation explicitly calls the existing engine boss-death operation only for engine actors; it is not treated as a neutral native-boss primitive. No public `should_clear_camera_bit` or `should_update_boss` predicates were introduced. The following cases define its evidence, recorded in the ledger

Check an expired and a live missile-camera reference with native hide/follow robots and a companion present. Assert native flags survive, companion camera state follows the established engine contract, and a native boss receives exactly one death update at its source phase. Account explicitly for worlds containing any additional engine actors; actor classification must agree with active definitions, not object index or a global mission shortcut

**C4. Restored native state.** In `d1_save_translate.c`, retain the serialized source `flags[4]` byte instead of clearing it as D2 `SUB_FLAGS`. The format adapter owns decoding and validation; AI owns its runtime meaning through the existing private source-state contract. Keep D2-only fields initialized separately. Audit subsequent object verification and AI initialization to ensure neither overwrites restored mode, submode, path cursor or clocks. Do not introduce a second save-only behavior interpreter

Implemented: the decoder preserves the source byte without a new AI API or changes to original D2 files. `test_d1_ai_checkpoints.ps1` writes actual native First Strike checkpoints, restores them through the native full loader and the imported adapter, and compares 28 frames across seven scenarios. It covers hidden/seeking robots, follow routes, both blocked endpoint reversals, a skipped frame and a second checkpoint during execution. Current native version 17 support was required by this evidence; its pending-selection record is decoded, validated and committed in the existing adapter. Truncated/invalid records leave the current objects and selection untouched. This does not establish complete checkpoint fidelity for every runtime subsystem, physics integration or network play; those remain part of the final persistence/campaign review

Verify by writing and reading an actual native checkpoint with hide and follow robots, then running restored frames. Compare the native and imported mode/submode, path points and cursor, movement, firing clocks and RNG. Include hiding, resuming a route, endpoint reversal and an interrupted save/load, together with ordinary D2 save behavior. A decoder assertion or fresh-path fixture alone does not establish this requirement

Close each change with its original-file diff, owned body, remaining public caller map and behavior evidence. C1-C4 can share an implementation change when source-byte ownership requires it, but they must not be presented as completed by the current per-object frame matrix. Level/reference and custom staging, native progression and briefing extraction now have separate ledger evidence; use the current delivery sequence for remaining work

### Keeping the final source layout maintainable

Judge each original-file change by the information it contains. A caller may supply existing objects, invoke an owned operation and handle its result. A caller should not need to know a D1 texture number, boss flag meaning, briefing coordinate, behavior encoding or sequence of D1 loaders. Those are practical review failures even if the code uses attractively named helpers

Use the existing domain headers as the integration surface. Asset-generation layouts, bitmap ownership and private frame/path/session state belong behind it. If a large owner needs collaborators, organize those by substantial responsibility and keep their internal header dependencies within the compatibility family. A public facade including every domain's data structures would recreate the coupling in a different file

For each extraction, review four artifacts together: the removed original body, its owned implementation, the remaining public declarations with their real callers, and the behavior evidence. Reject an extraction that adds a helper but leaves the original caller selecting each D1 step. Also reject a shared service that hides a D1/D2 rule switch: expose neutral geometry, drawing or allocation only, with game decisions in their owner

There is no target of zero hooks or an arbitrary maximum number of touched files. Some independent engine phases require independent calls. The target is change locality: fixing one D1 rule normally changes its owner and tests. End each domain with that review before moving on, so transitional interfaces cannot accumulate as the permanent architecture

### Detailed domain cards

**Session transition (unit 1).** The facade now keeps committed content separate from `Current_mission->descent_version` and the startup preference. Successful D1 publication or completed D2 definition loading commits the identity; registry retirement clears it. The runtime query uses requested content only before publication or after explicit unload. Selection-only D1 preparation remains deferred, with actual intro entry preparing the bank because briefings precede level decoding and can render models. Tests cover descriptor changes, failed selection, rejected publication and registered-data transitions; see the ledger for device evidence. Continue auditing runtime callers and explicit post-retirement failures. Rendering and simulation must never run against a requested profile with another profile's tables

The facade may call existing D2 loading services when committing a D2 destination. Neither assets nor those services may select a mission or call back into transition preparation. The retained engine hook supplies the request and handles a typed outcome if failure/defer/handled need distinction. Do not retain the current boolean "extra robot movie needed" return as an all-purpose success result

**Wall crossing (unit 3).** Inputs are the existing segment/side/face, wall flags, query flags, hit point and object identity for diagnostics. The D1 operation owns closed-door treatment, transparent-point sampling semantics and the crossing decision. It may invoke existing bitmap paging, merged-texture caching, RLE expansion and UV calculation; these cache effects are part of its contract. It does not change world geometry, doors, objects or RNG. Keep intersection search, hit ordering and segment traversal in `fvi_sub`. The result is a crossing decision, not a second collision engine. Verify the extraction without changing the current pixel-wrapping arithmetic in the same patch

**Robot phase (units 5-6).** Identify the actor's role before selecting D1 behavior. Write a short prelude/owned-phase/postlude list beside the first extraction: include timers, skip handling, route confirmation, multiplayer companion ownership, visibility cache, animation, firing and position publication. Move a coherent phase with the state it uses. A handled return must preserve required shared completion, while a not-applicable return must not consume RNG or mutate state. Expose existing geometry/object services where needed; do not pass a callback for every former local helper or export the whole function's locals as a context structure

Break that robot work into the following changes, keeping the same AI owner throughout:

| Change | Entry points to inspect | Owned implementation and removal target | Required evidence |
| --- | --- | --- | --- |
| 5a. Actor selection and initialization | `ai2.c`: `init_ai_object`, `ai_behavior_to_mode`; `ai.c`: `do_ai_frame` entry | Select policy using active content plus the actor's definition/role. Move D1 behavior decoding and initial mode selection into AI. Retire the mission-wide aiming alias at migrated callers; do not substitute a new public predicate at every old branch | Ordinary enemy and optional companion initialized in the same world; fresh load and restore choose the same policy. Existing D2 initialization is unchanged |
| 6a. Scheduling and perception | `do_ai_frame`, `player_is_visible_from_object`, `compute_vis_and_vec`, awareness propagation | Move D1 skip/timer/visibility decisions as a connected phase. AI owns whether object hits count as seeing the player, visibility caching and awareness depth. Shared FVI remains a service | Dormant, alerted, cloaked-player and obstructed cases; skipped frames; unchanged required completion and RNG order |
| 6b. Combat and movement | `ai_fire_laser_at_player`, `ai_do_actual_firing_stuff`, `set_next_fire_time`, `ai_move_relative_to_player`, matching `do_ai_frame` branches | Own firing eligibility, burst/aim selection and D1 movement decisions. Existing three AI helper bodies become private where no engine caller needs them. Move AI-only semantics helpers with their callers | Actual projectiles, gun positions, fire timers, chase/run-from/still modes; player cloak; optional companion present; network position/fire publication occurs once where applicable |
| 6c. Paths, doors and bosses | D1 branches in `aipath.c`, `ai_door_is_openable`, `do_boss_stuff`, boss initialization/death callers | Own destination/door eligibility, D1 path-following decisions and boss policy. Keep path storage, garbage collection and reusable geometry in engine code. Do not route a companion through D1 enemy decisions | Door/key cases, blocked/replanned routes, boss teleport/death; stable path storage and RNG use; companion route regression |
| 6d. Close the frame boundary | `do_ai_frame`, AI domain header, migrated helpers and remaining AI predicates in semantics | Assemble the connected native-enemy frame inside AI. Replace intermediate D1 phase orchestration in `ai.c` with the frame dispatch and required common work. Make frame-only helpers private; retain externally needed perception/firing/path operations | Full-frame native comparisons across all six D1 behaviors; skipped/processed/death paths; exactly-once shared effects; no D1 behavior switch or native frame ordering left in `ai.c`; ordinary D2 and companion regressions |

Do not assume these phases align perfectly with current file boundaries. If scheduling and a behavior branch share state that cannot cross a small interface, move the connected D1 frame operation together and keep its internal phases private. A short explicitly named result can distinguish not-applicable, processed and skipped when the caller needs that distinction. Never overload a boolean so that an early return accidentally skips common completion. Avoid copying the entire D2 frame routine into the owner and carrying unused D2 behavior along with it

For unit 6a, distinguish each robot's awareness reaction from world-level event propagation. The current propagation walk uses one `New_awareness` field and a mission-wide depth selection. Moving that ternary into a helper would neither establish ownership nor decide what the optional companion receives. Before extracting it, specify native-enemy and companion delivery semantics, consume the shared event queue once, and test the depth boundary with both actor roles. Any additional scratch results must be private to the update operation, not a second persistent awareness system

For unit 6d, use a compact phase-order comparison against native D1: entry/skip, clocks and target belief, recovery/matcen, awareness, animation/boss preparation, time slicing, behavior execution, firing/state transitions and final gun selection. Mark shared services and source-specific ordering explicitly. The current early-frame and connected navigation/awareness helpers have bounded evidence in the ledger; their presence does not close the frame boundary. Do not run a new migration from an older planning snapshot

**Weapon review (unit 4).** Current `laser.c` calls the weapon owner at creation, target acquisition, target retention, firing notification, homing steering, speed limiting and child selection. Preserve these phase boundaries: collapsing them into a single top-of-frame hook would change timing. Check `object.c` homing configuration and remaining consumers of homing constants as well as `laser.c`. The owner should contain the D1 arithmetic, source-record meanings, eligibility and diagnostics; allocation, collision execution and shared replay recording stay in the engine

Review inactive dispatch before calling it complete: it must leave D2 state and RNG untouched where it falls through. A handled no-target result is different from not-applicable; the existing target-operation sentinel distinguishes them. Validate player versus immediate parent versus propagated weapon owner, Spreadfire accounting versus projectile identity, rendered-view acquisition versus full scans, cloaking and rescan cadence. Record the evidence already available and any remaining coverage gaps without treating source presence as a new test result

**Level definitions and reference conversion (unit 7).** The existing `d1_in_d2_levels.*` uses the asset facade rather than a second loader. The texture and reactor extractions below are implemented; native trigger conversion is the remaining operation detailed in A1-A3. Keep the faithful source-reference operation separate from any legacy converted-slot adapter. Both accept the appropriate serialized namespace explicitly; neither guesses it from a coincidental numeric ID

Reference-boundary update: `d1_in_d2_levels.*` now owns the source texture-pair operation, explicit legacy D1-to-D2 slot mapping, and native reactor binding. The old mapping body and unused uniqueness helper have left `gamemine.c`; the public legacy declarations and PIG-presence global are removed. The mine decoder dispatches once after reading both source textures; the checkpoint adapter uses the same operation. Native IDs and overlay rotation bits are preserved and checked against the published count before either output changes. Legacy asset callers explicitly supply PIG availability and registered/shareware layout, and their adapter cannot switch to native IDs merely because native assets are active. This retained editor/overlay path is not the faithful runtime design

Reactor binding remains after common model-name remapping in `verify_object`, at its original effective phase. Moving it to the function entry was rejected by the real-level fixture because the common remap overwrote the native model. Comparison now covers all 1,710 side texture pairs in fresh First Strike and seven actual native checkpoint restores, plus 28 restored robot frames. Registered-data checks cover all 584 source texture IDs in four overlay orientations, bounds rejection, explicit legacy mappings, and D1/D2 profile transitions. The ledger records build/device evidence and narrower edition/editor coverage limits

The next unit-7 work is trigger interpretation and remaining custom-data lifecycle coverage. Inspect native trigger execution before sharing the current conversions: fresh loading currently prioritizes door flags, while checkpoint translation prioritizes exits, and native D1 can act on multiple flags. Moving either priority chain wholesale would preserve different behavior across restore. Keep native source flags available to the owned operation and separate D2's legacy v30 extensions; establish native action/enable/one-shot semantics with fresh and restored tests together with the progression boundary. Do not introduce a second trigger policy in the save adapter. Native secret execution cannot safely dispatch through the current D2 `EnterSecretLevel`: it performs D2 save side effects before choosing the D1 branch, and its death/return callers depend on those side effects. Move the connected native transition sequence rather than fixing only the trigger's call site

Custom staging update: `d1_custom.c` now prepares PG1, DTX and HX1 in their native precedence order inside one unpublished generation. Source names and explicit DPOG IDs resolve against that generation, independent of live D1/D2 tables. Custom images, samples, robot/joint/model definitions and diagnostics publish together before object loading. The old live overlay loader, bitmap/sound backups, custom sample resampler and separate removal calls are retired, including the Android reset call. No new custom-format branch was added to an original D2 file. Samples remain at their source rate until the common publisher handles device conversion

HX1 disk layouts still use existing engine readers; the custom owner strips robot fields native D1 ignores. Shared validation checks affected robots, animation users and model/bitmap references. A rejected custom stage is discarded without changing live resources or diagnostics. Integration fixtures cover initial object verification, three custom-to-stock cycles, 32 malformed HX1 files, 93 malformed PG1/DTX files, alternate supported image/sample layouts and both mixer/plain-SDL output conventions. Native D1 reads the same valid HX1/PG1/DTX fixtures

Custom checkpoint update: the host checkpoint runner now reselects the saved mission and calls the normal `StartNewGame` lifecycle before applying native checkpoint state, matching the production import sequence. Its `-CustomAssets` mode uses a separate mission/level plus native-generated PG1/DTX/HX1 files, verifies a stock-to-custom checkpoint transition, and compares the complete custom wall pixels, samples, selected definitions, saved object physics and 28 restored frames from seven native scenarios. The stock runner retains its seven-scenario comparison. This closes the custom asset reload/retention evidence gap without adding a save-specific asset loader or policy API. Broader saved-world, campaign, custom-content/edition and platform coverage remains open; this does not mark all of unit 7 complete

Preparation must include `.hx1` robot/model definitions before `verify_object` or AI initialization can use them. The asset/custom owner reads and validates definitions; the level owner interprets level references against the published result. Keep packed texture orientation bits intact, validate indices against the correct table, and use the same conversion on checkpoint restore. Remove late repairs only after identifying all their consumers, including diagnostics and saved-state adapters

Split review into reference conversion and custom staging, so an ID change cannot hide a lifecycle regression. Verify custom-to-stock cleanup, two D1 references formerly mapped to one D2 slot, malformed custom definitions and fresh/restore parity. Failure before publication must leave active data valid; failure after retirement must use the defined unloaded error path

**Progression (unit 8).** Native trigger activation and completion/death now dispatch to levels before D2-only transition side effects. The old `EnterSecretLevel` snapshot-before-D1-branch sequence and `AdvanceLevel` late native-return branch have been removed. Native completion owns destination and score/ending order, uses the mission's secret table for return, and calls the shared loading services. Keep reviewing the legacy single-type trigger fallback, direct ending/co-op callers and network travel interception; current single-player evidence does not close those paths

The level owner determines destination and D1 score/inventory/secret-return treatment. The existing transition boundary performs the specified load, flyout or ending through direct engine calls, with resource activation delegated to the facade. Return a small result containing only what that caller actually must execute; do not build a generic command list or callback-driven transition interpreter. If a sequence of actions is intrinsically D1, keep that sequence in the level owner and expose the narrow existing services it needs

| Scenario | Compare with native D1 | Additional boundary assertion |
| --- | --- | --- |
| Normal exit and reactor exit | Destination, scoring, inventory, flyout and persistent wreck | One transition and one resource activation |
| Secret entry | Destination, briefing, score/inventory and return bookkeeping | No accidental D2 secret-world snapshot path |
| Secret exit or death | Return destination, lives/inventory and destroyed-reactor consequences | Correct common completion even on early exit |
| Save/reload around secret travel | Same effective definitions and progression as uninterrupted play | Save adapter uses the same level policy |
| Last campaign level | Ending selection and menu return | Presentation cleanup precedes generation release |

**Briefing session (unit 9).** The private `d1_in_d2_briefing.c` now owns the D1 screen table and the connected initialization, command processing, screen progression, window handler and briefing/ending entry operations. The following requirements describe its retained contract and wider coverage obligations. Dispatch before creating a D2 briefing session. The D1 owner retains command position, screen selection, palette/resource choices, page progression and close-time cleanup for the entire session, rather than exporting one predicate per command to `titles.c`

Reuse existing window/input, text/bitmap/model drawing and audio services. A private D1 briefing-state structure is justified because the session owns real mutable state and a lifetime; a copy of every global in `titles.c` is not. Expose a reusable renderer operation only when both callers need it. Keep D2 command interpretation in the existing D2 implementation. If the extracted briefing body makes presentation unwieldy, a private `d1_in_d2_briefing.*` collaborator is reasonable; do not create it for a handful of helpers

Verify first/later/secret briefings, custom text/art, skip, close during animation, reopen and endings. Close must release screen/model/audio references before session resource retirement. With both games installed and optional high-resolution D1 art missing, selection must still preserve the original D1 appearance. D1 low-resolution art scaled by the renderer is a valid baseline

**Optional resources and retirement (units 11-12).** Treat Guide-Bot import as a bounded dependency graph: robot definition, model/joints, model textures/bitmaps, sounds and any referenced effects/weapons actually required. Assets owns deterministic extension-slot allocation and reference translation. No engine caller assumes the original D2 slot is populated. AI selects the companion role; ordinary D1 enemies keep their own policy. Cameras use engine views with cockpit-owned layout and do not require a D2 cockpit

Test D1 alone and D1 with D2 mounted using identical base-game settings; compare base resource provenance and ordinary gameplay. Missing optional content affects feature availability only. Before removing any overlay helper, name its final live-load, custom-content, save/replay and diagnostic consumers and migrate or explicitly retain each one. Preserve the accepted destroyed-light palette conversion as an owned conversion capability even though ordinary D1 lights cannot select that destruction path

### Containing shared services and future changes

Use a service extraction only when an owned operation genuinely needs access to engine machinery. Prefer an existing callable primitive, then a narrowly exposed former static function. Put a declaration in its natural engine header; do not grow a `d1_engine_services` callback table. The service's implementation must make sense without D1 resource IDs, behavior codes or game-selection checks

Examples: shared geometry may calculate a wall UV or test visibility; D1 decides how that result affects transparent-wall passage or robot perception. Shared object code may allocate/relink a projectile; D1 decides its initial position and weapon semantics. Shared rendering may draw a model; D1 owns briefing placement and palette selection. Shared fixes that also affect native D1 should be evaluated in both engines and kept distinct from compatibility policy

Extend the existing ledger during each domain change with this compact entry, rather than adding a second tracking system:

```text
Caller/function:
D1 owner and operation:
Original D1 body removed:
Shared prelude/services/postlude retained:
Inputs; mutations; RNG/cache effects; lifetime/failure contract:
Extraction evidence / native-D1 fidelity evidence / D2 regression evidence:
Temporary remainder, destination unit and reason:
```

At review, inspect the original-file diff first. Search explicit D1 predicates, format/version branches and hardcoded D1 IDs, then classify what remains; a grep count cannot prove ownership. Adding a hook is acceptable when it introduces a necessary phase boundary. Adding another D1 decision beside an existing hook is a reason to revisit the operation. A correction to homing, briefing layout or secret routing should normally change one D1 domain and its tests

### Evidence status and the next review

The ledger records independent registered-D1 tables, rendered original cockpit/cameras, host profile restoration, and complete isolated Android interaction runs through First Strike's exit to level 2. Wall crossing now has shared native-D1/D2 integration coverage and its owned implementation. These are useful foundations. They do not establish completed campaign fidelity, every supported D1 edition, optional companion independence, or removal of the remaining distributed algorithms

Continue unit 1's lifetime/failure coverage and unit 2's resource-open evidence. Units 3 and 4's wall and named base weapon operations are verified in the ledger. AI now owns the complete native robot frame, with private perception/firing/movement phases and separate path storage integration. The common native/imported full-frame runner verifies 1,260 scenarios and 5,040 frame snapshots. Separate world-update and actual checkpoint fixtures cover C1-C4; broader restored/network/companion evidence remains open. Level references, custom staging, native trigger operations and connected single-player progression now have ledger evidence. The briefing owner now has exact native/imported render and lifecycle evidence. C's named gameplay inventory now has owned operations and comparison evidence. Continue with D's optional generation and E's closure, retaining the explicit legacy/network trigger, progression and wider presentation coverage requirements. For every review, show:

1. **Original-file delta:** the exact D1 bodies removed and the hooks/services retained
2. **Ownership contract:** inputs, state/cache/RNG effects, lifetime and failure handling
3. **Behavior evidence:** extraction preservation, intended native-D1 behavior and ordinary D2 regression, clearly distinguished
4. **Remaining debt:** each temporary hook has a destination and a later unit; no unexplained "cleanup later" entries

Do not start by mass-renaming predicates or moving every D1 branch into one enormous file. The goal is that a correction to a domain is understandable in that domain's owner and its tests

### Definition of a tasteful boundary

An original-file hook may pass existing objects/context, select one operation and handle its outcome. It should not contain D1 asset indices, coordinates, behavior-code interpretation, arithmetic constants or the ordered list of D1 loaders. A small isolated scalar policy can remain a helper; a cluster of dependent predicates with shared state should become one owned phase

Use a concrete review question: if D1 homing, briefing layout or door timing is wrong again, can the correction normally be made in its D1 owner and tests? If several original D2 files must change for that same rule, either the boundary is unfinished or the owner is wrong. This is more useful than a numeric cap on hooks or a blanket ban on mode checks

Keep architectural and behavioral acceptance separate. A cleanly extracted operation may still have a native-D1 fidelity gap; a passing fidelity scenario may still leave distributed implementation. Each completed slice needs evidence for both its stated behavior and the removal target it claims

### Final consolidation review

Close each domain with these concrete artifacts in the existing ledger, not another parallel plan:

1. A caller map of the remaining original-file hooks, classified as lifecycle dispatch, operation dispatch, format adapter or neutral engine service. Transitional D1 bodies prevent architectural closure of that domain
2. A public-header review identifying actual callers. Remove helper declarations that became private after extraction, obsolete mode aliases and duplicate data ownership
3. A source search covering `EMULATING_D1`, compatibility predicates, serialized version branches and hardcoded D1 IDs. Inspect the matching function bodies; do not mechanically eliminate legitimate format detection or unchanged D2 logic
4. Evidence that the domain works with D1-only assets and behaves identically when unused D2 assets are also available. Optional Guide-Bot assets must not become implicit fallbacks for base D1 art or rules
5. Native-D1 behavior evidence and ordinary-D2 regression evidence appropriate to the change. Test the optional companion separately wherever actor policy is involved

Finish with three change-locality checks: correcting D1 homing should land in weapons; correcting briefing placement should land in presentation/cockpit; correcting secret routing should land in levels. A caller edit is justified only if the correction exposes a genuinely missing engine service or lifecycle boundary. If an ordinary D1 correction still needs coordinated policy edits across several original D2 files, the domain is not consolidated

## Planning validation

The latest 2026-09-21 revision moves the compatibility file set into its own directory and refreshes the finishing sequence in section 0. It inspects the production facade, optional-resource integration, checkpoint translation and actual replay scripts. Earlier D/E operation cards remain the ownership/removal contracts; strict baseline replay evidence now precedes optional-feature completion

Concrete open boundaries: the production facade does not attach an optional source; the native availability query coexists with the old append/capture fallback; companion goal/flare/morph consumers still use fixed engine constants; native texture validation currently uses the combined runtime count. These observations guide D/E review and are not claims that the newly staged extension has been exercised through actual gameplay. Runtime sound-field widening is called out separately because its generic codec/export obligations extend beyond a private asset change

The relocation preserves all 30 moved source/header files byte-for-byte and changes only their external include/build/test paths. Current host, source-test, Android-build and corpus-run evidence is recorded separately in the ledger. Earlier counts elsewhere in this document remain historical evidence. No gameplay correction or strict-runner implementation is claimed by this relocation/planning change
