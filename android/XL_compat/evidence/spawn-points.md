# Authored singleplayer spawn points

Type-28 wall and admitted nonplayer object actions configure an XL-owned table
of 16 spawn transforms. Redux retains its existing Player_init table and respawn
path in the shared binary. Configuring a destination does not teleport the live
player. The next ordinary death/respawn uses the retained position and orientation

## Behavior and persistence

Authored links cycle across all 16 XL slots. Each transform uses the linked
segment center and source-matched inward side orientation. Player activations
show the source's action-specific message unless TF_NO_MESSAGE is set; this
message does not require a shot. Object events show the same message: the source
accepts their negative player index. Set-orientation and silent bits do not
suppress this cue; TF_NO_MESSAGE does

The first live, exactly named SPAWN marker in the nine singleplayer slots takes
precedence during respawn. An authored spawn action removes that first marker,
allowing the new authored destination to take effect. Lowercase or unrelated
markers remain ordinary markers. The port preserves their object IDs and slots
instead of reproducing the reference's unsafe overlapping table compaction.
Trigger-driven deletion is an internal effect, not another recorded input command

Ordinary death preserves configured spawn points. New level/source loads clear
them. Development trailer schema 8 directly replaces schema 7 and stores either
an empty table or all 16 rows. Validation checks integer types/ranges, the exact
cyclic target sequence of one retained authored action, compiled positions and
orientations, and source geometry identity before publishing any feature state.
Pending object-master configuration survives removal of its initiating owner;
it never moves a replacement occupying the same object slot

Empty target lists are rejected during inspection instead of reproducing the
reference's modulo-zero behavior. Invalid segment/side references are likewise
rejected before loading. Multiplayer spawn selection, manual marker teleport
shortcuts and original-mission qualification remain separate requirements

## Source and native evidence

Reference: arbruijn/d2x-xl 2576a98b5de310e3ac7c9777df2677d1627b8bae.
The oracle extracts DoSetSpawnPoints and the actual TriggerSetOrient/math
functions. It links no Redux transform implementation. All 1,536 transforms
across 96 tables match exactly on Windows MSVC and Linux GCC with -Wall -Werror.
Cases cover six sides, four source regions, and one, two, three or ten links.
Sixteen player/object cue comparisons also match the extracted PrintMessage
function, using actual HUD duplicate detection to observe the runtime message.
The oracle's marker adapter is inert; marker lifecycle is verified separately
through the engine, not claimed as part of the mathematical comparison

Audited CRLF source hashes:

- main/trigger.cpp:
  74f1284260fb4a4bb7396bc45eeb83868a7f2dfa5e3fdf0bcb767b94f1b12852
- objects/player.cpp:
  bab2ca407a6ac51993fb1194b785caa8101fe0a8a5896bd8a010cf68e07c09e5
- gameio/loadgame.cpp:
  5d7ff36d6121411e0250be94353bbe3268bdd02b9bd35c6161951d879fcecc55
- objects/marker.cpp:
  303125f2f7d0ace7f77d3b008cd6423082ae6662aeafef0f8d35db9b76386a50

The native integration exercises a real wall crossing, object autoplay, lethal
damage through the death lifecycle, ordinary respawn, marker precedence and
case sensitivity, saved named markers, first-marker cleanup, independent
delayed descendants after owner removal/slot reuse, disk restoration through
stock, source reset and unchanged Redux spawn state. Thirteen malformed
trailers are rejected without partially publishing spawn, boost or event state.
The saved fixture combines earned spawn points, a boost and a pending event

Twenty-four focused native regressions passed, including inventory, teleports,
boosts, schedulers, object events, bosses, generators, resources, properties,
water and source transitions. This validates feature integration within those
controls; it does not certify every original campaign

## Derived fixture and Android acceptance

The fixture uses hash-verified original Neptune geometry, rewrites authored
actions, and is explicitly named Derived spawn points. Original source files
remain unchanged. Native generation produces these identities:

- spawn.hog: 0e3c46294cf1d4d992a70958753b733378d4f2fbfc16c3f4977950f5c2a9c147
- spawn.mn2: a91fc95bca8e7a0f1a1adf8922b155225b82831a30d0caeedbf06f115c59defe

An initial Android attempt used segment 70 as a respawn destination. Its center
is valid but the full player sphere overlaps side planes (side mask 5), and the
observed ship subsequently occupied an adjoining segment. The fixture now uses
segment 60 and asserts that the player's sphere fits, then advances native
physics. Transform-oracle samples still include narrow geometry; no production
clamping or alternate placement was added to disguise authored destinations

The final formatted APK passes all 56 Android steps in 195,937 ms, run
ffb2553ca8044d6b8e4be5da34d252cb. APK SHA-256:
9b72c7c763cb58fa83808673d23d61f5b2662fb780c5448655f0dc397ae534ed.
The registered runner verifies two actual deaths, authored replacement,
memory/disk checkpoint cycles, nested scheduling, stock transition and quick-load
restoration. It uses controlled poses and actions, not navigation. Protected
player/configuration/mission hashes match exactly after restoration

The stock-transition assertion was corrected to expect inactive XL state rather
than an XL-only introspection field that is absent in Redux. The native test
separately checks that the underlying spawn table is cleared. Final scoped
quality checks, both catalogs (399 top-level entries, 454 support scripts) and
all 12 private-asset policy checks pass. The formatted native four-test set
passed in 31.82 seconds; the final spawn/cue integration passed in 6.58 seconds

## Original mission evidence

The hash-verified survey contains 20 wall spawn-point actions across six selected
levels: dinter, sphere sp-04, anthology d3/d2, antholgy and boilpnt. Fresh inspection
in [spawn-point-candidates.json](spawn-point-candidates.json) decodes five; antho-d3
still rejects an invalid trigger target. The report preserves exact original
payload hashes and separates raw inspector findings from runtime qualification.
Expanded capacities, environment properties, effects and other actions remain
independent requirements. None of these six originals is promoted by this tranche
