# Complete authored equipment selections

This checkpoint follows strip admission PR #578 on the immediate-parent stack
rooted at cmake. Work remains within the November 18, 10:00 AM PST handoff,
reserving 06:00-10:00 for final validation and CI repair. No agents or merges.

## Problem and intended behavior

Control level 2 and Boiling Point level 5 select currently rejected equipment
IDs 7, 8, 9, 24, 26 and 27; Boiling Point also selects 46 and 47. Preserve every
authored selection bit and the exact selection RNG. Qualify creation, animation,
physics, contact, occupancy, persistence and subsequent production for all eight
IDs before removing their admission blockers.

The pinned XL dispatch treats unused slots, turbo, monsterball and megawow as
unhandled pickups. Hoard orb and flags require their respective multiplayer
modes, which remain outside admitted XL single-player runtime. An inert pickup
must remain a real generated object that occupies its producer; it must not be
silently filtered, consumed, substituted or awarded as a different item.
Redux Debug currently grants a megawow reward. Introduce only the necessary
XL-owned policy and thin native pickup hook, preserving stock Debug behavior.

## Ordered implementation and acceptance

1. Extend the pinned actual-source reference with pickup dispatch and mode-gated
   handler controls. Keep the existing 210 producer comparisons and add all
   single selections plus complete original masks, boundary selection tokens,
   unchanged item creation attributes and exact RNG transcripts. Verify source
   hashes before extraction. Test adapters may isolate absent multiplayer/UI
   services but cannot replace the behavior being compared.
2. Add a separate derived native owner through the real loader and retained
   resources. Exercise all eight item IDs, ordinary contacts without rewards or
   deletion, live animation/physics and occupied-producer timing. Cover pending
   and spawned disk restore through stock, malformed incoming ownership and
   exact masks. Record and cleanly replay earned futures with detailed native,
   world, object and SIM observations. Keep existing observer equivalences
   visible and preserve raw receipts.
3. Implement the narrow pickup correction, then admit the fully qualified mask
   members. Preserve multiplayer, resource, layout and capacity guards. Run
   assertion-enabled Windows and Linux checks, optimized native/source checks,
   ordinary developer-OFF original levels and rendered/Android controls. Use
   protected device state, fresh outputs and reusable registered runners.
4. Publish a non-draft fork-only immediate-parent checkpoint after coherent
   local qualification. Inspect exact-head CI. Run the unchanged original eight
   and frozen 64-case checks with authentic pinned private assets. New failures
   exposed after admission remain failures; do not mask them or change budgets.

## Dependencies and limits

PR #573's trusted staging adoption is still awaiting user action; supplemental
private CI cannot be credited before that controller includes the pinned inputs.
Public CI and local original-asset checks can proceed independently. Android's
general XL replay guard and startup FX mismatch remain separate open work.
This plan does not claim connected campaign completion or multiplayer support.
If equipment qualification exposes a new dependency, preserve its receipt and
leave the affected admission guard until the dependency is independently owned
and tested within the remaining window.

## First checkpoint: pickup policy and complete selection comparisons

The source reference now extracts DoPowerup, PickupHoardOrb and PickupFlag from
objects/powerup.cpp SHA256
5312c9cd927adbfaa71d298aadeacee8bab7d5190a27db4534d55c4dbbf4d366,
including the relevant actual handler assignments. All eight single-player
dispatch controls pass. The existing 210 producer cases remain, with 165 cases
added for eight single selections and three unchanged original mask rows across
five difficulties and three seeds. All 375 producer and 13 initialization
comparisons pass in equipment-masks-native-01.

New native contact controls create each item through the producer, preserve its
loaded definition, verify two contacts give no rewards or deletion, verify the
remaining item blocks further production, and require zero selection SIM draws
for a single choice. Optimized controls pass. Assertion-enabled
equipment-masks-debug-01 reproduces the Debug MegaWow reward, including the
native MEGA-WOWIE-ZOWIE message. An XL-owned policy hook now suppresses that one
Debug reward in XL worlds. equipment-policy-debug-02 passes all 375 comparisons,
all eight contact controls, and a stock Debug control proving the native pickup
still consumes the object and grants 200 energy and 50 missiles.

The Android Debug APK is
0bbb08d970c89502ff272fae87b1c5bee3338cf5ca3fba6d6bb1007cc49a6076.
Its actual predecessor d5fd45b2cbdc46c13922a28abe81efd84503a7a81f6e0f6a8644e2fb923cd62c
was backed up before installation. android-equipment-pickup-01 passes 10/10
steps in 11.739s, run 3535761b8b094a46b25dc03f339185f3, and restores protected
profile hashes exactly. This uses eight temporary native allocations and direct
pickup calls after loading the existing derived XL fixture; it proves contact
policy, not full generator rendering or persistence. The focused runner emits
the exact passed action sequence while leaving the original full persistence
script unchanged. Its prior baseline step-43/61 failure remains open.

No equipment admission guard has been removed in this checkpoint. Actual
incoming-mask ownership, cold lifecycle, rendered/Android generated objects and
recorded futures remain the next step before original level admission. Stock
HAM definitions for six inert slots have zero size and clip zero; preserve those
authentic definitions rather than synthesizing replacement artwork or radii.

## Lifecycle qualification in progress

PR #579's stock Debug assertion originally used RELEASE in a C++ translation
unit, but that macro is defined only for the native C files in the optimized
Windows build. equipment-lifecycle-full-01 preserves the resulting test failure.
The checkpoint repair e8726f1cc6a1b06387c1752a38638e78b30d5687 uses NDEBUG,
which distinguishes both languages' actual build configurations. The strict
stock Debug reward assertion remains enabled in Debug. No pickup policy changed.

equipment-lifecycle-full-02 passes all 375 source producer comparisons, 13
initialization comparisons, eight extracted pickup dispatch cases, existing
malformed-owner guards and cold continuation, plus eight newly authored incoming
single-item lifecycle cases. Each case earns its pending phase, cold restores
through stock, records and cleanly replays 120 zero-input frames, checks the
generated native definition and repeated contacts, cold restores the spawned
item, checks occupancy, then removes the item explicitly and verifies production
restarts. Controlled setup uses a player pose and stepped producer clocks;
these are not unassisted campaign controls. The shared future comparison retains
raw_equal=false and the established semantic observer equivalences; SIM state
and call counts must match exactly. No RNG rewind or table swapping is used.

equipment-inert-debug-matrix-01 passes each of IDs 7, 8, 9, 24, 26, 27, 46 and 47
under assertions using the unchanged 80-second per-process diagnostic guard.
equipment-shared-strip-01 passes the existing complete strip/source owner after
the replay helper is shared. The original combined owner and timeout remain.

The Android candidate APK is
5c5df75e2fd54c7c12090d118c81eed4494462bc81dac4c7e1fa950ff84257b2;
the actual previous installed APK was backed up and matches
0bbb08d970c89502ff272fae87b1c5bee3338cf5ca3fba6d6bb1007cc49a6076.
The old full equipment failure is a test sequencing defect: the native save
preserves the top-level trigger's 750 ms cooldown, so an immediate trigger after
restore is correctly ignored. Introspection now exposes the existing cooldown
state, and the runner waits for its expiry before repeated authored activations.
android-equipment-cooldown-01 passes 65/65 in 116.648s, run
9999e539930a4dc5bbf8fcf557f6b5ad, with exact protected-profile hash restoration.
This repairs the full owner rather than skipping its former failing step.

android-equipment-inert7-01 passes 74/74 in 81.852s, run
93da3fc2ff30458d9416742c928005dc. All eight initial device cases now pass 74/74, with exact protected-profile
hash restoration. Their timings and run IDs are recorded below. The reusable -InertId runner verifies exact native fixture hashes,
loads the authored mask, checks pending and spawned cold transitions, permits
live rendering, and invokes repeated pickup dispatch on the real generated
item. The probe preserves the object and compares the complete native player
structure; it does not create a replacement item in this mode.

The supplemental ordinary developer-OFF two-case run now passes Control (level
2), 120 frames in 6.937s, but level 5 still rejects before simulation. Engine
SHA256 is 8c38eccdf183724c714fc6873751bb0d25ea7d97253f66f618c5f6504d3ede6a.
equipment-control-rendered-01 also passes 120 actual rendered frames, engine
SHA256 3e499da3ffa2b34d2f79f1478d39e6c0379480fecb1058660520c94f14a9aba1.
These are diagnostics on the unpublished admission candidate, not new official
readiness. Latest complete published CI remains 62/64 and original 8/8.

Level 5 exposes a different dependency after mask admission: wall 31 at segment
502 side 4 is an exploding blastable clip 46, connected to segment 509 with no
wall on its reciprocal side. Existing strip ownership requires paired walls.
The unchanged input remains rejected; no resource or reciprocal-wall guard has
been removed for this case. This needs independent one-sided strip ownership,
native explosion flag safety, source comparisons and lifecycle qualification.

Local Linux runtime remains unavailable: both WSL inventory and a direct Ubuntu
/bin/true query time out. Only the agent's own timed query processes were stopped.
PR #573 is still awaiting user adoption for trusted supplemental CI staging;
public source compilation is not private-asset runtime qualification.

### Completed Android incoming-mask matrix

| Item ID | Seconds | Run ID                           |
| ------- | ------: | -------------------------------- |
| 7       |  81.852 | 93da3fc2ff30458d9416742c928005dc |
| 8       |  74.612 | 82bd6056b02f4e878078e8bfdee3b33b |
| 9       | 106.227 | 958c2d4df3644f0e9be174014710d06b |
| 24      | 108.088 | 62e94811fa7548deac43597bf872f2e5 |
| 26      | 112.436 | b81b27c7e0ed496f86cdd5118fdc5011 |
| 27      | 110.707 | 059941ce772f4dee933c0f7a5f434ea1 |
| 46      | 106.685 | 6223a86329db4058ba4d65bbea3b9a3f |
| 47      | 112.493 | cef0e3a70d9846e4991940747ac5f518 |

The initial flag screenshots face a wall rather than the generated item.
Their lifecycle passes stand, but they do not prove visible flag rendering.
The runner now uses an exact native-fixture pose for IDs 46/47; separate
view-02 runs both pass 74/74 and show their authentic blue/red sprites after
cold restore. Six zero-size source slots retain their authentic
definitions and have no fabricated visible artwork.

Flag 46 view-02: 135.329s, run d47310a202494131bae7ffa19f6c3151,
74/74 and protected hashes restored. The pose is controlled; artwork, size,
clip and native generated object are unchanged.

Flag 47 view-02: 122.671s, run 9f3669054e1b4b23be067872e9c65818,
74/74 and protected hashes restored. The pose is controlled; artwork, size,
clip and native generated object are unchanged.

The Android fixture gate accepts exactly the Windows and Unix newline variants
written by the native descriptor stream, with independently pinned hashes for
each. HOG bytes remain pinned exactly. This avoids making the new Linux runner
mode depend on Windows text-stream line endings.

Original Control Android android-control-original-01 passes under owner
ddb94ed325124f0db80228e0d9e8a961, with exact protected-profile restoration.
Warm: 35/35 in 69.866s, run
52e6c134a4f44b21b49ea7dac0dc8010.
Cold: 8/8 in 31.319s, run
47f0d92f9370481d91cf7df308761454.
The warm owner includes two actual EGL context recreations, two park/wake
cycles and controller movement after resume. Cold restore preserves original
archive ownership and level 2. No inventory or pose assists are used in this
original-level owner; the native-written pilot only unlocks level selection.
