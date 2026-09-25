# D1-in-D2 continuation: September 24 research refresh

Date: 2026-09-24

Status: implementation continues; baseline implementation is substantial, full fidelity remains unqualified

## Current stopping point and next work

Latest endlevel work: world schema 8 now observes the complete two-actor
flythrough records, phase timers, explosion waits/sound counter, camera and
exit geometry, phase angles and the active external explosion's object union.
Former function statics retain their process lifetime; this adds no resets or
save-format changes. Three actual rendered-sequence runs (levels 1, 2, 1) match
across 951 native/imported frames, including player/viewer motion and SIM/FX
draw counts. All four phases and the outside explosion are exercised. The
fixture drives the real endlevel frame function and actual campaign completion;
it does not claim GPU pixel or audible-output verification

The repeated run demonstrates real carryover: with the same FX seed, the first
level-1 flyout draws 41 times and the repeated level-1 flyout draws 37 times.
The second run exercises nonzero bank rate and sound counts carry into the third.
Do not blanket-reset this history. Next, add an actual post-flyout save/restore
probe, then determine the persistence contract for these waits/counters alongside
the remaining private-state audit. The current save writers do not serialize
them. Desktop endlevel input excludes normal save commands, but Android rewind
captures before `GameProcessFrame`, and its shared memory-save path has no
explicit endlevel guard. Inspect/drive that path before declaring active-flyout
rewind supported or excluded. Boss/network, death-phase and bump-state audits
remain open

Both host builds and full suites (53/53 D1, 61/61 D2), 53 comparator tests,
scoped quality and all Android ABI builds pass. Evidence is in
`temp/d1-endlevel-comparison` and `temp/d1-endlevel-observation-*.log`.
Schema-7 captures retain their frozen checker; no full-corpus or device-runtime
qualification is added by the schema-8 observer

Current source, host binaries, APK and completed endlevel evidence are pinned
in `temp/d1-endlevel-observation-source/manifest.json`. Earlier source capsules
and checkers are unchanged

Latest transition work: the campaign fixture now compares 21 native/imported
observations, including normal travel, all three secret levels, ordinary restore,
intact/destroyed-mine death, secret return and campaign completion. Seventeen
first-use phases exercise actual Fusion firing, refueling and the collision gate,
including exact awareness and SIM/FX draw counts. The three cadence clocks retain
their native absolute storage across level preparation; backwards-origin guards
restart them on use. Ordinary restore rebases their saved deltas. All phases match

The new ordinary D2 control exposed a separate level-lifetime bug: a pending
Fusion awareness event from D1 segment 355 survived into Counterstrike level 1,
whose highest segment is 227. The actual save then failed AI preflight stage 10.
Both engines now clear the pending count in `init_ai_objects`; ordinary restore
still reinstates its saved queue. The before-fix save and targeted observation are
in `temp/d1-awareness-transition-before`. All three actual ordinary D2 saves now
restore full-width cadence deltas, including values beyond 32 bits. Campaign
evidence is `temp/d1-campaign-comparison`; the runner accepts `-D2DataDirectory`
to include this control

Both host builds, full suites (53/53 D1 and 61/61 D2), seven actual checkpoint
scenarios with 28 resumed frames, scoped quality and all Android ABI builds pass.
Current sources, binaries, APK and evidence are frozen separately in
`temp/d1-cadence-lifetime-source/manifest.json`; the earlier persistence capsule
is unchanged. This milestone adds no fresh device-runtime or full-corpus claim

Continue with the private endlevel and boss/network state audit, not another
cadence format change. Audit complete death-phase state and multiplayer bump
timing alongside those caches. Then stabilize the observation contract and run
all eight recordings with two native captures and one imported capture each.
Live network, rewind-history selection, optional-feature lifecycle, custom art,
full D1/D2/D1 presentation switching and platform qualification remain open

Current persistence work: actual native checkpoint evidence reproduced all three
clocks retaining deliberately contaminated process values (123/456/789), while
all other compared fields matched. Evidence is retained in
`temp/d1-cadence-persistence-before`. The shared `cadence_runtime.h` now owns a
24-byte, full-width epoch-relative record, appended in native D1 version 18
and D2 version 37. Version 36's native AI object contract stays unchanged.
The staged native translator reads the same record before committing state;
older formats clear missing clocks instead of inheriting another session.
Present replay collision metadata still overrides the save. With absent metadata,
the comparator now reports that independent clock check incomplete rather than
assuming the opaque save contains zero

Seven real checkpoint scenarios pass native restore, import, ordinary re-save,
legacy/current switching and 28 resumed frames, including wide past/future clock
values at a nonzero save epoch. Both host builds and all 51 comparator tests pass.
Both full host suites pass (53/53 D1 and 61/61 D2). The codec fixture covers
both byte orders, positive/zero/negative restore epochs and all 24 truncation
boundaries without partial clock application. Both engines build for all three Android ABIs. Native and imported Android
x86-64 runs pass 16/16 each, with deliberately contaminated clocks restored
through ordinary memory load and the actual authoritative rewind entry. Both
retain deltas `[8192, -1099511627776, -16384]`; all four captured projectile
frames have identical native/imported state and GPU pixels. Both helpers exit
0, restore the original app directories and remove their backup

Current sources, binaries, APK and completed evidence are frozen in
`temp/d1-cadence-persistence-source/manifest.json`. APK SHA-256 is
`155427e1a8f0ff1ef3d9f9d8c867d48419d8f49d84e8f3109131e31927698a50`.
No full-corpus recapture, ordinary-D2 loaded-world save, live network, ARM64
runtime or rewind-history selection claim is added by this milestone.
Level/ship/secret/network clock lifetime contracts and the other private-state
audit remain open

Earlier observer/replay follow-up: world schema 7 observes the signed 64-bit Fusion, collision
and refueling clocks. Fresh frame captures declare diagnostic schema 1, and
the comparator requires all 360 fields with exact scalar/array shapes. Its 51
tests pass; all 6,759 frames in the retained three-way short capture satisfy
that field contract without changing their archived schema or checker

The real refueling regression first failed only in the imported D1 profile:
it used D2's quarter-second sound cadence instead of D1's third-second cadence.
The semantics owner now selects the native interval, with ordinary D2 controls
before and after D1. Tests exercise actual refueling and Fusion firing, including
Fusion awareness and overcharge damage. That accessor-only milestone preserved existing lifetime behavior; the
persistence milestone above now supplies disk-save and rewind records

A fresh imported short level-7 capture exposed missing replay metadata:
`Collision_delay_last_play_time` restored as 0 instead of the recorded 33286937.
Evidence is `temp/d1-collision-clock-before/metadata-comparison.json`; its source,
host binaries and APK are pinned in `temp/d1-private-observation-source`.
Replay collision-clock restoration now runs in shared startup for native and
translated checkpoints, replacing the duplicated engine replay branches.
Android rewind metadata retains its existing restore path. The comparator
adds an independent recorded-checkpoint clock check, so equal omissions in
both engines cannot pass. Both host suites pass 53/61 and all three Android
ABIs build. Sources/binaries/APK are frozen in `temp/d1-collision-clock-source`.
The completed `temp/d1-collision-clock-parity/report.json` passes all seven
native-repeat and native/imported checks: 2,253 frames, 2,257 world/boundary
records, 6,818 SIM events, 2,142 FX events and the terminal result. All three
runs restore the recorded collision clock 33286937. Historical recording/native
diagnostics, SIM context metadata and terminal summary still fail independently;
RNG values match. Full qualification remains incomplete

The ordinary D2 before/after control also passes without canonicalization:
563 frame/object/world records, 8,821 RNG events and terminal result match; both
restore collision clock 6065102. All 1,126 frame diagnostics validate. Evidence
is in `temp/d1-collision-clock-d2-control`, also pinned in the source capsule

Next: complete network clock lifetime contracts and the remaining private state. Preserve
native behavior deliberately: these clocks currently survive level preparation,
and their first-use guards handle a backwards game-time origin. Do not add
blanket resets merely because they are private. Compare real transitions and
first-use behavior before changing that contract. Endlevel and boss/network caches,
the remaining saved globals and full corpus remain open

Persistence contract: ordinary saves write zero as their game-time origin;
replay and rewind supply their own epoch on restore. The shared cadence record
retains full-width relative timing. Nonzero epochs, populated past/future
deadlines, native import, ordinary re-save/reload, authoritative rewind, byte
order, truncation and legacy decoding now have targeted evidence above.
Actual ordinary-D2 loaded-world persistence and single-player transition coverage
now pass as described above. Rewind-history selection remains a separate follow-up

Endlevel audit detail: `was_located[MAX_FLY_OBJECTS]` in both flythrough
functions is Android log suppression only; its sole reader gates `debug_log`,
not movement, RNG or sound. Record that exclusion instead of serializing it.
The explosion waits, sound counter, transition timer, bank rate and external
explosion state do affect cutscene execution and remain in scope. The native
boss `eclip_state` controls outgoing network effect messages and is also in
scope; matching local effect flags alone does not cover it

The wider static-state sweep also found these concrete follow-ups, beyond the
first three clocks. None is yet qualified by the current short capture

- `collide_player_and_player` keeps `last_player_bump[MAX_PLAYERS]` privately
  in both engines. It gates multiplayer damage by packet cadence, so include
  it in the network reset/restore and repeated-contact tests
- `dead_player_frame` retains `time_dead` (function-local in D1, file-local in
  D2). It advances death rotation, camera distance, explosions and player drops;
  audit the complete death phase with its other globals, not just object state
- `draw_cloaked_object` keeps pulse timer/direction/delta across draw calls.
  This is presentation state with a visible-object-count dependency, for the
  matched-renderer F5 checks
- `check_rear_view` keeps sticky-view mode and a wall-clock entry time. Include
  it in camera/input lifecycle and simulation-invariance coverage
- `set_homing_update_rate` sets cached original-homing policy and cadence.
  Existing object runtime observations cover part of the cadence; verify the
  configuration-to-cache initialization contract before excluding derived state

Latest correction: native AI path indices retain their signed-short width
through import and version-36 saves. Original 30-byte AI records also carry
the two follow fields through the current D2 network object converters.
Ordinary D2 and optional companions retain their D2 layout; legacy save
versions retain their old decoding. Both host suites and Android live restore
pass. The active checkpoint case restores index 256 and advances to 257 in
both engines. Levels 1/7/27 each pass seven checkpoint scenarios and 28
resumed frames; levels 7/27 also pass 30 boss checkpoints each.
Current sources/binaries/APK are in `temp/d1-path-width-source/manifest.json`

Earlier correction: native boss death-sound state now retains its full signed
integer through import and D2 save version 35. The old byte-global/four-byte
write is removed; versions through 34 keep their original signed-byte read
semantics. Both host suites (53/61), runtime checkpoints on levels 1/7/27,
all Android ABI builds and the isolated Android Spreadfire/live restore pass.
The final device helper exits 0 and restores the original app directories.
Each level passes seven scenarios and 28 resumed frames; levels 7/27 also
pass 30 boss checkpoints each. Sources, binaries, APK and completed evidence
are pinned in `temp/d1-boss-sound-width-source/manifest.json`. The remaining
width finding was the native short AI path index; the implementation above addresses it

Path-width implementation contract: retain the native signed short in runtime
and import. Use an explicitly typed original 30-byte AI record for native D1
actors in version-36 saves and the next D2 network protocol, with unchanged
object-record size. Preserve old save decoding and ordinary D2/companion
records. Cover both byte orders, real native morph/checkpoint state, resumed
path use and the actual multiplayer object converters; wider network session
qualification remains a separate gate

The trigger/comparator milestone is implemented and has current host/device
verification. Save version 34 retains original trigger type/link bytes without
changing the 52-byte core record. Object schema 2/world schema 6 retain the raw
storage; checked mappings preserve native fields and unknown members

- All 44 comparator tests, 53 native-D1 tests, 61 D2 tests and seven real
  checkpoint scenarios pass. The 28 resumed frames now also compare native
  AI diagnostic hashes, bucket hashes and original follow-path values
- Additional level-7 and level-27 checkpoint runs completed successfully.
  Each covers seven restore scenarios, 28 resumed robot frames, 21 live
  projectiles and 30 boss checkpoints across all five difficulties. Retained
  logs are `temp/d1-ai-semantic-diagnostics-level7.log` and
  `temp/d1-ai-semantic-diagnostics-level27.log`
- `temp/d1-trigger-canonical-comparison/report.json` compares the first new
  capture with the audited mappings: all 2253 object frames, 2257 world/boundary
  records, 6818 SIM events, 2142 FX events and terminal results match. Twelve
  old AI frame diagnostics still differ because they used D2 layouts/sentinels
- The shared diagnostic producer now uses native field order and preserved
  values for native actors. Ordinary D2 and optional engine actors retain
  their D2 layout. Both host builds and all three Android ABI builds pass
- `temp/d1-ai-semantic-parity` is the fresh paired capture after that fix.
  Its completed report passes all six native-repeat and all six
  native/imported checks, including every frame diagnostic. Recording/native
  still fails independently; full qualification remains incomplete
- Final APK SHA-256 is
  `0cb54a2c3fb5c6b4d0943ba340b2ad80e27e41c8462f6ea591d0f5f003152ad9`.
  Android Spreadfire/live restore passes 16/16 with native-identical pixels.
  The helper then failed during adb cleanup; manual recovery restored all
  580 original files with matching hashes, removed the backup and reopened
  the launcher. Preserve the separate gameplay and cleanup outcomes
- Source, binaries, APK and stable checkpoint/device evidence are frozen in
  `temp/d1-ai-semantic-diagnostics-source/manifest.json`. The ledger records
  exact contracts, logs and coverage limits. Its evidence index now includes
  the completed paired report and level-7/27 logs, with hashes; frozen source
  and binary identities were preserved

**Next milestone:** complete the remaining saved-state and private-runtime
observation audit, then qualify the full corpus with the resulting stable
schema. Do not equate the short case's current observed-state match with F1
completion

1. Carry forward `d1-ai-semantic-parity/report.json`: all six native-repeat
   and native/imported checks pass. Keep recording/native failures separate
   and preserve the frozen checker/inputs. This short case is the current
   observed-state baseline, not a substitute for missing observations
2. Carry forward the boss integer and native AI record corrections, including
   current/legacy checkpoint and endian controls. Continue the remaining
   saved-global audit. Version-36 core records and the legacy AI extension
   both carry follow fields; review that duplication under F4. Live native
   object join/rejoin with protocol 30076 (Android)/30023 (desktop) is not yet
   qualified; converter tests alone do not close network session coverage
3. Complete observation/lifetime contracts for private mutable state,
   including Fusion warmup cadence, collision delay, refueling audio,
   endlevel transition caches and boss/network effect state. Audit remaining
   saved globals and frame-diagnostic completeness together. Retain raw
   evidence and all populated native fields; unknown fields remain differences
4. Once those contracts and mappings are reviewed, run the dynamically
   discovered complete D1 corpus (currently eight recordings), with two
   native captures and one imported capture each. The short comparison took
   259.11 seconds before the diagnostic fix; allow sufficient capture and
   comparison time for the long level-7 recording and preserve live runs
5. Fix the earliest real discrepancy in its owner with a focused regression.
   Follow the presentation, optional-feature, persistence and platform gates
   below; native D1 remains the reference until a separate retirement decision

## Concrete continuation batches

This research refresh checked the retained reports/logs and current source;
it did not rebuild or run gameplay. Existing passes apply to their recorded
builds, not automatically to every unrelated change in the current dirty tree.
The source findings below remain present at the time of this refresh

| Order | Deliverable | Completion evidence |
| --- | --- | --- |
| 1 | Completed: reconcile the earlier frozen evidence index with the paired report and boss checkpoint runs; preserve original captures and checker versions | Hash-linked report, source/binary identities and separate native repeatability, engine parity and historical recording failures |
| 2 | Boss sound state and AI path-index corrections implemented; verified through current/legacy checkpoints; carry their regressions forward | Actual native checkpoint import, re-save/reload and resumed-frame comparison; malformed/endian and applicable prior-format controls; ordinary D2 coverage |
| 3 | Complete the private-state inventory and trace contract for Fusion, collision delay, refueling, endlevel and boss/network caches; require the full declared diagnostic field set | Every relevant field has an owner, observation point and reset/restore rule; missing fields fail validation; focused lifecycle cases exercise populated state |
| 4 | Run the full discovered eight-recording corpus with stable observations, then repair the first real mismatch in each failing case | Two repeatable native captures and one imported capture per recording; exact frame-zero, frame, terminal and both RNG comparisons; no skipped cases counted as passes |
| 5 | Extend presentation verification beyond stock Spreadfire | Custom art replacement/retirement, complete D1/D2/D1 switching and native-checkpoint rendering on Android; key-icon visual check; effects listening with music off/on |
| 6 | Complete optional Guide-Bot, persistence and resource cleanup | Cold spawn, morph/flare/sound, restore/error/retirement cases; unchanged baseline with optional D2 data; real save/load/rewind and normal/secret/death/co-op transitions |
| 7 | Qualify the declared platform/edition scope | Windows and Android including arm64 runtime; ordinary D2 regressions; explicit unsupported configurations; F1-F5 acceptance review |

Continue with item 3; the final checkpoint run has passed. The storage regressions
first proved exact losses: boss sound state 256 became 0, and path index 128
became -128. The current code preserves both, with explicit legacy decoding.
These findings do not establish the cause of the reported Spreadfire
appearance. The remaining work is private-state coverage and broader
lifecycle/qualification, rather than repeating the resolved width repairs

Keep presentation work scoped to unverified configurations: the existing
stock Spreadfire and physical co-op level transition already have passing
evidence. Do not restart the obsolete September 7 bitmap-remapping plan.
The older recordings remain diagnostic evidence when current native and
imported engines agree with each other but disagree with the recording

## Research refresh: actionable next implementation package

This planning-only review rechecked the current source, the eight D1 demo
paths, `d1-ai-semantic-parity/report.json`, the path-width source manifest,
both final host-suite logs, all three runtime checkpoint logs and the Android
restore log. The retained results support the stopping point above. No new
build, replay or device test was run, and existing binaries were not certified
against the entire current dirty tree

Implement batch 3 as a bounded state/lifetime package before paying for another
full corpus capture. Start with the following source-confirmed gaps; these
are audit targets, not yet demonstrated gameplay divergences

| State or check | Current source finding | Next change and acceptance case |
| --- | --- | --- |
| Fusion cadence | Both engine clocks are now exposed and traced. Actual firing tests prove the gate controls awareness and overcharge damage/SIM RNG as well as sound | Define new-level/ship/restore behavior and compare populated warmup and overcharge state before and after actual save/load and rewind. Do not classify it as presentation-only |
| Collision delay | The clock is traced, replay metadata restoration is verified, and current disk saves retain a full-width relative value. Actual campaign transitions and first-use RNG now match | Carry the persistence and transition regressions forward; complete applicable network and rewind-history selection coverage |
| Refueling cadence | The original sound clock is now exposed and traced; the imported cadence is corrected to one-third second. D2's repair-center clock remains separate | Check refueling across restore and level/session switching and complete its reset/restore contract without importing D2 repair behavior |
| Endlevel and boss effects | Endlevel timers/flythrough caches remain private. Native boss `eclip_state` and the owned `Boss_gate_effect_state` have different reset locations | Audit each cache's consumers and lifetime before adding fields. Exercise consecutive transitions and boss effect start/stop across applicable restore and network boundaries; exclude only proven diagnostic-only state with a written reason |
| Required frame diagnostics | Implemented: all 360 declared fields and exact array lengths are required for fresh diagnostic schema 1; unknown fields remain compared | Carry the missing-from-both and malformed-field regressions forward. Keep historical recordings and frozen older checkers separate; completeness of this diagnostic list does not prove complete simulation observation |

For every relevant field, record its owner, type/width, observation boundary,
initialization, reset, disk-save, rewind, replay and network rules. Extend
`input_demo_world_trace.cpp`, `d1_replay_parity.py` and its comparator tests
together. Use `test_upstream_compat.cpp` and `test_d1_ai_checkpoints.ps1` for
actual populated-state restore cases. First reproduce any loss, then repair
its owner; do not bump save formats just to expose a diagnostic field

Package exit: required observations fail closed, lifecycle assertions pass,
both host suites pass, applicable Android restore checks pass, and source /
binary / asset / harness identities are frozen. Audit the remaining saved
globals before calling F1 complete. Only then run two native captures and one
imported capture for every discovered D1 recording, fixing the first real
difference with a focused regression

The runner currently initializes `qualification` to `incomplete` with a fixed
coverage-gap list. As gates are completed, replace stale descriptions with
evidence-backed gate status; neither a permanently incomplete label nor an
automatic green label from matching traces is a useful completion rule.
Keep recording/native, native repeatability and native/imported verdicts
independent. The retained short case passes the latter two while recording
frames, SIM RNG and terminal state still fail

Presentation remains a separate deliverable: reuse the stock Spreadfire pass
and extend custom replacement/retirement, D1/D2/D1 switching, imported native
checkpoint rendering, key-icon visuals and audible effects checks. Keep the
native D1 executable until the broader F1-F5 acceptance gates close and a
separate retirement decision is made

## Earlier follow-ups and their evidence

The cloak-fixed paired replay has finished and remains
incomplete. Native repeatability passes all six checks; imported SIM/FX RNG and
terminal results match, but frame/object/world comparisons still fail. Focused
inspection confirms the cloak-memory and door repairs across 2255 world records.
The comparator has 34 passing tests, including reconstructed object cardinality.
The Android build and isolated actual laser playback check pass

Implementation follow-up: the co-op flyout close veto is repaired. Both imported
peers now pass physical exit, score, briefing, level-2 thrust/firing and menu
checks. Android view introspection additionally proves the touch overlay is
active/shown/attached, and real Android Back opens and closes the menu on both
peers. Native-D1 physical transition and ordinary-D2 natural movie controls pass.
See the ledger's latest entry and `temp/d1-coop-transition-ui.log`. The next
priorities are strict replay observation/mappings and the remaining presentation
lifecycle checks; the tested transition is no longer a pending fix

Weapon-art follow-up: the new isolated Android `-WeaponArt` regression passes
native D1, D1-only imported play and D2 startup followed by First Strike. Real
Spreadfire firing, both orientations and each engine's live save/restore retain
exactly matching state, indexed pixels, palette and 1920x1080 GPU output. Source
and captures are pinned in `temp/d1-weapon-android-source` and the three
`temp/d1-weapon-android-*-evidence` directories. Custom-art lifecycle, a full
D1/D2/D1 round trip, native-to-imported checkpoint rendering and arm64 runtime
remain open. This adds verification, not a new Spreadfire rendering fix

AI persistence follow-up: the translator no longer discards the two original
follow-path fields or the three old local timing fields. Owned runtime records
retain them, and D2 save version 32 preserves them through disk/memory save
extensions. Seven actual native-checkpoint scenarios pass, including inactive
local storage, re-save/reload, resumed frames, malformed/endian handling and a
prior-format control. The raw observer now uses object schema 2/world schema 4
and emits the new D2-owned `d1_saved` records. Existing schema-3 paired captures
still need their frozen checker. Native/imported mappings, remaining private
runtime observations and whole-corpus qualification remain the next F1 work

AI comparison follow-up: the comparator now relocates five preserved
`d1_saved` fields and the two original boss integers to their native names for
D1-content comparisons. It keeps all D2-only and unknown fields visible,
preserves raw traces and rejects incorrect executable/storage identities.
The 39 comparator tests pass. This is a partial mapping, not a strict replay pass

Boss-state follow-up: the owner now retains native `Boss_been_hit`, sets it on
actual damage and clears it for a new ship independently of pending weapon
contact. The translator and version-33 save extension preserve both original
boss integers exactly, including non-Boolean pending values that the older
adapter reduced to a Boolean. World schema 5 exposes the damage marker.
Network cloak acknowledgement still clears only pending contact. D1 damage
no longer updates D2's unused boss-hit timestamp. See the ledger for final
build, checkpoint and device evidence and their limits

## Authority and scope

Start here for the next work session. This refresh supersedes pending-run status
and immediate priorities in the [September 23 handoff](d1-in-d2-continuation-20260923.md).
The [consolidation plan](d1-in-d2-consolidation-plan.md), section 0 F1-F5 and
section 16, remains the architectural and acceptance contract. The
[implementation ledger](d1-in-d2-implementation-ledger.md) contains detailed
implementation history and evidence

Research inspected HEAD `361167d9`, the existing dirty tree, source, retained
test logs, reports and running processes. No builds, gameplay tests or device
scenarios were started for this refresh. Results below are retained evidence,
not fresh validation of every current file. Music, Guide-Bot/routing, cockpit
and LAN work also occupies this tree; preserve those changes and establish
build/device ownership before implementation

The June overlay design and September 7 weapon audit are historical. Current
D1 sessions publish independent original D1 resources. Do not implement their
old unchecked bitmap-remapping checklist against the new architecture

## Where work stopped

| Area | Verified evidence or source state | Still open |
| --- | --- | --- |
| Original assets and gameplay | Independent D1 base/custom generations; dedicated AI, weapons, semantics, campaign and presentation owners; retained D1-only Android interaction/transition passes | Complete integrated fidelity, lifecycle and edition coverage |
| Spreadfire | Original projectile 20/accounting 12; ten matching host frames including custom art/D2 switching. Android native, D1-only imported and D2-startup imported runs now match source/palette/GPU pixels for both orientations and live save/restore | Android custom lifecycle, full D1/D2/D1 round trip, native-to-imported checkpoint rendering and arm64 runtime. Controlled frames do not disprove every reported visual failure |
| Checkpoints | Real-save comparisons cover creation/hit history, morphs, flares, effects, reactor timing, automap, secrets, original AI path/timing fields, both original boss integers and original trigger bytes/full link storage. Current save version is 36 | Remaining saved globals/private state, complete actual save/reload/rewind and transition/network coverage |
| Strict replay | `temp/d1-ai-semantic-parity/report.json` passes all six native-repeat and native/imported checks, including every frame diagnostic, object/world records and both RNG streams | Private-state coverage, full corpus and independent recording/native disagreement remain open |
| Door state | Existing fix preserves native completed nonautomatic-door OPENING state; focused and loaded comparisons pass. Walls 86/87 match throughout the cloak-fixed capture | Network lifecycle remains separate |
| Cloak memory | Existing working-tree fix gates D2's continuous cloak-memory refresh during D1 gameplay. Before-fix imported fixture fails; host suites pass 53 native and 61 D2 tests. All eight cloak times/positions match across the fresh capture; Android compilation passes | Whole-world parity remains open |
| AI/object observer | Object schema 2/world schema 6 preserve original AI, boss and trigger fields; current mappings and validation have 44 passing comparator tests. Complete slot-zero default plus exact exceptions retain inactive storage | Private runtime observation, saved-global/width audit and whole-corpus qualification |
| AI encoding verification | `temp/d1-ai-codec-equivalence.json` reports all 6,764 native records equal across schemas 2/3 after lossless decoding; no first difference | This verifies encoding, not native/imported fidelity |
| Android effects | Per-sample rate/exact-rate conversion repair and owned briefing dispatch are implemented. Current device check passes all 21 steps, including actual laser playback and exact sample-conversion duration | Audible pitch/listening and music interaction; prior OS failure is superseded for this isolated check |
| Co-op level transition | Shared flyout window retirement repaired; imported physical transition, both Android overlays and Back/menu paths pass. Native-D1 transition and ordinary-D2 natural movie controls pass | Wider transition/network configurations and arm64 runtime |
| Optional Guide-Bot | Production preparation calls `d1_in_d2_prepare_available_guidebot`; source attachment/publication exists | Full actor, restore, error and retirement lifecycle; baseline invariance |

Earlier cloak-fix host logs: `temp/d1-cloak-frame-fixed-buildd1.log` and
`temp/d1-cloak-frame-fixed-buildd2.log`. That Android build evidence is
`temp/d1-cloak-frame-android-build.log`; frozen APK `temp/d1-cloak-frame-app.apk`
has SHA-256 `3306b2ce217b261f2b9a2294dacb008bc4e4853293b194f864b17d23fdba1ea3`.
Sound evidence is `temp/d1-cloak-sound-android.log`, matching automation run ID
`2c214af487804da693cd2259adfee97e`

Latest boss-state evidence uses `temp/d1-boss-state-*` logs. The frozen APK is
`temp/d1-boss-state-app.apk`, SHA-256
`77ecb3a22066e2a3f980e8587e0baebcbcbef9947498e562ef3d36f20a645410`.
The Android live-memory-save/Spreadfire comparison passes 16/16 steps with run
ID `f0e7c4074d8a4f2d9f6c24f7759d2596`. Exact nonzero boss-state persistence
has host evidence; this device case covers the real memory-save format and
projectile rendering with its normal zero boss state

All three paired reports (`d1-door-ai-parity`, `d1-ai-default-parity` and
`d1-cloak-frame-parity`) now exist. Preserve their frozen provenance; schema-2
captures require their archived comparator. Focused cloak/door evidence is
`temp/d1-cloak-frame-focused.log`. Complete object validation and cardinality
evidence are `temp/d1-object-schema-real-traces.log` and
`temp/d1-object-cardinality.log`; the latter validates 2255 reconstructed
records in each of three captures. The completed paired report used its frozen
checker, not the later cardinality addition

## Recommended delivery sequence

### 1. Reconcile the handoff and freeze the next baseline

- Carry the completed reports, focused fixes, current 44-test checker result,
  repaired transition and Android weapon-art evidence forward. Keep native
  repeatability, native/imported and recording/native separate
- Use the completed `d1-ai-semantic-parity` report as the current short-case
  baseline. Earlier cloak/trigger reports explain resolved differences;
  do not schedule fixes from their superseded mismatch lists
- The earlier capsule's running status is reconciled and completed evidence
  is hash-linked. Freeze the save-width correction separately so the previous
  short replay remains tied to its actual executable
- Pin the source diff, executable/APK hashes, assets and harness before the
  next fix. Preserve unrelated dirty-tree changes and existing artifacts

Exit: one current, reproducible baseline; every retained failure is classified
as a real state difference, a representation difference or incomplete evidence

### 2. Resolve the user-visible Android failures

The reported synchronized level-1-to-2 failure is repaired and verified on
Android x86-64. Keep the regression while addressing the remaining art/audio
checks. The investigation below records the completed repair and its boundary

- Reuse `test_lan.ps1 -Game d2 -D1LevelTransition` and its native-D1
  `-Game d1` control; both now pass. The earlier native reactor timeout was
  not reproduced and remains unclassified
- The repaired window-lifetime defect was in shared
  `coop/coop_briefing.c`: `flyout_handler` returned 1 for
  `EVENT_WINDOW_CLOSE`, while `d2/arch/sdl/window.c:window_close` treats a
  nonzero response as cancellation. `coop_flyout_render` requested closure
  without checking success. The draw callback also advanced endlevel physics
  unconditionally and computed frame time from millisecond elapsed time.
  The retained crash address resolves to this callback. It now permits close,
  skips zero-elapsed physics and does not advance an ended sequence. Both
  windows report retirement before successful next-level synchronization
- The regression now requires two connected players, active Android overlay,
  thrust, firing, Android Back/menu access and no pause/window leak. Preserve
  the original failing logs and the successful native/imported controls.
  Extend configuration coverage as part of F5 rather than repeating the
  original diagnostic runs
- Extend the successful effects-rate check with audible duration/pitch and
  music-off/on comparison. Require matching automation IDs and playback
  assertions. Coordinate MIDI performance with the existing music work
- Keep the passing Android Spreadfire stock/both-orientation/live-restore
  regression. Extend it to custom replacement/retirement, a full D1/D2/D1
  round trip and native-to-imported checkpoint rendering. The current
  both-installed run covers D2 startup followed by First Strike, and each
  engine restores its own save. Visually verify key-icon scaling separately

Exit: the synchronized transition retains controls/menu/overlay, and device
audio/art checks have recorded outcomes on a pinned build. Add regressions for
reproduced defects; leave untested configurations explicitly open

### 3. Finish F1 and close measured F2 divergences

- Complete observation of transition caches and private runtime state; audit
  saved globals and object-subtype validation. Preserve populated inactive AI
  storage and complete restore/per-frame/terminal boundaries
- The preserved D2 `ai.d1_saved`/`ai_local.d1_saved` fields now map to their
  native counterparts with explicit content/storage checks. Selected neutral
  D2-only fields and extra capacity now have checked mappings; review their
  source contracts and the remaining globals. Raw AI hashes still describe
  different layouts in earlier captures; the updated producer now uses native
  actor fields/order and the short paired capture passes every diagnostic
- Both original boss integers are now preserved and mapped. Keep the actual
  damage, new-ship reset, pending-contact and checkpoint regressions while
  auditing the remaining stored state
- Trigger source bytes and their explicit mapping are now implemented. Verify
  the current change through the first milestone above; retain compound-flag,
  malformed/endian, old-format and full-link-storage regressions. Complete
  save/level/network lifetime review. Do not recapture the full corpus until
  current mappings and remaining observer gaps have been reviewed
- Define identity-checked mappings for genuine native/D2 representation
  differences, retaining raw records. Never blanket-map door states, omit
  populated native fields, copy actual values into expectations or relax
  fixed-point/RNG comparisons to obtain a pass
- Fix the earliest remaining real difference in its owning subsystem; prove
  it with a focused case and recapture before moving to the next difference
- Measure compact-observer capture/comparison cost on short level 7, then run
  the dynamically discovered complete D1 corpus, currently eight recordings
  including long level 7. Use two native captures and one imported capture,
  D1-only staging, original checkpoints and frozen provenance
- Extend actual save/load/rewind cases alongside these repairs. Historical
  recording disagreement remains a separate finding even when engines agree

Exit: fail-closed complete declared observations and exact supported
native/imported simulation parity across the corpus. Missing evidence,
timeouts and partial projections cannot produce a fidelity pass

### 4. Finish F3 optional features and F4 consolidation

- Exercise optional D2 data absent, invalid and valid; cold companion spawn,
  morph/draw/sound/flares, doors, save/restore and retirement across switches
- Prove that merely installing optional D2 assets does not change baseline D1,
  and that camera toggles do not change simulation
- Inventory callers before removing legacy overlay/capture/backup paths.
  Finish serialized identity ownership and normal/secret travel, death,
  resource failures and applicable co-op joins/transitions

Exit: one resource lifecycle, working optional companion lifecycle and no
unexplained migration hook; ordinary D2 remains covered

### 5. Complete F5 qualification before considering native-D1 retirement

Qualify registered-PC content on Windows and Android, including actual arm64
runtime evidence. Cover stock/custom resources, D1-only cold startup and
shutdown, menus/errors, D1/D2 switching, presentation and ordinary D2
regressions. Explicitly list untested shareware/OEM/Mac editions and other
platform/network configurations. Keep native D1 as the reference until a
separate retirement decision after these gates pass

## Reuse existing tools

| Purpose | Entry point |
| --- | --- |
| Strict paired replay | `android/tests/test_d1_replay_parity.ps1` |
| Comparator/schema checks | `android/tests/test_d1_replay_parity_compare.py` |
| Rendered Spreadfire comparison | `android/helpers/test_d1_weapon_art.ps1` |
| Android rendered Spreadfire and live restore | `android/helpers/test_d1_in_d2_android.ps1 -WeaponArt -NativeD1`; imported runs use `-WeaponArt -WeaponArtReference <native-evidence/weapon-art>`, optionally `-D2DataDirectory` |
| Real checkpoint/dynamic-state comparisons | `android/helpers/test_d1_ai_checkpoints.ps1`, including `-RuntimeState` |
| Loaded gameplay and campaign comparisons | `android/helpers/test_d1_gameplay_rules.ps1`, `test_d1_campaign.ps1` |
| Isolated Android launch/playback | `android/helpers/test_d1_in_d2_android.ps1`, including `-SoundCheck` |
| Co-op physical level-1-to-2 transition | `android/tests/test_lan.ps1 -Game d2 -D1LevelTransition`; native control uses `-Game d1` |

Use both host builds and relevant complete suites for significant engine
changes, scoped code quality, and JDK 21 for Android. Run emulator tests
serially, preserve logs/exits and matching automation IDs, and use retention
before new artifacts. Avoid graphics fixtures during live replay capture and
respect the Android build cleanup guard. Update the ledger with build hashes,
coverage limits and remaining failures; leave `android/outstanding_bugs.md`
unchanged per its instruction
