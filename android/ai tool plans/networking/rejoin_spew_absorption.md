# Rejoin spew absorption

- [x] Confirm saved resource restoration and locate the existing absorption effect
- [x] Present committed reclamation with the powerup disappearance animation and sound
- [x] Cover both removal/ledger arrival orders without duplicate effects or pickup effects
- [x] Build both engines and validate recovery and migration tests

Restore living players' saved energy/shields independently of orb collection.
Recover unclaimed weapons, ammo and durable equipment instead of starting gear.
Existing rules for death respawns, temporary powers and level-specific keys remain.
Mine ammo converted into armed mines by the existing spew code counts as spent;
this is an exception to describing departure recovery as exact inventory.
Keep reclaimed objects out of the returning client's snapshot; play absorption
on existing peers with bound objects, once, even when removal and ledger messages
arrive in either order. Keep normal pickup and natural expiry behavior intact.

Native D1/D2 recovery harnesses pass: both message orders emit one animation and
sound, duplicate terminal rows do not replay effects, normal orb pickups do not
absorb, and collecting both orbs leaves saved energy 137/shields 64 unchanged.
Both ARM64 engine targets and the x86_64 APK build successfully. Scoped quality
checks pass. Paired emulator host migration/rejoin tests pass for native D1 and
D1-in-D2, both at level 8, including both host swaps, inventory conservation,
empty recovery worlds and one absorption for each excluded snapshot object.

Validation output is retained in `temp/rejoin-absorption-{native,build,arm64,d1,d2,quality}.log`.
