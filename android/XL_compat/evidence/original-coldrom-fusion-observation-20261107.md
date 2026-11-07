# Original Coldrom Fusion observation, November 7, 2026

This is a bounded native ordinary-player observation on the original Pumo
mission, not a Coldrom combat or escape pass. The mission source is commit
`55e67309066cead2f73a9c91541ae43bab8a7500`. The read-only original
resource set was checked against its 2,670-file private manifest, SHA-256
`5a971e04170af58c001d393e1b9a5b50d0eac8a67b4f60170c25fb5f3e8545bd`,
before and after the controls. No proprietary resources, saves, mine
coordinates or raw logs are included here.

The final observation executable's **file SHA-256** was
`a88bd6bb81896c4c9b421c96ae9a0b388b7519c0b29e35d292e57efff14b9c54`.
The observer header's **file SHA-256** was
`d5bbcdd4531c8a32e8099f3fde834b927e4da8d06db30cdfe59d1240cd6dc856`;
its **Git blob SHA-1** was `e8f6b95a7e296a06761efbb5b1a9a3a816bd00d4`.
These digest types are distinct. The executable, source header and input saves
were hash-checked after the run.

## Earned input chain and controls

An unchanged 15-action pickup phase from earned level-14 entry SHA-256
`f4072bfbb154e240c608fa8f825605d10bdc63a3b2faa1d77489b48b7baf86f9`
passed on the earlier observer executable, **not** the final executable. Its
executable SHA-256 was
`8b209cbb2476f4d535de2fbe4115248a1c283c507dc16baf28a7fc863d6f9069`;
its executed-plan SHA-256 was
`3827c2f61ee65022f2228b3aa4632b5dcb2e09adb58ea5daf22904d1177284f3`;
its PASS receipt SHA-256 was
`87d2cfcb22d3c84964e8c6f146fe9410b895d5844fc184aaf144c3e5990264cb`
(run ID `84cc4f87a140ad2a8862b33318d30d68`). It produced a unique level-14
post-pickup checkpoint, SHA-256
`880881341d54527cc9dbb3c4f986beca93908a5c176f71ac00434c5f86699a93`.
The save had 100 energy, primary ownership flags 17 and one remaining
objective. The final Fusion phase consumed this exact checkpoint. The final
observer changes after the pickup were read-only timer and player identity
fields, but the pickup was not rerun on the final executable.

Two independent controls on the **final** executable used the earlier
preserved post-pickup checkpoint, SHA-256
`e1f0bf4935cb521f9c36ded14d1dc0b5f65a4f5bfbf21e124bcee43129c84fb0`:

- Zero-frame control: 2-step PASS, executed-plan SHA-256
  `0e06753b7b229aacacc0f3ccef8ec32e176da640793262ec4224d392231449e7`,
  receipt SHA-256
  `5b61b47e023066cc636ee47cfa2ed97d28c1943498aa22eb522c6d5bddc04879`.
  Both snapshots held the same RNG state 3136845513 and call count 0. Idle
  Fusion queue was false and its timer delta null. Player object/signature
  were 0/0 in both snapshots; reactor and guard bindings were live.
- Save/load control: 5-step PASS, executed-plan SHA-256
  `4ce2c7a9302a02b39ed5a3006c8c836a2abe943a25e249aca3e21148aca79b1e`,
  receipt SHA-256
  `40c6d4a0dd92afe7af3d687dbf20af3d0c4a9ee8decbe38f33b87389ac2e159a`.
  A normal quick-load advanced the binding generation from 2 to 3 and
  invalidated the prior reactor and guard bindings, although their slots
  remained occupied. RNG state and call count did not change.

The first Fusion candidate was retained as a failed input. Its executed-plan
SHA-256 was `e921a9ea43f1c9358a4e6d17287bd0d3ba517436cebf07e5c1273602a20d5f72`.
After a 250 ms held press, energy and charge remained unchanged because the
normal primary-fire rearm delta was still +49,152 fixed ticks. The source's
weapon selection path sets `Next_laser_fire_time = GameTime64 + REARM_TIME`,
where `REARM_TIME` is one fixed second. This result stopped before release and
did not establish a firing defect or a shot.

## Bounded Fusion release

A distinct candidate waited up to 1,500 ms for the normal readiness delta to
reach zero, then held primary fire for the same 250 ms and released it. Its
strict 14-step PASS receipt had executed-plan SHA-256
`8db3f2fe90efde8d38daa7eee13f1891d1d14452dcdc79c316a3d86ad4b3617b`,
receipt SHA-256
`5145ccc7b934fe42886cd51bfb07071ffb0010ec759ce19c5180c51f1a178ec8`,
and run ID `a40881b261c4c99f7c372c4753c94bcf`. The strict phase checked its
fresh checkpoint, plan and executable hashes and normal level-14 endpoint.

Before pressing, no player-owned weapon object was present. During the press,
Fusion charge reached 16,384 fixed ticks, its pending timer delta was +547,
and energy fell from 100 to 97.75. After release, the charge reset to zero,
the queue cleared, and energy was 97.44999694824219. Two unique live weapon
objects, slot/signature 2/161 and 3/160, had weapon ID 14, current player
parent object/signature 0/0, creation time 121241 within the press/release
window and remaining lifetime 655360 fixed ticks. The owned-weapon view was
not truncated. Level 14, ownership flags 17 and one remaining objective were
unchanged, with no player death, invulnerability or level transition. Presence
and identity of these objects, not fire count or energy alone, support the
Fusion release observation.

The bounded result qualifies an earned Fusion charge and release after a
declared local view/position assist. It does not qualify travel to the reactor,
damage to any required actor, reactor defeat, trigger operation, escape or
branch rejoin. Those remain separate route gates.
