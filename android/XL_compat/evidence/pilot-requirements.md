# Original Ahayweh Gate requirement reconciliation

Scope: original `d2leva-1.rl2`, SHA-256
`5e835e76e984c658531175640294a4b05f6127904884ba7acfd65c93165be5b2`,
singleplayer, through its native exit. This is the user's selected level pilot,
not the entire D2-XL campaign. Exact images and incidental classic gameplay
retuning are excluded by the user's clarification. Required authored extensions
are not excluded. This document reconciles the 19 categories in `EXTENSIONS.md`
against the decoded bytes, resource container and current implementation

## Required categories and evidence

| Category | Requirement in these bytes | Implementation and verification |
| --- | --- | --- |
| Container | Standard DHF HOG and ordinary mission descriptor | Original HOG/descriptor hashes pinned; original ZIP import and extracted-byte comparison pass |
| Serialization | Level 15 / game data 33, including wide wall indices and embedded colors | Bounded native parser and translation preserve required fields; exact-hash original admission; rejection, lifecycle and stock-decoding checks |
| Capacity | 828 vertices, 228 segments, 110 objects, 86 walls, 13 wall triggers | Within native capacities; indexed references and section bounds checked; no table widening required |
| Segments | Functions 0, 1, 3, 5, 6; neutral owner/group; only ambient-lava flag | Classic ordinary/fuel/reactor/team-goal functions; no XL movement/environment property. Team goals are inactive in this singleplayer scope. Ambient discrepancy classified separately |
| Walls | Blastable, door, illusion, open, closed and overlay walls; classic flag bits | Real-loader values, all 13 native trigger actions, target persistence, physical linked-portal traversal; authored opacity has separate verification below |
| Triggers | Five open-door, seven open-wall, one exit; flags zero | All actions activated through normal inputs; target state and earned-resource saves restored; native exit and Android score/menu completion verified |
| Object triggers | Zero records | Decoded absence; no object-trigger interpreter required for this level |
| Objects | Robots, hostages, player/coop starts, powerups and reactor | No XL effect or other extended object type. Ordinary live combat, pickups, destruction and progression exercised; classic AI/weapon retuning is outside the requested fidelity requirement |
| Gameplay assets | Retail robot/weapon/powerup/model definitions | HOG has no HAM/HXM/PMH. Native transitive graph resolves 5 robot definitions, 33 weapons, 17 powerups, 55 models, 56 vclips, 780 bitmaps and 51 sound samples; negative dependency case rejects invalid IDs |
| Baked lighting | 828 vertex, 1368 side and 910 texture color records | Vertex arithmetic compared with pinned reference functions; side-registration consumer audit; texture colors consumed by live coronas, including all 24 chromatic candidates |
| Dynamic lighting | Classic deltas: 70 indices / 3960 records; no authored flicker records | Native light subtraction/restoration and UVL state checks; authored emitter suppression/recovery; Android save/restore. Legacy dynamic-source calculations have additional scoped tests |
| Visual assets | Retail textures/models, classic archive palettes/fonts/images | Native dependency paging and Android rendered checks. No level-1 POG or XL model/image payload; the HOG's only POG belongs to `d2levd-s`, outside this level |
| Mission flow | One level, reactor/countdown and ordinary exit | Earned red key, reactor fight and timed escape with normal controls; Android native score and menu return. Subsequent levels are deliberately outside admission |
| Runtime | Playable progression, material behavior, persistence and stock isolation | Normal-input route/branch/secret/crossing cases, original import, Android extension persistence and completion. See acceptance audit for exact test scopes and run IDs |
| Navigation | Ordinary connected geometry only | No teleport, speed-boost or spawn-setting action; all linked-target portals and nine secret-area capture segments have physical entry evidence |
| Generators | No robot matcens or equipment generators | Decoded absence; no extended generator dependency |
| Wall materials | 14 nonzero authored opacity records | Reference opacity interpretation, native visibility and projectile-object queries with solid controls, Android rendering/persistence. No additive flag or type-8 colored wall |
| Effect objects | No particles, lightning or ambient-sound objects | Complete object record inventory has no such payload; no required effect-object extension is omitted |
| Resource scope | Original mission assets plus retail data | Hash-verified original import; HOG contains no pilot custom definitions or level-specific override. Pilot -> retail -> pilot restores definition tables and extension state. Other campaigns' override behavior is unqualified |

Inventory diagnostic strings deliberately remain conservative: they describe
the standalone inspector's capabilities, not a runtime qualification inferred
from a title. The production browser still reports the campaign unsupported

## Object illumination and the selected rendering profile

The reference profile has `render.nLightingMethod=0`, color level 2 and powerup
lights disabled. Authored face colors and texture coronas remain required in
this profile. Alternate per-pixel/lightmap/shadow modes are not claimed

Pinned source `11038eb53dd86f3bcdc027dddd3f93c60f02536e` establishes the object
consumer chain:

- `render/objrender.cpp:1305` clears the per-object color index before rendering
- `lighting/light.cpp:737` obtains an RGB segment color for non-polygon objects
  only with a nonzero lighting method; otherwise it uses average segment light
- `include/dynlight.h:342` makes `SHOW_DYN_LIGHT` false for method zero
- `3d/fastmodels.cpp:119` therefore selects the classic model-light path; its
  fallback color is white when no per-object color index was set
- `lighting/light.cpp:769` adds classic dynamic segment intensity to object light

Redux retains its ordinary object-light calculation. Its color/intensity details
need not duplicate XL's incidental classic lighting choices to implement this
profile's authored extensions. This is a source-based consumer classification,
not proof of pixel equality or every optional XL rendering mode. Porting unrelated
object-light tuning is not a remaining requirement merely because an older note
called object illumination unverified

## Explained differences and evidence limits

The reference's two missing ambient-lava regions are reproduced by its texture
paging state and a native opaque-boundary control; see `pilot-ambient-flags.json`.
Redux retains saved PIG transparency flags while paged out. The two one-unit
player-start orientation normalization differences are classic matrix rounding,
not lost extension data; gameplay tests use the actual loaded orientation

Native controlled diagnostics are not unassisted playthroughs. Gameplay tests
use normal controls but read topology to choose their inputs; independent earned
checkpoint routes do not establish a single uninterrupted all-secrets run.
Android completion starts from an earned exit approach. These limits remain
visible and do not turn into claims of Android start-to-exit or multiplayer parity

The [acceptance audit](pilot-acceptance-audit.md) holds the final passing runs,
APK identity and bounded pilot acceptance. This checklist supplies the feature
scope; qualification additionally depends on those runtime results
