# XL level expansion beyond Ahayweh Gate

## Isolation and review

Worktree branch `xl-level-expansion`, based on published PR120 commit
`92aca5067a55e83fbd132431e60dca37ab825d56`. Do not publish to `d2xxl_compat`.
The active Counterstrike controller is outside this change. Belial runtime
campaigns remain excluded. Assets remain private and unmodified.

PR120 records repeatable normal-input key/reactor/escape and eight passing branch
checks; branches 3-6 remain failing in the latest published evidence. This is not
full pilot qualification. Latest Linux XL CI fails GCC dangling-reference warnings
in inventory.cpp; Windows passes. Local GCC 14 reproduces this build failure.
The returned section reference belongs to Inventory, not the temporary name;
use literal names without constructing temporary strings and named payload arrays
without disabling warnings. No engine gameplay failure follows from that CI result.

## Ordered expansion gates

| Order | Exact level                                                              | Baseline and dependencies                                                                                                                  | Completion criterion                                                                                                                                               |
| ----- | ------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| 1     | Counterstrike XL `d2leva-1.rl2`                                          | Existing pilot; timed exit passes; four branch checks still fail                                                                           | Active agent proves all branch activations, physical traversal and save persistence                                                                                |
| 2     | Robotech Arena `rtarena.rl2`                                             | 15/33; 188 segments; one old-type-7 water segment; HXM/POG closure must be checked; original reader rejects legacy child flag at segment 6 | Original archive loads with original resources; arena movement, water entry/exit, collision, saves, deterministic replay and stock cleanup; no reactor/exit exists |
| 3     | Counterstrike XL `d2leva-2.rl2`, then `d2leva-4.rl2` and `d2leva-s.rl2`  | Same serialization family; runtime untested here; generators, boss and secret return introduce progression gates                           | Ordinary completion, authored switch dependencies, checkpoint reload, secret return and next-level identity                                                        |
| 4     | Apocalyptic Factor XL `fbasin.rl2`                                       | 15/34 rather than current 15/33; classic trigger types alone do not prove compatibility; resources and opacity 63 unresolved               | Version-specific decode, resource closure, ordinary key/reactor/exit plus all branch targets                                                                       |
| 5     | Neptune XL `neptunxl.rl2`                                                | Smallest corpus geometry (93 segments), but 23/40 layout; static scan shows no new semantic feature                                        | Bounded version fixtures and exact endpoints, original mission startup and normal exit                                                                             |
| 6     | Observatory XL `obsrv-xl.rl2`                                            | 17/36; 26 no-damage segments, descriptor dialect and opacity                                                                               | Version/resource gates first, damage boundaries and save/replay parity                                                                                             |
| 7     | Crownpoint X `crownx.rl2`                                                | 11/31, 58 colored walls plus two camera triggers                                                                                           | Separate layout, wall and camera milestones before level qualification                                                                                             |
| 8     | Glass House `g-house.rl2`, Lunar Outpost Reloaded, Sphere, Boiling Point | Multiple independent blockers: layouts, events, transport, properties and object effects                                                   | Decompose features using smaller probes first; unresolved object-section scans remain unknown                                                                      |
| 9     | D.I: Monolevel                                                           | Capacity and combined stress, not a first feature probe                                                                                    | Widening boundary fixtures, resource limits, saves/network and whole-level runtime stress                                                                          |

## Candidate decision

Glass House and Crownpoint are rejected as single-feature candidates. Robotech
Arena is the provisional water candidate. Static absence is not runtime evidence.
The exact retained assets checkout is `d9fbeaf6eb059fd4504308bfd6364734775b7cba`.
Extract only `missions/spotlight/rtarena.*` from `levelpack.7z` for this experiment.
Original level SHA-256 is
`5d456eb26f603d942e565a7944c3837741613dccbed6573323bcd56874133392`.
First establish complete native inventory and custom-resource closure. Do not
claim one blocker until those checks pass. A legacy ignored child-mask bit is
format normalization, not proof of another gameplay feature.

Reference source: `arbruijn/d2x-xl@2576a98b5de310e3ac7c9777df2677d1627b8bae`.
Its old-type mapping identifies 7 as water; do not interpret it as the new split
function enum. Its fluid profile scales translation by 75 percent, adds water
rotation under thrust, and uses water contact behavior without shield damage.
Exact historical profile and presentation qualification remain to be established.

## Implementation and private evidence

Selected Robotech Arena after the native inventory, original archive load and
transitive resource check succeeded. Added only the opt-in old-type-7 water slice:
75-percent translation time, matching collision velocity reconstruction,
simulation-RNG turbulence under thrust, water contact classification without
fluid shield damage, and the existing ship-in-water sound path. Texture-based
classic water/lava behavior remains with the stock engine. Fluid properties are
immutable source data reloaded with the original mission on save restore; no new
save payload, scheduler or network state is added. Multiplayer admission is refused.

`-xl-water-arena` admits only mission `rtarena`, level `rtarena.rl2`, exact 80598
bytes and the audited SHA-256. It never marks the Counterstrike pilot active.
`-xl-preview-start -xl-water-arena` starts this developer arena with ordinary
inputs and original mission identity. Production browser support remains blocked.
The translation's stock segment enum is neutral; the XL source retains water.
The default diagnostic translator still refuses water unless explicitly enabled
by its caller. Lava, other segment functions and unknown flags remain rejected.

The private `xl_water_arena` CTest pins the original HOG and descriptor hashes,
stages copies in the build directory and checks input hashes again afterward.
Configure `XL_ARENA_HOG`, `XL_ARENA_DESCRIPTOR`, `XL_PREVIEW_LIFECYCLE` and
`XL_RETAIL_DATA`. No private bytes or runtime save files are committed.

Observed original-resource graph after review: 37 models, 603 paged bitmaps and 28 sound
samples; zero compatibility warnings. The HXM replaces model 108 with a 17892-byte
payload, distinct from stock. The graph includes placed pickups, player arsenal,
models, upgraded laser levels, flare, wall clips and every authored side texture/effect dependency. This establishes
loaded resource closure, not audible playback or pixel-level override fidelity.

Actual native physics checks require exactly 12288 fixed units of travel for
one unit/second over a quarter second in water, without scaling stored free-flight
velocity. They check invalid segment indices, overflow-safe scaling, idle RNG
isolation, two simulation samples under thrust and no fluid shield damage.
The subsequent ordinary-control run restarts the original mission to discard all
controlled-test state, then reaches segment 187 in 1235 frames and leaves for
segment 146 in 90 frames, collecting pickups through normal game processing.
This arena has no reactor or exit; no campaign completion is claimed.

An earned native water checkpoint restores mission, position and water property.
Replaying the exit agrees exactly on position, orientation, velocity, shields,
energy, game time and simulation RNG state, with the same number of RNG draws.
Native restore intentionally resets the diagnostic SIM call counter
(`d2/main/state.c`); the test compares draw deltas rather than treating that reset
as a simulation divergence. No RNG or physical state is overwritten to force
agreement. Arena -> stock -> arena restores model payload and gameplay tables,
clears water in stock and reactivates it on return.

Fresh-directory testing found two harness issues, corrected only in the arena
check: initialize the normal player defaults before pickups, and acquire the
caller-owned pause before `state_save_all_sub` consumes it. Existing Counterstrike
harness/controller behavior was not changed. Linux pilot compilation also needed
C++ headers before legacy min/max macros and undefining those macros at that
boundary. Assertions remain enabled in all local Debug validation.

Remaining qualification: Android, interactive rendering/audio, alternate XL
profiles and fog modes, multiplayer and campaigns are not established. The
historical level uses the old combined segment enum; the pinned source provides
its explicit type mapping and fluid-enabled reference behavior. Do not claim
modern XL2.0 parity or general water-level admission. Headless teardown prints
existing allocation diagnostics; the process succeeds, but this is not a leak
qualification. Private test logs and saves stay local under the build tree.

## Validation checkpoint

- Linux GCC 14 Debug: both opt-in runtime executables build
- Standalone XL CTest: 3/3 pass, including private original-arena integration
- Default XL-disabled D2 full build and CTest: 98/98 pass
- Survey evidence audit passes; scoped formatter/linter passes
- Changed-source negative test resolves a private one-bit mutation explicitly,
  requires hash rejection and cleared water state, then recovers the original
- No original mission assets changed; no private bytes are staged for publication

Published-base CI reviewed separately: Public LAN smoke succeeds; Windows XL
contracts succeed. Linux XL contracts fail on the inventory warning corrected
here. Linux tooling also has a pre-existing unexpected-success test verdict;
this is unrelated to water and not classified as an engine gameplay failure.

## Review follow-up

Original arena objects are four player starts and 35 powerups. All 18 wall clip
indices are -1. Segment 187 sides 0, 2, 3, 4 and 5 have no walls and use texture
266 (non-water, zero damage); side 1 connects to segment 146 through illusion
wall 17, texture 399 (native water), without overlays. The ordinary-illusion
segment-water fallback is not exercised by this authored portal, but the native
waterfall-to-interior transition is reachable. Fluid loop bookkeeping now retains
the sound ID and switches within XL water, while preserving classic behavior.
The native test checks texture waterfall selection, segment-contact selection,
waterfall -> underwater loop switching and idle contact silence. This verifies
selection and bookkeeping with headless audio, not audible output fidelity.

The added resource roots include all six laser levels, flare, both water sounds
and every wall clip. The original arena traversal still takes 1235 frames inward
and 90 outward, with matching native save continuation. The original archive and
descriptor remain unchanged.

A focused native collision check starts beside the original x=-25 solid wall,
requires clipping at the ship radius and retained tangent motion/velocity, and
checks exact boundary-turbulence samples and divisor with two SIM draws. Native
scrape applies a small shove, so this is not a frictionless slide oracle. This
case finishes with a collision-free remainder and does not cover the final
collision-velocity reconstruction branch; that factor remains source-reviewed
and separately arithmetic-tested. No stronger branch-coverage claim is made.

## Alignment with the published Counterstrike pilot

The approved history-preserving integration pins upstream
`fcb2c803d393e62231e064923112d0ee8a0ce896`. Merge it into the water branch,
then propagate the resulting tips through inspection, descriptors and browser.
Do not merge pull requests, rewrite published history or update the upstream branch.

Preserve the new Counterstrike completion, Android opt-in, save/export, corona and
XL menu behavior. Robotech remains a distinct arena pilot: it must not acquire
Counterstrike completion identity. Its new texture-corona emission remains off
until separately qualified; this preserves the water slice's previous presentation.
The original arena contract checks every side for that exclusion, in addition to
its existing native save/replay, source rejection and stock-transition assertions.
Both opt-in pilot paths reject multiplayer early. The generic XL menu indicator
continues to identify a loaded XL source, including the arena.

Retain bounded inventory/compiler fixes and descriptor admission guards. Metadata
feature colors describe feature support, not production admission of arbitrary
levels. Validate native contracts and both pilot paths, then run exact-head CI
for each updated pull request and the actual synthetic Android browser dialog.

Alignment validation also exposed host-only integration gaps: all executable test
consumers of the shared XL OpenGL library need the same hooks, and the save export
must include its C++ dependencies before legacy min/max macros. The original pilot
lifecycle assertions now accept the original retail archive's uppercase filename
and supply the pause consumed by direct low-level save calls. The shared mid-level
input-recording checkpoint had the same missing caller pause in both D1 and D2;
its call site now balances that native contract. The earned escape test verifies
that recording preserves the caller's pause state. No navigation/control policy
or campaign qualification is broadened by these fixes.
