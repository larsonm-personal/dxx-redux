# Combat sound consumers

XL now selects its own ordinary weapon firing and weapon/actor explosion cues.
Small hooks in `d2/main/laser.c`, `fireball.c` and `collide.c` dispatch to
`runtime/combat_audio.cpp`. Redux remains the alternate when XL is inactive.
An unavailable XL sample is consumed without falling back to Redux.

## Source decisions

Reference: `2576a98b5de310e3ac7c9777df2677d1627b8bae`.
`tests/reference_combat_audio.cmake` checks source hashes and extracts the whole
`CreateWeaponSound` function, splash classification assignments and predicate,
and the actual weapon, actor and water explosion audio calls. Storage, options
and audio destinations are adapters; the decisions are source code.

- Local firing uses the player class; the Vulcan cue has half volume. Remote
  firing uses a spatial generic cue at full volume. Missing parents use the
  laser class, while an in-range empty parent slot follows source `OBJECT`
  lookup and remains spatial. Redux QuietPlasma and reactor reductions do not
  alter XL firing.
- The source splash classification selects logical sound 57 only for IDs 18,
  28, 40, 54, 58, 59, 61 and 62. Positive damage radius alone is insufficient.
  Other weapon explosions use 11. Actor explosions use 58. These resolve to
  three distinct samples in the retail bank, not interchangeable aliases.
- These explosions use the explosion class. The source D1 mission distinction
  and water call's separate selection are preserved. Local firing also records
  its logical sound, pan and volume through the legacy demo event writer.
- Omega retains its existing generic firing path, matching the separate source
  `weapons/omega.cpp` caller. Optional hires gatling, missile-flight and flare
  addon behavior is outside this increment; the oracle disables those options.

The source adapter compares 10,752 decisions across all byte-sized weapon IDs,
parent positions, sound availability, make-sound flags and D1/D2 selection.
It is part of the existing public named-audio contract, with no new CI process.

## Engine and output controls

The shared native/Android fixture adds fifteen captured-output cases: eight
actual local/remote firing events, missing and empty parent consumers, actual
concussion/mega explosions, actor explosion and a two-cue water impact. Silent
and unmapped firing cannot create voices or fall through to Redux. Each case
checks the selected sample/class/gain and actual PCM. A real local Vulcan shot
also produces exactly one 13-byte logical demo event.

Explosion damage is set to zero and unrelated AI awareness is suppressed in
the water fixture to isolate audio while executing production explosion and
collision handlers. These are controlled event checks, not campaign completion
or proof of unrelated damage/AI behavior. Temporary objects, weapon resources,
viewer selection, texture flags, RNG and sound mappings are restored.

Native stock-transition controls compare exact captured Redux PCM for
QuietPlasma and both weapon/actor explosions against the existing stock sound
APIs, requiring no XL voices or owners. The complete audio runner then restores
the XL save and verifies persisted playback and backend reopening.

Windows source and both 23/40 and 27/40 runtime runs pass, including the water,
stock PCM, demo, save and scope controls. All five selected Linux checks pass in
86.92 seconds: the modified source contract, full audio integration, projectiles,
lava volumes and water arena. Android passes all 101 steps in 343.875 seconds,
checking 1,156,096 samples across 75 output cases at 48 kHz, including all fifteen
combat cases. All 26 protected files are restored with identical hashes. Both
Android catalogs, scoped formatting and diff checks pass.
Logs are under `temp/xl-gameplay-audio-20261029` in
`C:/local/dxx-redux/temp/xl-batch2-20261027`.

## Remaining work

Robot/boss and afterburner caller policies are covered by the subsequent
[actor/afterburner increment](actor-afterburner-audio.md). Ambient source
distribution, external sound-object playback, wall-change lifecycle and classic
replacement installation remain separate requirements. This increment does not expand
mission admission or establish complete gameplay sound fidelity. Legacy demo
event writing is checked; complete XL demo playback is not claimed.
