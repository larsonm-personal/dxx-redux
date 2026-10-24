# D2X-XL first mission pilot

## Decision

Completion scope clarified by the user on 2026-10-23: a fully playable, verified
Ahayweh Gate port, including gameplay and lighting. The inventory, assisted route
and compatibility browser are intermediate deliverables only. Do not close the
goal when the browser is finished

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
   - Establish expected behavior from a pinned D2X-XL reference run of these exact
     bytes; stock Counterstrike is a secondary control, not the XL behavior oracle
   - Exercise every trigger and target, keyed doors, wall changes, robot combat,
     pickups, reactor/countdown and normal exit through gameplay actions
   - Check all reachable branches and secrets, not just a route to the exit
   - Save/reload before and after representative state changes and verify persistence
   - Use introspection assertions and reusable automation; warping to the exit or
     directly setting completion state cannot prove playability
   - Verify leaving the mission restores stock assets and behavior
4. **Verify presentation separately**
   - Account for embedded colors, palette, textures, briefing, music and sounds
   - Compare reference views/audio at selected checkpoints. A grayscale rendering
     with correct geometry is a presentation gap, not faithful XL support
   - Explicitly label any gameplay-only experimental milestone with remaining
     presentation gaps; do not call it full support
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
