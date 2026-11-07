# XL weapon charge and orientation clocks

Base #394, 0d9461e028e0ac793a3b621e49a33de06d615f85.

Qualify two bounded weapon consumers before admission: Omega charge increment
(integer divide by four before world-clock division), and visible homing-missile
orientation (multiply turn time by sixteen before world-clock division). Test
actual native entrypoints, charge cap/energy debit, rounding, difficulty and
classic/network isolation. Preserve current native gating and normalization;
do not claim complete firing or homing parity. Add pinned-source arithmetic
comparisons and run relevant native/classic/public regressions.

Further weapon work is required: reference fire eligibility uses last-shot
relative deadlines, and Redux has no last-missile timestamp. Redux Omega also
uses a one-third-second cooldown whereas the reference uses last-fire frame
indices. These need coordinated state/persistence qualification before complete
slow-motion admission. Door/cloak reversal is not equivalent to uniformly
stretching all wall durations; source cloak progression still uses its fixed
one-second duration. Effect timing, controls, authentic pickup rendering, audio
and persistence remain unfinished. No original assets or cohort changes, no merge.

## Qualified results

3840 actual omega_charge_frame calls match the extracted charge expression and
OmegaEnergy arithmetic over four clock profiles, multiplayer isolation, five
difficulties, eight frame durations, four charge positions and three energy
levels. Include integer-division boundaries, the default one-unit cap, zero
energy and debit clamping. Native ownership/death/current cooldown gates remain
unchanged. This does not claim the reference frame-index recharge gate.

512 actual homing_missile_turn_towards_velocity calls verify the extracted scale
and native orientation update for four directions, eight turn times, both actor
identities and multiplayer across the four clocks. This is visible orientation,
not new physical homing-guidance qualification. Native normalization fidelity is
unchanged; the oracle independently validates the scale, not the entire matrix.
The new consumers leave the simulation RNG count unchanged.

Reference revision 2576a98b5de310e3ac7c9777df2677d1627b8bae, exact supported-setup
raw source pins:

- weapons/omega.cpp: 76f003622dcbac54dbd533fd4dfa8537c7ceceed0c75c5310c83b7b075cfb61d
- weapons/fireweapon.cpp: 37ae81f686e063ca7102bb9154368e67626e843ae898d82e8583167d31dbaac8

Source ASan/UBSan passed (leak detection disabled); fourteen malformed,
missing/duplicate/incorrect transcript variants rejected. Existing motion,
contact and AI comparisons remain mandatory. Native water/lava/projectile/cloak
owners passed 4/4; classic upstream/trigger owners 2/2; public source/GPU/audio
owner 55/55; private-assets Python policy 33/33. Scoped quality and catalog
validation passed. Catalog execution remains blocked by the host's missing
/proc/self/task/<pid>/children interface, child-supervisor exit 125.

Native engine sanitizers and Windows/Android execution were not run. No original
rendered Warp Shock qualification or readiness improvement is claimed. Original
Neptune HOG remains SHA-256
4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303.
Assets PR #4 sprites at d09f199c3b66bab71970364b1939b1658851ea81 remain unused by
this numerical stage; admission stays disabled.

## Door/effect audit for the next stage

Pinned main/wall.cpp AnimPlayTime scales total duration. Cloak reversal at lines
306/328 scales its timestamp, but DoCloakingWallFrame/DoDecloakingWallFrame still
compare with unscaled CLOAKING_WALL_TIME. The current cloak save validator bounds
time to one second, so correct reversal needs coordinated state qualification.

Pinned main/effects.cpp EffectFrameTime scales frame periods. Wall effects at
line 226 subtract world-scaled elapsed time, while object effects at line 298
subtract raw elapsed time; the accumulator resets at line 364. Uniformly scaling
all animation steps would not reproduce this source. These paths need explicit
source oracles before production changes.
