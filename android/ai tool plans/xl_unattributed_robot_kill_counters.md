# Unattributed XL robot kill counters

Pinned D2X-XL increments local level and total robot kill counters only when the
killer object index is nonnegative. The common Redux consumer increments both
counters for any lethal hit. Ordinary generator contact uses killer -1, so its
mature lethal damage should retain death effects without crediting local kills.

Gate both counter increments in the XL singleplayer profile only. Preserve
existing positive-killer, classic/D1-in-D2 inactive and multiplayer behavior.
Do not change death initiation, shield damage, object-trigger attribution or the
generator population policy itself.

Use the existing linked CPU consumer to prove fail-before/pass-after with mature
boss and non-boss victims, lethal and surviving damage, negative killer indices,
player/nonrobot/guidebot attribution, classic/D1/multiplayer and cheat controls.
Seed both counters separately and verify death and trigger bookkeeping. Mutate
unconditional counting and local-player-only counting independently.

Extend the existing native generator occupancy fixture with a mature low-shield
robot killed by real materialization-center contact, preserving normal death and
both counters. No new harness or native environment setup. Exact-head hosted
runtime qualification is required; local CPU coverage alone is not native
campaign or merge-readiness evidence. No merge is authorized.
