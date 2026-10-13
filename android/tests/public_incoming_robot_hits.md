# Incoming robot hits on the compact combat routes

`public_incoming_robot_hits.py` strengthens the two existing combat cases without
adding scenes or changing any of the nine admitted replay/oracle pairs. Its small
causal profile comes from the original full live captures and their verified
object/world playback boundaries:

| Game | Original robot slot / ID / signature | Projectile slot / ID / signature | First-live boundary | Direct hit frame | Player shields |
| --- | --- | --- | --- | --- | --- |
| D1 | 8 / 3 / 9 | 68 / 5 / 76 | 70 | 112 | 100 to 94 |
| D1 | 8 / 3 / 9 | 66 / 5 / 78 | 76 | 117 | 94 to 88 |
| D2 | 11 / 37 / 12 | 42 / 41 / 112 | 157 | 168 | 100 to 98 |
| D2 | 11 / 37 / 12 | 90 / 41 / 150 | 237 | 247 | 98 to 96 |

A hit frame denotes the action between that frame's pre-state and the next
boundary. Shields are validated in raw fixed-point full-world state, not merely
rounded HUD values. Every frame and the terminal result must agree, every shield
delta must be explained by the expected direct projectile hits, and the player
must remain alive. The live projectile must retain its original robot owner,
damaging state and identity throughout its life, become dead on impact, and
retire on the next boundary. Its owner must be alive when it is emitted. D2's
second projectile correctly arrives after its owner starts exploding at frame
241; the gate does not mistake a delayed live shot for a living shooter at impact.

The shared `player_weapon_damage` observation is emitted only immediately after
the ordinary non-radius weapon collision applies actual player damage. It records
the player and projectile identities, robot ownership and exact shield change.
Wall collisions, contact attacks and explosions cannot enter this direct-hit
hook. D1 previously had no player-damage event; D2's production player-damage
events identified the robot but omitted the projectile. This read-only trace
closes that evidence gap in both engines. No trace record is a gameplay command,
and no poses, health, inventory, AI or collision outcomes are modified.

The new observations are checked semantically against the separate profile and
complete object/world boundaries. They are not inserted into or used to regenerate
the old immutable frame, diagnostic, terminal, SIM or damage/loot oracle payloads.
The ordinary compact runner automatically enforces both the outgoing combat gate
and this incoming gate. It still compares every original oracle field.

## Verification

- `python android/tests/test_public_incoming_robot_hits.py` checks event schemas,
  wrong owners/projectiles, staged or non-live projectiles, premature retirement,
  missing or extra hits, raw shield corruption, wall-only damage and player death
- `android/tests/test_compact_public_replays.ps1` runs both existing cases twice
  alongside the unchanged public corpus; choose `-RunMode headless` or `graphics`
- `android/tests/test_compact_public_replay_failures.ps1` preserves the ordinary
  flight/aim controls but removes combat primary shots. It requires a real engine
  terminal mismatch, rejection by the outgoing kill gate, and separate rejection
  by the incoming gate. The still-living robot produces an extra incoming hit in
  each game, at D1 frame 134 and D2 frame 261. A load/start failure is not accepted
  as a negative control
