# Ordinary XL Gauss signed draws

Pinned D2X-XL ordinary Gauss torque calls SRandShort, which is 16383 minus one
RandShort draw. Redux ordinary collision currently uses draw minus 16384. Add a
tiny C-compatible draw interpretation helper and use it at the three ordinary
torque assignments for the XL singleplayer profile. Consume one d_rand per axis.
Keep fixmul, skip-count increment/saturation, thrust flags and boss/companion/
static exclusions unchanged. Preserve classic/D1-in-D2 and multiplayer dispatch.

Do not route ordinary robots through the camera torque bridge: its bounded
negative/overflow behavior is different. Reuse its existing hash-pinned source
oracle to compare every signed draw and retain its frame/skip/RNG controls.
Demonstrate the old arithmetic fails, correct the sign/offset, and independently
mutate sign and offset to check test sensitivity. This checks draw arithmetic;
it does not alone execute the full ordinary collision consumer.

Add ordinary-contact torque, skip, flags and exact RNG ordering assertions to the
existing native projectile owner for XL, classic and controlled multiplayer
profiles. Keep this distinct from full multiplayer synchronization coverage.
Use the existing hosted exact-head native lane rather than expanding the linked
CPU collision harness. Native execution remains required before qualification;
no campaign completion or merge readiness is implied by the local oracle.
