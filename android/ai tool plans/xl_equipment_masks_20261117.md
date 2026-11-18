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
