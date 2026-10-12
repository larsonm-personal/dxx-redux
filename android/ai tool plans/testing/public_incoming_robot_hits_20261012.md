# Qualify incoming robot projectile hits

## Plan

1. Preserve all nine public replay/oracle pairs and reuse their ordinary control routes
2. Add shared read-only direct projectile damage observations for D1 and D2 where existing traces omit collision identity
3. Require exact hit frames, live projectile identity, original robot ownership at birth, full-world shield deltas and a living player throughout
4. Reject damaged event/object/shield evidence and a real changed-control playback; test both native console and headed playback

## Evidence scope

The incoming profile is separate from immutable capture oracles. Existing live captures already contain the shield and projectile boundaries. New observations prove direct weapon damage and are never replayed as commands. D2's second projectile was emitted by a living robot before its death and reaches the player during the robot's explosion
