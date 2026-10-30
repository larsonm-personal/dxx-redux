# XL combat audio

Worktree: `C:/local/dxx-redux/temp/xl-batch2-20261027`.
Branch: `xl_more_64_combat_audio`, above published PR #213.
Authorized block ends October 30 at 04:57:30 UTC.

1. Port weapon firing and weapon/actor explosion sample, class, volume and
   spatial decisions from pinned XL source. Keep Redux as the runtime alternate.
2. Compare against extracted source with caller adapters; cover the source's
   explicit splash classification rather than inferring it from damage radius.
3. Exercise actual firing and explosion engine calls with PCM capture on native
   and Android. Check silent cases, stock isolation, and save/lifecycle coverage.
4. Run scoped quality checks and relevant Windows/Linux/Android validation,
   publish the verified increment, and monitor the stack without merging.

Pinned source: `2576a98b5de310e3ac7c9777df2677d1627b8bae`, especially
`weapons/createweapon.cpp`, `objects/object.cpp`, `effects/fireball.cpp`, and
`physics/collide.cpp`. Optional hires gatling/missile/flare addons, ambient
distribution and classic replacements require further work; no admission
expansion or complete gameplay audio claim follows from this increment.

## Verification checkpoint

The pinned source adapter passes 10,752 decisions. Shared controls now cover
fifteen actual PCM cases plus a real local firing demo event. Windows passes
both formats, stock QuietPlasma/weapon/actor PCM isolation, save and scope tests.
All five selected Linux checks pass in 86.92 seconds, including projectiles,
lava and water regressions. Android passes all 101 steps in 343.875 seconds,
including fifteen combat output cases, 75 total output cases and 1,156,096
checked samples. All 26 protected files are restored exactly. Builds, both
catalogs, scoped formatting and diff checks pass. See
`android/XL_compat/evidence/combat-audio.md`.

The initial native failure came from using the existing spatial fixture's
detached listener as a firing parent. The firing control now selects the real
player and restores the incoming listener afterward. No production workaround
was added for that invalid fixture setup.

## Next caller audit

Boss teleport/death already use the owned `runtime/boss_spawn.cpp` path; changing
only the old `ai2.c` boss functions would miss XL. Ordinary robot final death uses
standard explosion 11 at volume 2, whereas boss final death uses actor 58 at
volume 4. Both use the explosion class; groans and boss see cues use robot class.

Source afterburner uses live owner lookup to recover a missing loop, instead of
Redux's input-transition-only test. Its lookup skips stopped owners, which can
allocate duplicates while muted and leave a stopped loop alive after release.
The next increment should preserve the intended recovery while testing muted,
stopped, released and restored ownership explicitly; document any source repair.
