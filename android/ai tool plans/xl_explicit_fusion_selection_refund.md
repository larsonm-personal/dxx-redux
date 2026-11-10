# Refund held Fusion charge on explicit XL primary selection

Pinned arbruijn/d2x-xl commit 2576a98b5de310e3ac7c9777df2677d1627b8bae,
weapons/weapon.cpp DoSelectWeapon lines 499-562, adds the exact stored charge
to player energy and clears charge at 513-514 in the primary branch before
toggle/availability checks. Source confirmed through the GitHub connector.

Add this refund only to active XL singleplayer explicit primary selection.
Do not cap energy or require positive charge. Preserve classic/D1-in-D2 and
multiplayer, secondary selection, lower-level select_weapon, cycling and
autoselection. Failed or unavailable selection still preserves its existing
pending queue/timer even though XL refunds and clears charge first.

Extend the existing no_damage_fusion native owner with successful, repeated,
unavailable, same-slot, owned toggle, secondary, direct lower-level, cycling
and usable-autoselection cases. Test zero, one fixed-point unit, fractional,
negative and unit charge at full energy, protected and ordinary segments,
stock and controlled multiplayer dispatch. Save/restore player/config,
selection-history arrays, queues/timers and RNG. Update the earlier successful
switch frame control to require XL's refund and retain cancellation behavior.

No new harness or local build/checkout near the reserved storage floor.
Formatting and exact source/blob checks do not constitute native execution.
Native fail-before/pass-after and actual production mutations remain unrun;
require exact-head hosted xl-pr-assets native owners for qualification.
Current parent native provisioning is blocked by HTTP 504 downloads and the
inherited projectile owner remains unresolved. No merge-readiness claim.
