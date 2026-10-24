# D2X-XL first mission pilot

## Decision

Latest implementation constraint: the user explicitly permits ordinary guarded
hooks in d1/d2 and rejects generated-source CMake substitution. This supersedes
the earlier no-base-edits constraint and historical overlay checkpoints. The
runtime now uses opt-in `DXX_XL_PILOT` hooks with extension state kept in
`android/XL_compat`; default engine behavior must remain unchanged

Direct-hook verification: both `xl-render-preview` and `xl-preview-lifecycle`
build from the ordinary engine sources with `DXX_XL_PILOT=ON`; the lifecycle
suite passes. The normal `buildd2` game and metadata targets also rebuild with
`DXX_XL_PILOT=OFF`. Runtime configuration no longer generates or substitutes
engine C files. Scoped formatter/linter and diff whitespace checks pass. The
normal rebuild reports an existing C4113 warning in untouched `texmap/ntmap.c`;
the new hook sources add no compiler warnings

Mission isolation uncovered a real admission bug: retail Counterstrike and the
XL pilot share `d2leva-1.rl2`. Admission is now scoped to the `d2-xl` mission
(or the explicit private `xlprobe` test fixture), as well as the original hash.
The test renames its loose fixture copy before mission-level checks so it cannot
shadow retail HOG members, and verifies retail resolution from `descent2.hog`.
Pilot -> stock -> pilot passes fresh UVL/color/material checks and byte comparisons
of loaded robot, weapon, powerup and joint definitions. Intro skipping is a
consumed flag and must be set before each headless startup; failing to do so
entered the unsupported movie path. Custom override rollback remains separate

Latest completion scope, clarified by the user on 2026-10-23: a fully playable,
verified Ahayweh Gate port with faithful implementation of every XL engine
extension used by the pilot. Exact visual fidelity and XL author's changes to
features already present in 1996 are explicitly NOT requirements. Use Redux's
classic behavior as the baseline. The inventory, assisted route and browser are
intermediate deliverables only. This scope supersedes stricter visual-parity
requirements in the historical checkpoints below

## Current acceptance boundary after user clarification

Trigger coverage is now observed from the real `check_trigger` path, separately
from linked-target passability sampled during normal frames. The guarded hook
records successful native activations and resets on load; it never activates a
trigger. Route evidence aggregates across the earned key/reactor saves. The
original key/reactor/escape route covers only triggers 12, 9 and 0

Registered `xl_normal_branch_1` through `xl_normal_branch_12`. Each uses the
normal earned key route and restores that save to explore a specific branch.
Trigger 12 is already exercised on the key route, so its test retains that
evidence instead of attempting to shoot an already destroyed switch again.
The controller can shoot across a transparent barrier from a reachable firing
position, sample different points on a switch face, and collect energy during
branch exploration. These are input policies, not changes to engine rules

Latest complete sweep: timed escape passes unchanged (407 frames, countdown
2504568, shields 3333686; `escape-tests/db733127f8f0`). Eight branch tests pass
with both native activation and linked-target passability; four fail. Earlier
passes from other controller revisions do not replace the current failures:

| Trigger | Current result | Artifact under `temp/xl-compat-ninja` |
| --- | --- | --- |
| 1 | Pass, 5513 branch frames | `branch-1-tests/ee39d933aefb` |
| 2 | Pass, 5460 branch frames | `branch-2-tests/66f9114e67ce` |
| 3 | Player dies before activation, frame 8754 | `branch-3-tests/7d9dddbba817` |
| 4 | Alive at 36000-frame limit, no activation | `branch-4-tests/883dda3cc281` |
| 5 | Alive at 36000-frame limit, no activation | `branch-5-tests/a4ad4bcb12b2` |
| 6 | Player dies before activation, frame 16944 | `branch-6-tests/c83e9ac49e2f` |
| 7 | Pass, 4206 branch frames | `branch-7-tests/54d091099323` |
| 8 | Pass, 2763 branch frames | `branch-8-tests/86c33683d235` |
| 9 | Pass, 2317 branch frames | `branch-9-tests/a20ec063ed05` |
| 10 | Pass, 2555 branch frames | `branch-10-tests/6f438b5ad094` |
| 11 | Pass, 4422 branch frames | `branch-11-tests/2d0c5847f640` |
| 12 | Pass on the earned key route | `branch-12-tests/43cdebe63d83` |

The latest sweep is deliberately failing, not qualified. Trigger 4's host in
segment 52 is behind the two closed walls it opens; approach planning now tests
projectile visibility rather than assuming the player can fly to every host.
Triggers 5/6 are on those barriers. Their actual activation sequence remains
unproven. Target logs distinguish door opening from the later flyable flag;
trigger 8 now demonstrates both. These checks do not yet prove physical player
traversal of every opened target or branch-specific save persistence

Branch exploration initially changed the established reactor route and caused
it to fail. Its additional supply/search policies are now confined to branch
exploration, and the ordinary key/reactor/escape policy passes again. Investigate
remaining failures from current position/goal/switch logs rather than selecting
a favorable random seed or altering combat rules. Lifecycle checks, opt-in and
default D2 game/metadata builds, scoped formatting/lint and whitespace checks
pass; the four branch failures remain visible

Timed native escape now passes in an initial run: `preview-tests/819174b41b20`
restores the earned reactor save without changing shields, energy or countdown,
then reaches native completion in 407 frames with countdown 2504568 fixed units
(about 38.2 seconds) and shields 3333686 (about 50.9). The input controller skips
supply/combat detours during escape; simulation, damage and the countdown remain
normal. No trigger/completion handler is called by the controller

The first run crashed after reaching the exit because headless startup has no
game window and `start_endlevel_sequence` unconditionally dereferenced it. The
ordinary live-exit visibility calls now check for a window. The guarded pilot
completion hook runs from `AdvanceLevel`, awards normal bonuses, and ends the
single-level session instead of trying unsupported level 2. Only the headless
pilot skips the modal score presentation. Interactive score/menu behavior still
needs runtime verification. `xl_normal_escape` is registered separately and
requires both earned key/reactor checkpoints and native completion evidence

Repeatability now passes: `escape-tests/656546d46f1e/preview.log` and
`escape-tests/90b01acb943b/preview.log` report identical 407-frame results.
Each then verifies completion/admission state clears through stock and returns
fresh in the pilot. `xl_normal_escape`, `xl_preview_lifecycle` and
`xl_stock_loader_probe` pass twice. Both opt-in game/metadata targets and normal
XL-disabled D2 game/metadata targets build. Scoped formatting/lint passes;
existing `weapon.c` return-path warnings are outside these changes

This closes the timed-route gate, not all branch, authored extension,
interactive or Android acceptance gates

Next reconcile coverage against all 13 inventoried triggers, with
activation and target-state evidence per branch. Keep authored side/texture
color semantics, material targeting/projectile behavior and Android deployment
as independent open gates. The Dot survey supplies fixture priorities and
requirement leads; neither its parser agreement nor an exit run can close those
gates by implication

Normal reactor milestone now passes twice. `xl_normal_reactor` first earns the
red key, restores that exact save without replenishment, then uses normal inputs
to collect reachable supplies, cross authored door triggers and destroy the
reactor. The controller now uses portal-aware waypoints with full player-radius
checks; a recenter fallback that oscillated in segment 35 was replaced with a
destination-center fallback. A secret-area detour required physically crossing
an authored open-door trigger to get back out. The navigator reads trigger links
to choose the route but never calls the trigger handler or assigns wall state

Two consecutive runs agree: red key at frame 4887 with ten kills, then reactor
destroyed after another 6819 frames, player alive with shields 3333686 fixed
units (about 50.9) and countdown 2949120 (45 seconds). This updates the earlier
red-key controller timing below. Evidence: `reactor-tests/9d0acf156549/preview.log`
and `reactor-tests/70e9c8528af9/preview.log`. Each contains the earned
`preview-user/xl-normal-reactor.sav` (322815 bytes). Lifecycle, red-key and reactor
tests all pass twice; build and scoped formatting/lint pass. The countdown is
not paused or extended. Timed exit and remaining branches are still unverified

Normal red-key milestone now passes reproducibly with seed 1. The input
controller's earlier failures below exposed slow target tracking, wasted firing,
and a missing input-loop step: `do_weapon_n_item_stuff` handles missile/item
inputs outside `GameProcessFrame`. The controller now uses that normal handler,
aims ahead of moving robots, engages visible threats before advancing, strafes,
and seeks nearby reachable shields/laser upgrades. All changes are input policy;
the engine still enforces turn/thrust limits, collision, firing delay, ammunition,
damage and pickup rules

Registered `xl_normal_red_key` separately from the assisted route and lifecycle
test. Two consecutive runs both finish at frame 4356 (72.6 seconds), with nine
robot kills, red key held, player alive, 12 shields and level-2 lasers. The
actual player checkpoint is `preview-user/xl-normal-key.sav`, 314487 bytes in
both runs. Evidence: `navigation-tests/b20dccdcb87b/preview.log` and
`navigation-tests/752c2e2ab257/preview.log`. The lifecycle test also passes twice;
build and scoped formatting/lint pass. This supersedes the failing red-key
controller result below, not the remaining reactor/exit/branch requirements

Normal-input navigation investigation: an optional `NAVIGATION=ON` invocation
of `tests/preview_lifecycle.cmake` adds `-xl-test-navigation`. It starts the real
pilot with 38 robots and difficulty 2, checks combat cheats/invulnerability,
finds a path across currently passable or normally openable doors, and drives
only `Controls` through the normal frame loop. It uses ship rotation and thrust,
aims at visible robots, and fires ordinary lasers. No transform, health, enemy,
weapon damage, door or key state is assigned by the navigator

The first untuned run reached the red key at frame 2487 with only 25804 fixed
shield units remaining (`preview-tests/a5120ec209cc/preview.log`). That run did
not explicitly seed the RNG and is not the reproducible acceptance baseline.
With seed 1, the controller dies before the key. The current lead-aim/dodge
version kills four robots but dies at frame 2943
(`preview-tests/eb6360b2550b/preview.log`). The diagnostic returns failure and the
CMake runner requires a living key-pickup result; it cannot silently pass by
skipping navigation. Ordinary lifecycle CTest remains separate and passes.
Improve controller combat/navigation from the logged aim, range and damage
evidence; do not select a favorable seed or change gameplay rules to qualify
the level. Reactor, exit and all required branches remain unverified

| Required for the pilot                                                           | Not a completion blocker                                                                            |
| -------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------- |
| Correct 15/33 decoding and guarded admission, no silently dropped XL fields      | Pixel-identical rendering, resolution, filtering, contrast, cockpit appearance                      |
| Authored XL colors and opacity with their actual visibility/gameplay semantics   | XL changes to classic player thrust light, headlight cone, flare RNG or explosion palette averaging |
| XL-only wall/trigger/segment/object/resource behavior if required by these bytes | Reproducing XL tweaks to classic AI, physics, damage, weapons or ordinary resource presentation     |
| Normal Redux combat/progression through every required pilot branch and exit     | Bit-for-bit matching of incidental classic runtime normalization to XL                              |
| Persistence of extension state and stock-state isolation                         | Identical random sequences or rendered frames between different engines                             |

Do not confuse this exclusion with permission to discard extension data: a wall
that changes targeting, a trigger that changes progression, or an authored color
table still needs correct support. Classify discrepancies by their cause before
waiving them. Keep evidence scoped to this level and single-player; campaign and
multiplayer claims remain separate

Guarded original-source admission and controlled save/load now have the evidence
described below. Next work is required extension semantics, resource closure and
normal unassisted progression. Stop adding classic renderer-parity
adapters or aligning screenshots solely for visual matching. Existing experimental
adapters and tests can remain evidence, but must not become mandatory production
behavior merely because they were implemented before this clarification

### Survey reconciliation and remaining qualification work

Rechecked the supplied Dot conversation and downloaded evidence ZIP against the
preserved bundle. The archive hash is identical. The repository verifier passes
manifest, JSON/CSV and aggregate checks and the native same-hash pilot comparison.
This rerun uses the existing native inventory; it is not a fresh corpus scan,
new native parse or gameplay test. See `XL_compat/evidence/README.md` for provenance
and the two excluded object sections

Dot's ten areas map to the catalog's 19 rows. Apply them to the pilot as follows;
corpus prevalence determines later fixture priorities, not pilot requirements:

| Survey area | Pilot decision and next evidence |
| --- | --- |
| Formats and packaging | Keep exact-hash 15/33 admission; test original descriptor/HOG selection and reject unsupported sources. No blanket version admission |
| Capacities and widths | Current serialized counts fit. Check reference bounds and runtime spawn headroom; widening all XL tables belongs to later fixtures |
| Events and object triggers | Inventory has 13 classic wall actions and no object triggers. Exercise every action and branch; classic action IDs alone do not prove semantics |
| Movement transitions | Do not implement corpus teleports/boosts for a pilot that does not require them. Confirm absence across decoded actions before marking not required |
| Segment rules | Classify each serialized property using v15 semantics; distinguish required XL behavior from classic loader normalization |
| Generators and spawning | No pilot matcens are inventoried. Validate contained-item/runtime spawn dependencies; do not infer all spawning is absent |
| Wall materials | Test all 14 authored-opacity walls for rendering, actor/projectile passage and targeting independently; alpha and doorway flags alone are insufficient |
| Lighting | Account for vertex, side and texture tables and changing-light persistence. Trace consumers in the pinned reference/profile; unused in one mode is not universally unused |
| Effects and audio | Preserve object identity and validate referenced resources. Corpus effect counts do not establish pilot requirements or make missing cues cosmetic |
| Resources and custom data | Close transitive dependencies and test failed-load rollback plus pilot -> stock -> pilot isolation; direct object references alone are insufficient |

Maintain a requirement ledger with source hash, serialized location/value,
reference consumer, implementation path, behavioral test and unresolved scope.
For each non-neutral extension field, either demonstrate the behavior or document
why the selected profile does not consume it. Do not turn an unexplained field
into a silent default or a `not required` result

Qualification order: finish the authored-material/color and dependency audit;
exercise required semantics in the real engine; then complete ordinary combat,
keys, all required branches, reactor, countdown and native exit with normal rules.
Keep the existing assisted-route artifact explicitly separate. Controlled save
mutation tests prove persistence only. Follow with stock-state isolation and
the intended Android launch/import path before claiming Android support

Single-player pilot qualification does not require implementing XL multiplayer,
all 30 campaign levels or modern hub persistence. Those claims remain unverified
and unavailable until separately tested. Grayscale or omitted cues may be useful
diagnostic modes, but do not fulfill this pilot's authored-extension requirement

### Original-source admission checkpoint

Authored-material collision follow-up: the real-loader lifecycle test now traces
from each of the 14 nonzero-opacity illusion walls' segment centers into their
adjacent segments using `find_vector_intersection`. Point, small-radius and
loaded-player-radius traces all reach the expected adjacent segment. Each trace
also has a negative control: temporarily closing that same wall produces a hit
on the exact tested side, then its original type is restored. This verifies wall
passage independently of alpha and doorway bit assertions, for both serialized
opacity values (21 and 31). The rebuilt Windows lifecycle test and scoped
formatter pass. Objects are excluded from these geometric traces; weapon/object
collisions, AI targeting and ordinary combat remain separate requirements

Added `-xl-pilot` to the dedicated runtime target. The wrapper reads original
`d2leva-1.rl2` through PhysicsFS, checks its exact size/hash, runs the bounded
15/33 translator and verifies the private translated bytes before loading.
A generated file-open hook substitutes only the level stream; it preserves the
original filename used by engine mission/save logic. Extension state is enabled
only after successful loading and geometry checks. This is opt-in pilot admission,
not qualification or acceptance of other levels/versions

Real-loader coverage passes for the original source, rejection of a one-bit
source change, restoration, preserved Gamesave_current_filename, and original HOG
resolution (verified with PHYSFS_getRealDir), followed by color/material checks
and archive unmount. Existing unrelated-level isolation still passes. Evidence:
`temp/xl-compat-ninja/preview-tests/09706a3229a9/preview.log`. Both runtime targets
build; base d1/d2 sources remain unchanged. The startup helper can select the
original d2-xl mission with -xl-preview-start -xl-pilot, but that full mission
startup and ordinary gameplay are still to be exercised

Next verify mission descriptor/resource precedence and run original-source
progression/save checks without the assisted route's robot removal, invulnerability,
boosted damage, paused countdown or position commits. Authored side/texture color
semantics and opacity/collision interactions remain extension obligations. Classic
renderer tuning and matched pixel images are not blockers under the latest scope

### Original-mission save/restore checkpoint

Resource traversal now also follows every side's base/overlay textures,
destroyed-texture replacements, linked effects (including critical/destroyed
variants), model texture effects and wall animation frames. Combat, pickup,
effect and door sounds resolve through `digi_xlat_sound` to nonempty loaded
samples. Passing totals are 136 textures, 25 effects, nine wall animations,
56 vclips, 780 paged bitmaps and 51 resolved sound samples. An initial failure
on sound 0 was a verifier error: effect playback consumes `eclip.sound_num`,
not the sound field of its embedded vclip; `gameseq.c` ambient setup and
`collide.c` effect destruction confirm that distinction. The check now follows
the actual consumer. Headless dummy audio does not establish audible playback
or spatial mixing. Override precedence/rollback and resource isolation still
need their own evidence; these counts are not full mission qualification

First normal-input simulation: after the persistence checks, restart the original
mission and drive the real player for 600 frames at 60 Hz through `Controls`
forward thrust and primary fire plus `calc_game_time`/`GameProcessFrame`. No route
actor is started and no objects, positions, damage values or countdowns are
changed. Reject combat cheats/invulnerability and require player movement,
energy expenditure, player projectiles, robot projectiles and shield damage.
The run passes: 82 observed player projectile signatures, 22 robot projectile
signatures, player alive in segment 193 with shields 5916348 fixed-point units.
This is live simulation under normal rules, not a rendered playthrough or route
completion. Input navigation, all required branches and the reactor/exit remain
unverified. Fixed forward/fire inputs alone are not a competent completion policy

Loaded dependency follow-up: after real `StartNewGame(1)`, the lifecycle test
traverses placed robots, contained robots/powerups, robot weapons and smart blobs,
weapon children, the conservative full player arsenal, reactor shots, model
variants, animation joint ranges and vclip frames. Referenced object bitmaps are
actually paged in and checked for resident payloads and nonzero dimensions. A
negative control replaces a reachable robot's weapon reference with an invalid
ID and requires rejection, then restores the original field. Build, lifecycle
test and scoped formatting pass. This closes those loaded-data reference checks,
not the entire resource gate: audio, wall/effect texture chains, custom override
precedence and failure rollback remain distinct obligations. It does not prove
that all attacks, loot paths or animations have executed during ordinary play

Added optional XL_PILOT_DESCRIPTOR coverage using the outer mission descriptor
and original HOG. The embedded descriptor differs in displayed title and text
line endings; the regular/secret level lists agree. Runtime checks confirm the
outer D2-XL: Counterstrike! title wins, with 24 regular and six secret entries

The real StartNewGame(1) path loads the original pilot, retains all 38 robots
and does not grant invulnerability. The persistence test switches one light off,
changes a live authored-opacity record, writes a normal engine save, changes the
in-memory values, then restores. Mission/level identity, energy/flags, robot count,
all UVLs, live wall opacity and XL ambient/material activation pass. These are
controlled state changes, explicitly not combat/progression evidence

The test uncovered that the included metadata initializer ignores -userdir.
Selecting a write directory after initialization also fails because the console
log is already open. The isolated test now selects and mounts its private
directory immediately after PhysicsFS initialization, before console startup.
CTest requires the save artifact in that directory as well as the final success
message. The corrected run passes. Remaining gameplay and extension obligations
are unchanged; classic renderer parity remains out of scope

Rendering investigation: the pinned local XL source loads vertex RGB into
`gameData.renderData.color.ambient` in `LoadVertLightsCompiled`. The static face
path runs `SetVertexColor`, `SetVertexLight`, then `AdjustVertexColor`. It includes
face-color blending, dynamic-light/color mixing, brightness and reactor/seismic
scaling. `ScaleColor` caps the largest RGB component to the supplied light value;
it does not simply multiply every component by that value. An implementation
that multiplies raw vertex colors into stock lighting is not reference parity

Stock `render_face` already supplies per-vertex RGB to `g3_draw_tmap` and
`g3_draw_tmap_2`, with vertex identities represented by pointers into
`Segment_points`. Investigate an opt-in XL runtime target with a draw adapter
confined to XL compilation before considering any base-source changes. This is
an integration candidate, not an implemented or verified rendering path. The
adapter must retain face context and XL light state; matching one static image
cannot substitute for dynamic lighting, reactor and save/restore checks

An isolated vertex-lighting calculation now exists in `XL_compat/lighting.cpp`.
Windows x64 and Android x86_64 analytical cases and 8,192 comparisons against
functions extracted unchanged from the hash-pinned XL `renderlib.cpp` pass. The oracle uses substituted data and
vector containers; it establishes arithmetic agreement only. No renderer hook or
production admission was added. Light generation, runtime state mapping, texture
and face-color setup, reference visual checks and normal gameplay remain open

The existing reference profile `temp/xl-reference-ar12/user/profiles/xlref.plx`
has SHA-256 `702102254d6905a89b96e41a2bcf89af3327480dc841922cd99e87ff0247e514`.
Its saved normal-profile settings are lighting method 0, color level 2 and
lightmaps disabled; contrast is 8. These are the saved settings associated with
the prior startup experiment, not newly chosen settings to make a comparison pass.
They still need runtime confirmation. XL's `SetMethod` enables ambient colors at
color level 2; other methods can regenerate ambient colors instead of retaining
the file tables. Do not generalize vertex-path parity to per-pixel/lightmap modes

Added `vertex_brightness` to reconstruct XL's initial shared-vertex brightness
from the maximum decoded face UVL light, using pinned side corner ordering. Tests
cover unsigned high-bit values, shared vertices, open portals and portal walls.
This supplies a required input for the lighting calculation; it is not a renderer
integration or proof of dynamic-light generation

Runtime integration checkpoint: a separate Windows x86 `xl-render-preview` build
now compiles using `XL_compat/runtime/configure.cmake`. Its generated source
overlay adds one checked drawing hook, and an isolated loader wrapper resets XL
state on every load and verifies complete fixture bytes against the source
translation before enabling colors. Normal source files/builds remain unchanged.
This is a static-light preview only: dynamic lights, reactor/seismic flash and
special face colors remain unintegrated. Build and scoped formatting pass;
runtime/lifecycle rejection tests and images are still pending. No gameplay or
production compatibility claim follows from this build

Lifecycle integration now passes against the real stock loader with the isolated
wrapper: inactive state, preview activation, unrelated-level reset, reactivation,
same-size one-bit coordinate mismatch rejection, and restoration/recovery. The
test exposed stock MSVC headers leaking one-byte packing into XL color structs;
XL public C++ headers now bracket their own eight-byte layout without changing
stock packing. Inventory and both lighting tests still pass. This exercises the
loader and direct color hook, not the render loop or normal gameplay; those
acceptance gates remain open

Use **D2-XL: Counterstrike!**, starting with **d2leva-1.rl2 only**, as the first
XL format and gameplay pilot. This is a candidate for implementation, not a
claim that the mission currently works. Full campaign support remains unverified
until all 24 regular and six secret levels pass their own feature and behavior
checks. Do not mark the entire mission supported after completing the pilot

The immediate engineering milestone should be a read-only feature inventory in
the native engine, with explicit unsupported-feature diagnostics, before enabling
XL levels for play. Keep game-format knowledge there rather than adding a second
production parser in Kotlin or a tooling script

## Existing notes

- [Archive analysis](d2xxl_mission_archive_analysis_20260823.md): original 35-archive
  investigation, duplicates, variants, standard versus extended HOGs and import results
- [As-is import work](plan_d2xxl_as_is_mission_import_20260823.md): archive retention,
  cache omission and import/metadata checks
- [Extended-HOG diagnostic](plan_d2xxl_extended_hog_diagnostic_20260823.md): six
  rejected XL archives and D2-XL as an importable negative control
- [PO2 study](plan_po2_engine_feature_compatibility_study_20260823.md): example of
  apparently usable content with missing weapon/robot data and executable behavior
- `game_data/mission_files/d2xxl_downloads/po2_compatibility_notes.md`: PO2 handoff
- `android/outstanding_bugs.md`: outstanding XL archive analysis item

Those historical import and metadata results do not establish gameplay support.
In particular, the archive analysis's wording that extended-HOG missions need a
converter describes a packaging prerequisite, not sufficient engine support

## Fresh inspection on 2026-10-23

Source archive:
`C:/Users/first last/Desktop/d2x-xl levels/d2x-xl/D2-XL.7z`

- Archive SHA-256: `24060d3addb78bdafb8691e7c3a3137543f92b1f4b2f68d694c28703a3d14f3c`
- Two outer files: `d2-xl.hog` (9,871,510 bytes) and `d2-xl.mn2` (514 bytes)
- Standard `DHF` HOG, ordinary `name` descriptor, 24 regular and six secret levels
- All 30 RL2 headers are level version **15**, game-data version **33**
- Current `d2/main/gamesave.c` accepts level versions 1 through **8** through
  `level_envelope_is_valid`; its `GAME_VERSION` is 32
- Therefore successful archive import is not evidence these levels can load today
- HOG inventory includes palettes, fonts, briefing/credits, PCX art and music,
  plus one POG for `d2levd-s`; no HAM, HXM, PMH, PTI or external mod tree was found
- Outer descriptor and embedded `d2-xl.mn2` differ in size (514 versus 521 bytes);
  descriptor precedence and contents still need reconciliation

First-level raw inventory, obtained without loading it into the engine:

| Item                         | Value                                                              |
| ---------------------------- | ------------------------------------------------------------------ |
| Level SHA-256                | `5e835e76e984c658531175640294a4b05f6127904884ba7acfd65c93165be5b2` |
| Level bytes                  | 149,235                                                            |
| Vertices / segments          | 828 / 228                                                          |
| Objects / walls              | 110 / 86                                                           |
| Geometry triggers            | 13                                                                 |
| Trigger types                | Five open-door (0), seven open-wall (9), one exit (3)              |
| Trigger flags                | All zero                                                           |
| Maximum trigger target count | Four                                                               |
| Object triggers              | Zero                                                               |
| Robot centers                | Zero                                                               |

These counts and trigger types make this a bounded first candidate. They do not
prove object behavior, wall semantics, geometry or lighting compatibility. Object
types/IDs, wall types/flags, segment functions/ownership, every link, colors and
asset resolution remain to be inventoried. No runtime test was performed

Local XL source inspected at `C:/local/d2x-xl`, clean checkout commit
`11038eb53dd86f3bcdc027dddd3f93c60f02536e`:

- `main/segment.cpp`, `CSegment::Read`: XL owner/group bytes precede segment data
- `main/side.cpp`, `CSide::ReadWallNum`: wall references become 16-bit at version 13
- `gameio/loadgeometry.cpp`: embedded vertex, side and texture color tables;
  version 15 uses different component encodings across these tables
- `gameio/loadobjects.cpp`, `ReadTriggerInfo`: game version 33 adds object triggers
- `main/trigger.cpp`, `CTrigger::Read`: checked serialized trigger layout against
  the pilot's raw records

Simply increasing the version limit would misinterpret data. Reading wider wall
IDs into the existing 255-wall/sentinel representation would also be unsafe
without range checks. The pilot has only 86 walls, so it should not require a
general wall-capacity expansion if all references validate

## Why this mission first

It exercises real XL level data while avoiding extended-HOG handling, a new
descriptor dialect and a separate mod-asset tree. Familiar Counterstrike content
provides a useful stock comparison, though the XL version must be treated as its
own authored content rather than assumed identical

Other candidates are less bounded for this first step:

- Sphere: historical unsupported `d2x-name` descriptor plus its large cache-heavy package
- Anthology: freshly listed extended HOG plus OOF models and external textures
- Boiling Point: freshly listed extended HOG plus external replacement textures
- PO2: historical evidence of unloaded custom gameplay tables, custom executable
  objectives and wingmen; a particularly poor candidate for an easy success claim
- A vanilla-format mission merely downloaded from the XL collection would be a
  useful control, but would not demonstrate an XL level-format implementation

## Acceptance contract

Keep separate results for archive import, format decoding, gameplay, presentation
and campaign coverage. Use verified / unsupported / unknown / not applicable for
each feature, with evidence attached. Unknown is never equivalent to supported

1. **Inventory before admission**
   - Pin archive/level hashes and engine/source revisions
   - Produce a native per-level manifest covering versions, counts and limits;
     object types/IDs and behavior data; wall types/flags/keys; trigger types,
     flags and targets; segment functions/ownership; lighting and asset dependencies
   - Classify every extra field and asset as implemented, verified neutral,
     unsupported or unknown. Do not silently drop records or substitute stock data
   - Report exact level, feature and record index for blockers. Preserve existing
     rejection until supported decoding and feature checks are available
   - Run inventory across all 30 levels, but select only level 1 for the first
     runtime milestone. Other levels retain their own blocked/unknown status
2. **Decode faithfully**
   - Implement only the explicitly audited version combinations; do not blanket
     accept every format through version 15
   - Check section extents, record counts, IDs, references and representable limits
   - Compare normalized geometry, objects, walls, triggers and colors against an
     independent XL reader/reference dump, including values rather than just counts
   - Test rejection of truncation, unknown semantics and unrepresentable wall IDs
   - Verify stock D1/D2 loading and metadata remain unchanged
3. **Verify level 1 gameplay**
   - Establish XL-extension behavior from the pinned reference/source for these
     exact bytes; retain Redux's behavior for classic features
   - Exercise every trigger and target, keyed doors, wall changes, robot combat,
     pickups, reactor/countdown and normal exit through gameplay actions
   - Check all reachable branches and secrets, not just a route to the exit
   - Save/reload before and after representative state changes and verify persistence
   - Use introspection assertions and reusable automation; warping to the exit or
     directly setting completion state cannot prove playability
   - Verify leaving the mission restores stock assets and behavior
4. **Verify authored extensions separately from visual fidelity**
   - Account for embedded colors, palette, textures, briefing, music and sounds
   - Verify authored colors, opacity and required resource semantics. Do not
     require matched images/audio for changes to classic 1996 features
   - Disclose visual differences, but do not block completion on filtering,
     resolution, classic emitter tuning or other non-extension presentation choices
5. **Expand only with evidence**
   - Level transitions, secret entry/return, all remaining levels and their assets
     need separate checks before a campaign claim
   - Coop, demo compatibility and other XL missions remain unverified until tested
   - If no reference runtime is available, retain static-analysis-only status and
     do not promote the pilot to gameplay verified

## Work status

### Isolation requirement (2026-10-23)

User requires XL code not to pollute the base game. All XL-specific parsing,
policy, conversion, diagnostics and tests belong under `android/XL_compat/`.
Start with a standalone native CMake library/tool, with no dependency from the
stock game and no changes to `d1/` or `d2/`. Future integration must preserve this
boundary; do not insert XL format branches into the stock loaders or weaken their
validation. Determine from the inventory whether faithful translation into stock
data is possible; unsupported behavior or presentation must remain explicit

- [x] Locate original notes and downloads
- [x] Select a concrete pilot and inspect its actual archive and level headers
- [x] Check key serialization differences against local XL source
- [x] Record evidence limits and explicit acceptance criteria
- [ ] Native feature inventory and diagnostics
- [ ] Independent semantic comparison and XL reference run
- [ ] Guarded version-15/game-33 decoder and rejection coverage
- [ ] Level 1 gameplay and presentation validation
- [ ] Remaining campaign and secret-level validation

## Isolated implementation checkpoint

`android/XL_compat/` now contains a standalone native library, inspection CLI,
synthetic tests and optional pinned-archive CTest integration. It has no dependency
from the stock engines, and `git diff --name-only -- d1 d2` is empty

The reader decodes all 30 levels' geometry, objects, walls, triggers, embedded
colors, header data, model-name tables, reactor links, generators and delta lights.
It accepts only the audited 15/33 version pair and never grants gameplay admission.
It explicitly retains unresolved dependencies and runtime semantics in the report

Fresh level 1 findings:

- 392 of 828 vertex colors, 36 of 1,368 side colors and eight of 910 texture
  colors have unequal RGB components. A grayscale conversion would lose content
- 110 objects decoded, including 38 robots (one Guide-Bot), four hostages, eight
  player starts, four coop starts, one reactor and 55 powerups
- The two special segments beyond fuel/reactor are existing blue/red goal types,
  not repair centers. All segment flags are existing ambient-water/lava bits
- No object triggers; the retained 700-entry legacy head table is entirely `-1`
- 70 light-index records and 3,960 delta-light records decoded

Verification completed: MSVC CMake build, synthetic CTest, 30-level private-corpus
CTest, Android x86_64 build and emulator execution of the synthetic test and full
archive inspection. Host and Android JSON reports are byte-identical. Scoped
format/lint checks pass. The Visual Studio CMake generator hit sandbox FileTracker
access restrictions; the Ninja/MSVC build succeeded. The installed formatter
required an approved elevated scoped run

Next: independent XL semantic/reference validation and asset-aware dependency
checks, then isolated gameplay/presentation integration. No converter or loader
has been enabled; colored lighting must be preserved rather than silently omitted.
The full acceptance contract above remains open, including runtime verification

### Reference runtime preparation

Downloaded the source repository's pinned Windows release `v1.18.74-ar12` into
`temp/xl-reference-ar12/`, verifying the GitHub release asset's SHA-256:
`1fb635000a0c12b6497915fff81b48b0b57c7fd902f1d2d616fc84b8d00fb7d4`

- Release: `https://github.com/arbruijn/d2x-xl/releases/tag/v1.18.74-ar12`
- Local source tag: `d2b3e3f1f1594d1d7da2ec545f6fc75d18043866`
- Executable SHA-256: `77db2a39a2293b10554986eeb0c6a4b664621cfc9839bf5686ad9bd402505e18`
- No diff between that tag and the inspected source HEAD for `loadgeometry.cpp`,
  `loadobjects.cpp`, `segment.cpp`, `side.cpp` or `objectio.cpp`
- Original local retail data and the XL mission have been copied into the scratch
  `game/data` and `game/missions` folders, with data hashes in `data-manifest.json`
- Separate scratch `user` and `cache` directories are prepared for the reference
  executable's `-datadir`, `-userdir` and `-cachedir` flags
- The reference executable loaded level 1, Ahayweh Gate, and saved an in-engine
  starting-room screenshot. Log: `temp/xl-reference-ar12/level1-reference-load.log`;
  screenshot: `user/screenshots/Ahayweh_Gate-0000.tga` under that reference folder
- Screenshot SHA-256: `a010ee9b8819920f72702b9951fed66e5761c0967b79ef3fd7e54e98d9fce399`
- Optional XL enhancement assets were missing and audio was disabled. No gameplay
  route was exercised. This is startup evidence only, not full presentation parity

### Developer stock-loader probe

The isolated `xl_probe_fixture` tool creates a deliberately unverified gameplay
fixture, requiring `--incomplete-presentation-probe` on its command line. This is
not production conversion or supported mission admission. Embedded colors remain
in the source report but are not applied by stock rendering

The unchanged Windows D2 headless metadata executable loaded the fixture and
reported 228 segments, 86 walls, 13 triggers, 110 objects, 38 robots, four hostages,
55 powerups and one reactor. The isolated `xl_stock_loader_probe` CTest reproduces
this check and verifies the original source remains unchanged. Synthetic and
stock-loader CTests passed after the probe was added

These counts prove only a bounded loader milestone. Independent value comparison,
asset validation, trigger/progression/save tests and faithful lighting remain open.
No base engine files have been modified and no mission has been marked supported

### Stock field audit findings

An opt-in separate D2 build using `XL_compat/stock_audit/configure.cmake` now
compares 22,295 loaded values against the original XL inventory. Geometry,
textures/UVLs, wall fields and trigger values/targets agree except for four
ambient flags: segments 51, 81, 82 and 111 change from 0 to 2. The audit exits
nonzero and preserves these differences as unresolved evidence

Stock `gameseg.c::set_ambient_sound_flags` recomputes ambient sound reachability;
the pinned XL source uses `main/findsegment.cpp::SetAmbientSoundFlags`, with
XL-specific passability checks. Source similarity alone does not establish that
the same segments are audible in the reference runtime. Do not waive the audit
failure without that comparison. Wall controlling-trigger reconstruction, object
normalization and color rendering remain outside this audit's coverage

The audit was then expanded to object identity, positions/orientation, size and
shields, contained items, physics/spin, AI, pickup counts and render records. It
now compares 29,120 values, with six mismatches: the four ambient flags above and
two player-start object 4 orientation components changed by one fixed-point unit.
Stock loading calls `check_and_fix_matrix` for players; XL calls its own matrix
normalizer. Keep the mismatch open until the reference values are established

All 225 checked direct object dependencies resolve against the retail tables.
This does not prove complete transitive asset coverage. A negative source with
robot ID 255 was rejected using its original ID, before a loader substitution
could be accepted. Physics, AI and rendering-record agreement is static loaded
state evidence, not a test of combat or dynamic rendering

### Actual draw-path checkpoint

The isolated Windows preview now has an explicit `-xl-preview-start` mode that
starts the private pilot fixture with ordinary enemies and no route assists.
The generated startup and graphics overlays remain under the dedicated build
directory; no `d1/` or `d2/` files are edited. The capture runs after 30 swaps
with successful calls to the XL vertex-color hook

The first post-event/front-buffer capture was entirely black and is rejected
as visual evidence. Capturing the back buffer immediately before swap produced
the starting room, cockpit and textured geometry at 640x480:
`temp/xl-runtime-engine/main/screenshots/scrn0001.png`, SHA-256
`08c92ff4151347bd391ee0b7efe21461ec2badd82cdf9b34822ef67adb4b66ad`.
It was visually inspected. This is not a matched reference-camera comparison,
and does not verify dynamic lighting, special face effects, combat or progression.
The pilot remains unsupported in the compatibility report. The four lighting,
inventory and real-loader lifecycle tests pass

Windows desktop automation did not discover the CLI-launched window; the helper's
own launch attempt timed out on app approval. Reproducible native startup and
engine screenshot capture allowed this check without relying on UI interaction

### Assisted route and checkpoint diagnostic

The fresh stock route build completed the translated pilot's red-key, reactor and
exit objectives in 1,739 simulated frames. Two fresh JSON reports were identical.
A separate red-key run saved a checkpoint at frame 713; another process restored
it with key flags 4 and completed reactor/exit objectives in 1,005 frames.
`xl_route_checkpoint_probe` reproduces this under the isolated CTest project

This is explicitly assisted diagnostic coverage: ordinary robots are removed,
the player is invulnerable, projectiles have boosted damage, the countdown is
paused and the diagnostic actor's position is committed for checkpoint saving.
The generated coverage report retains `playable: false`. None of the combat,
every-trigger/branch, ordinary playthrough, native-transition, presentation or XL
reference acceptance gates is marked complete by this result

## Survey reconciliation and revised execution order (2026-10-23)

See [evidence audit](../../XL_compat/evidence/README.md) and the revised
[extension catalog](../../XL_compat/EXTENSIONS.md). All supplied manifest hashes
match; the 131 distinct payloads/56 HOGs and principal feature counts were
recomputed from the supplied records. The original corpus was not rescanned.
The survey's pilot has exactly our source hash and agrees with native counts,
offsets, wall/cloak histograms and chromatic colors. Its wider v11-v23 coverage
is research evidence, not an extension of our native 15/33 parser

Ahayweh Gate remains the first pilot. The Apocalyptic Factor XL is a later
candidate family, not a replacement or a shortcut to completion. Do not require
unrelated teleports/generators to be ported before the pilot; do require evidence
that the pilot does not need each omitted feature. Static field agreement alone
does not establish behavior, even for classic-looking trigger types

Next acceptance work, in order:

1. Reconcile source/reference profiles, descriptor precedence and resource closure.
   Retain the six loader mismatches until reference values explain them; no waivers
2. Verify changing lights and material behavior together. Specifically inspect
   pilot walls 6-17 (type 3, cloak 21) and 81-82 (type 3, cloak 31), in segments
   99-105, 84 and 226. Compare visibility, targeting, collision, projectile passage
   and activation. These fields may be classic or XL-authored semantics; their
   presence does not prove either a new feature or equivalence. Include the
   existing 70 light-index records and 3960 delta-light records in on/off,
   flicker, shot-light, reactor-flash and restore checks
3. Match camera, assets and actual runtime lighting settings in the reference and
   preview. The real starting-room capture proves drawing only. Integrate dynamic
   light generation, face colors and state transfer before claiming fidelity
4. Run normal combat and every trigger/branch, red key, reactor countdown and exit
   without route assists. Save/reload before and after state changes; verify robot,
   wall, trigger and lighting state. Complete production loading, not just fixture
   translation. Neither grayscale nor assisted completion satisfies the user goal
5. Exercise pilot -> stock -> pilot and, when a second fixture exists, custom A ->
   stock -> custom B. Verify failed-load rollback and audio/model/texture/light
   cleanup. Save/replay/network representations must preserve XL state; record a
   single-player scope explicitly while multiplayer remains unverified. Do not
   admit XL multiplayer based on single-player tests

Broader backlog after the pilot: versioned parsing and widened references;
finite event interpreter/scheduler (masters, enable/disable, delayed/repeating,
object damage/destruction); teleports/boosts and route edges; old/new segment
properties; equipment generators; authored materials and effect/audio lifecycles;
mission resource precedence. Each subsystem carries persistence, replay and
multiplayer-consistency gates, rather than adding them at the end. Gameplay data
and cues move ahead of presentation work whenever progression depends on them

Smaller fixture progression: Apocalyptic Factor/D2-XL additional levels;
Lunar Outpost Reloaded/Speed! movement; Glass House/Crownpoint X materials;
Sphere/Boiling Point event/environment/generator slices; finally Monolevel and
Anthology capacity stress. Resolve Subzero/playgdxl object-parser uncertainty
before claiming complete inventories. Modern official 2.0 hubs and non-cubic
geometry require a separate scope/specification decision

Current-state clarification: earlier 'no renderer hook' and 'images pending'
paragraphs above describe historical checkpoints. The isolated preview now draws
and has a verified image artifact; dynamic/material fidelity, normal gameplay and
production admission remain open. The survey changes priorities, not support status

## Pilot wall-material implementation checkpoint (2026-10-23)

Source comparison found a concrete gap: XL `CWall::IsPassable` considers authored
cloak values when deciding render-through, and `WallAlpha` applies textured
opacity to ordinary XL walls. Stock Redux does neither for these pilot illusion
walls. XL `palette.h` sets FADE_LEVELS to 31, unlike stock GR_FADE_LEVELS=34.
Thus cloak 21 means alpha 10/31; cloak 31 means full opacity on this ordinary
textured path. It must not be mapped to stock cloaking fades

The isolated runtime now wraps doorway classification and textured face alpha,
using current live wall state. A checked generated OpenGL overlay supplies the
alpha to the three textured draw paths; per-face state clears after drawing and
on every load. It does not modify `d1/` or `d2/`. The wrapper supplements the
classic subset needed here; it is not a complete implementation of XL colored
walls, player/key-specific passage, camera surfaces or all door-state semantics

The real-loader lifecycle test passes with the added pilot assertions: twelve
cloak-21 walls use 10/31 alpha and retain fly/render-through flags; two cloak-31
walls remain opaque; per-face and stock-level state isolation holds. Both Windows
preview targets build. This proves the tested numeric/flag paths, not full visual,
collision or projectile equivalence

`-xl-preview-wall 6` places the diagnostic camera at the wall's segment center,
facing the side. It is explicitly repositioned visual evidence, not gameplay.
The default material capture is `temp/xl-runtime-engine/main/screenshots/scrn0002.png`
(SHA-256 f8921e835884735ae771e6e3fcc53448067280979929797ac02cb68ed205e9ae).
`-xl-preview-stock-materials` disables both material adapters for investigation;
its capture is `scrn0003.png`
(SHA-256 10c3d22f8e300ab33a94eda33c0bbd19424078c097a0109056b60b2be3172b47).
Both images were inspected. Animation/simulation timing differs, so their pixel
difference is not a clean material-only metric. A matched-time XL reference scene
and deterministic capture are still required. Both diagnostic processes stopped

Remaining priorities: independent material/reference capture, changing-light
integration, normal combat/progression, saved-state fidelity and production
admission. Compatibility remains unsupported

## Changing-light integration checkpoint (2026-10-23)

The preview now passes live reactor/seismic state and the engine flash scale into
XL vertex shading; previously its replacement RGB discarded those inputs. The
reactor zero-scale case follows XL FlashFrame's minimum one fixed-point unit.
This wires state into the arithmetic already compared against pinned XL source;
it does not establish phase/timing parity, automap behavior or dynamic-source
color generation. Stock and XL FlashFrame have additional lifecycle differences
that still need runtime comparison

Extended the real-loader test to exercise every one of the pilot's 70 light
index records through stock subtract_light/add_light. Expected corner values are
computed from the original source deltas using pinned XL light.cpp ChangeLight's
legacy-mode formula: ordered +/- byte * 2048, clamped at zero after each update.
All 70 sources change UVLs and pass exact comparisons, including repeated off/on
no-op checks. Feeding the changed live UVLs to the XL hook changes 5878 tested
colored corners. Add-back uses the post-clamp state; the test does not falsely
assume subtraction/addition must recover the initial values after saturation

The reactor/seismic test observes normal RGB 32426/32767/32305 and half-flash
16213/16383/16152 on its selected pilot vertex. A one-unit fixed truncation is
allowed at the expected 16384 peak. Both preview targets build; the lifecycle,
analytical lighting and pinned-source arithmetic tests pass. Logs are in
`temp/xl-compat-ninja/preview-tests/cb780d3f7acf/preview.log`

Coverage limits: scalar UVLs and direct color-hook output, not a matched rendered
on/off scene; not runtime reference light-manager behavior, object illumination,
dynamic colored-light generation, save/restore, or normal combat/progression.
All production compatibility gates remain open. This supersedes the earlier
statement that the preview does not consume reactor/seismic flash state

## Dynamic-light accumulation checkpoint (2026-10-23)

XL legacy lighting does not store the same state as Redux's final RGB light sum.
Its ApplyLight accumulates scalar fixed-point intensity separately; SetDynColor
keeps a color-present flag and blends successive source colors by pairwise average.
Unforced white colors are skipped; forced white colors participate. This is
order-dependent and not recoverable by averaging or normalizing Redux RGB output

Added `DynamicVertexLight` in the isolated lighting module with checked scalar
accumulation and the XL color rule. This receives an already-attenuated contribution;
it is not an emitter-intensity or distance calculation. A new optional
`XL_REFERENCE_LIGHT` source oracle extracts unchanged InitDynColoring/SetDynColor
from hash-pinned `lighting/light.cpp`:
`ca95b9183729c7f3665ef0706206afce093b53b0d97aa6ba11379dd5fdf5f36f`.
The test substitutes data containers, not color code, and compares 12288 steps
across fresh accumulation sequences, source colors, force flags and missing colors.
It passes on Windows x64 and Android x86_64 emulator-5556. Existing analytical and
pinned vertex-lighting tests also pass

The new state is not yet fed by the live emitter loop. Do not mark dynamic lights
implemented or turn the preview's static output into a supported fallback. Next
port the legacy ApplyLight path and its per-frame reset/visible-vertex selection,
then feed the separate scalar/color state plus original vertex brightness into
shade_vertex. Emission and attenuation must be ported from pinned XL rather than
inferred from stock final RGB

Specific source differences to preserve during that port:

- XL uses CFixVector::Dist where stock uses quick distance estimates
- XL robot emission uses lightcast as a gate and lighting as intensity; stock uses
  lightcast itself as intensity
- XL player emission uses thrust magnitude, unlike stock velocity-smoothed thrust
- Weapon colors use XL's weapon color table; missiles use 1/0.3/0; flare emission
  uses random intensity rather than stock's time/XOR pattern
- Fireball colors average resolved animation-frame colors and normalize by their
  maximum channel; resource overrides therefore matter to lighting
- Powerup color/enable rules, dim-source segment-local attenuation, source order,
  muzzle flashes and headlight cones must remain explicit

These findings change the next implementation action; they do not certify a
reference runtime, renderer, combat sequence or save. Dynamic generation and the
pilot's existing gameplay/production-admission gates remain open

## Partial live dynamic-light checkpoint (2026-10-23)

The isolated render overlay now feeds robot, debris and explicit OBJ_LIGHT
sources into separate scalar/color state each frame, then into XL vertex shading
with original vertex brightness. It selects visible vertices for ordinary point
sources and segment-local squared attenuation for dim sources. Both Windows
preview targets build. The real-loader lifecycle test passes for an explicit
point light reaching shading, state clearing on the next frame, and a weapon
source being reported unresolved. This supersedes the previous checkpoint's
statement that the accumulator has no live emitter connection

This is a partial port, not dynamic-light qualification. Player, weapon, fireball
and marker emitters are counted as unresolved. Muzzle flashes and headlights
are not covered by that counter. Powerup lighting follows the saved reference
profile's disabled setting; running-reference confirmation is still required.
Distance math mode, attenuation boundaries, object illumination and matched
frames remain unverified. Stock lighting still executes for stock consumers;
avoid duplicate random-number draws and lifetime changes when porting emitters

Before implementing player emission, preserve XL's distinction between local
pre-scale player thrust and remote object thrust. Stock final physics thrust is
not equivalent. Trace the raw controls, XL speed/damage multipliers and frame
reset together, then test the adapter against pinned source. Weapon/fireball
colors require resolved resources, so keep resource closure ahead of any claim
that combat lighting is faithful. Follow with matched normal-play scenes and
save/restore checks; none of the earlier gameplay gates is waived

Rechecked the user-supplied survey ZIP using the repository verifier and the
original pilot bytes. Manifest/CSV/JSON checks and selected native pilot fields
pass; the report remains playable:false. EXTENSIONS.md now states an explicit
evidence ladder from inventory through experimental loading, arithmetic, assisted
routing and per-level qualification, preventing any one checkpoint from silently
becoming a broader support claim. No full corpus rescan or new reference
playthrough is implied by this reconciliation

## Weapon and marker emission checkpoint (2026-10-23)

Ported ordinary weapon emission into the isolated frame loop: resolved weapon
intensity, forced source color, and the pinned CObject::InitTables missile
classification including robot missiles. CWeaponData::Create initializes weapon
colors to white; the inspected consumers do not assign a replacement table.
Missiles use 1/0.3/0. Flare randomness and multiplayer omega suppression remain
explicit unresolved cases rather than silently borrowing stock behavior

Markers now use XL's 8 * abs(0.5 - fractional lifetime) intensity, 0.1/1/0.1
color and segment-local squared attenuation regardless of intensity. Stock
compute_light_emission already adds one second to their lifetime; that preserves
the fractional bits used for emission. The XL hook runs afterward and does not
repeat the mutation. This dependency must be preserved or replaced explicitly
if the stock lighting loop is removed later

The real-loader integration checks orange player/robot missiles versus white
laser/mine sources, forced-white color mixing, strict point-source radius,
visible-vertex selection, marker pulse, single lifetime mutation ownership and
state clearing when dynamics are disabled. The checks use controlled weapon
intensities, not proof of retail weapon-table or complete resource parity

Next emission work remains local player thrust, headlight cones, flares, fireballs
and muzzle flashes. A specific fireball resource distinction is now confirmed:
XL CBitmap::GetAvgColor uses AvgColorIndex's nearest palette entry for indexed
frames, then averages resolved animation frames and normalizes their maximum.
Redux's avg_color_rgb is not equivalent. Decode/resolve the actual animation
frames and compare this path before claiming explosion-light fidelity. No
normal-combat, reference-image or production-admission gate is complete

## Indexed explosion emission checkpoint (2026-10-23)

Implemented indexed fireball frame resolution in the isolated preview. Source
inspection of XL gameio/piggy.cpp confirms that nonzero average palette indices
come from bitmap headers; GetAvgColor uses those before calculating a fallback.
The fallback ignores pixel indices 0/254/255, averages palette channels with
integer division, and selects the nearest palette entry among 0..254 with the
first equal-distance entry retained. Stock's closest-color helper excludes 254,
so it cannot be substituted. RLE and RLE_BIG frames are decoded by row through
the stock scanline decoder. Colors are averaged across the actual animation
frame IDs, normalized by maximum channel, and forced into the dynamic color
accumulator. Lifetime attenuation follows the pinned ComputeLightIntensity path

Controlled real-loader checks cover stored-index precedence over pixels,
noncontiguous frame IDs, raw and RLE_BIG fallback, index 254 selection, and
full/half intensity. Three retail clips (small explosion, player hit, volatile
wall hit) also exercise resource resolution after restoring the real palette
and bitmap tables. These tests do not prove exact reference resource resolution
or visual parity. No persistent bitmap-color cache is introduced, avoiding stale
palette/override state across loads while correctness is being established

This path only handles indexed GameBitmaps; true-color/OOF/TGA overrides and
XL-only explosion render modes remain outside its evidence. Player/headlight,
flare and muzzle-flash emission remain incomplete. Keep resource closure,
reference runtime comparisons and normal combat/progression gates open

## Muzzle-flash integration checkpoint (2026-10-23)

The dedicated build now generates a lighting.c overlay with a checked unique
timestamp hook inside cast_muzzle_flash_light. It snapshots queue contributions
before stock expiration, using exactly that loop's timestamp, then applies them
before object lights in the XL accumulator. Flash sources use visible vertices,
exact distance and minimum-distance clamping, with no source color. The stock
queue remains the sole owner of expiration; the adapter only reads it and clears
its private snapshot on load. No base d1/d2 files were edited

Pinned XL CastMuzzleFlashLight and stock share the one-third-second duration,
integer scale and signed 16-bit elapsed narrowing. Preserve that behavior,
including its historical wrap, rather than silently improving one engine during
a fidelity comparison. A runtime/reference timing comparison is still required

Real-loader tests exercise zero-age, half-age, last-live-tick and expired
contributions through colored shading, check that snapshotting does not mutate
the queue, and run stock set_dynamic_light to verify actual expiration reaches
the XL hook. Player thrust, headlights and flare randomness remain unfinished;
normal-play, resource, image, save/replay and production-admission gates stay open

## Local-player thrust light checkpoint (2026-10-23)

Added checked generated controls.c/object.c overlays for raw thrust capture and
player-reset clearing. The preview uses max(magnitude(raw thrust)/4, 2) + 0.5,
with unforced white color, for the normal single-player ship. Load and death
clear the vector. Headlight and multiplayer/hoard branches remain explicitly
unresolved rather than using this formula for those modes

The saved reference profile selects ship 0 and nDamageModel=0. XL's shipModifiers
entry has speed 1, and SubSystemDamage returns 0.5 with damage disabled, making
ApplyFlightControls' 2*DriveDamage multiplier one. This supports direct raw-vector
capture for that profile, not general alternate ship or damage-model support

Correction to the earlier investigation: updateobject.cpp's playerThrust zeroing
is in StopPlayerMovement, not a per-frame reset. Do not clear this state merely
because another render frame occurs. The adapter resets on lifecycle/control
events. Reference pause, guided-missile, respawn and save behavior still require
comparison; this checkpoint does not certify those paths

The real-loader test invokes read_flying_controls with controlled input and ship
thrust. It checks that final thrust doubles while the light uses the original
vector (expected peak 28672, one-unit truncation tolerance), then checks death
clearing reaches the 20480 baseline at the selected distance. Both preview
targets build and the lifecycle test passes. Player reset overlay compilation
does not itself prove respawn/save parity. Headlight and flare implementation,
runtime profile confirmation and all ordinary gameplay gates remain open

## Local headlight cone checkpoint (2026-10-23)

Implemented the legacy single-player, non-darkness headlight branch from pinned
ApplyLight. Its intensity is 10 fixed units; the radius test shifts distance by
three; source color is forced white. Normalized forward dot below 0.5 uses
intensity/(10*clamped distance); the cone uses fixed dot squared * intensity / 8.
No multiplayer maxHeadlightDist restriction is applied to this single-player
path. Multiplayer remains unresolved rather than borrowing local behavior

The saved reference profile has headlight available, no loadout device, darkness
off and lighting method zero. HeadlightIsOn therefore requires both possession
and on bits. Tests exercise front/back/side, a 3-4-5 angled direction, a point
beyond ordinary point-light range, the exclusive shifted radius, and each flag
missing independently. Both preview builds and lifecycle checks pass

This is face-light generation, not proof of reference rendering or object
illumination. Darkness cone sizes, alternate lighting modes, remote obstruction
tests and headlight energy-drain parity are not covered. Flare emission remains
the immediate missing single-player emitter; normal gameplay and all original
qualification gates remain open

## Flare formula and RNG isolation checkpoint (2026-10-23)

Inspection of pinned include/maths.h revealed that RandFloat(scale) divides by
scale, rather than multiplying. Therefore ComputeLightIntensity's flare factor
is 1.75..2.0. Added checked flare_intensity arithmetic to the standalone module
and connected it to one 15-bit Redux FX sample per evaluated flare. Do not use
the apparent but incorrect 1.75..5.75 range or stock's time/XOR flicker

The analytical tests cover multiplier endpoints, lifetime capping and zero life.
The real-loader test seeds the FX stream, verifies live color/intensity against
the sampled result and checks exactly one FX call with unchanged simulation RNG
state and call count. It restores FX state/count afterward. Multiplayer omega
remains an explicit unresolved emitter; it replaces flare in that negative test

Using Redux's FX stream is intentional integration, not a claim of exact XL libc
random-sequence parity. XL's platform RAND_MAX and shared global random consumers
can differ. Capture comparisons must control the random samples or assess the
specified flicker range/behavior, and ordinary combat/replay must confirm that
rendering does not perturb simulation. The emitter loop now has implementations
for the investigated normal single-player cases, but object illumination,
resource closure, reference profiles/scenes, normal gameplay, save/replay and
production admission still require evidence. A zero unresolved count certifies
none of those gates
