# Original Tulev1 master5 and blue-key owner

## Scope

Register one native player owner after the original Tulev1 spawn owner. The new owner checks three separate phases: a local cloak pickup and versioned checkpoint, an ordinary primary-weapon interaction followed by a local blue-key pickup and versioned checkpoint, and a cold restore from that second checkpoint without advancing a frame or writing a new save.

This is an assisted-local owner. Its declared view and position assists establish controlled local approaches, not connected travel from the original spawn through the level. It does not claim a full level clear, boss defeat, or exit.

## Lineage and checks

The owner consumes the spawn owner's earned, immutable checkpoint receipt. Each phase starts in a fresh process from the immediately preceding versioned save. The CMake runner requires the original qualified preview binary through the existing engine input; it does not build or substitute a new engine. The binary identity pin and existing per-phase time budgets remain unchanged.

Canonical asset-free JSON templates define the actions. The runner binds each copied input save by hash, assigns a distinct receipt nonce, and checks each phase's receipt, executed-plan hash, engine hash, trace, final state, and versioned output save. The contract rejects aliases as substitutes for immutable versioned saves and requires the intended ordinary pickup and weapon effects rather than accepting setup assists alone. The separate cold phase requires no frame and no new checkpoint. Its state comparison preserves persisted fields and permits only documented process-local resets of diagnostic counters and observations.

## Registration and evidence boundary

The native test is registered with the spawn-owner fixture dependency and serial execution. Its asset-free contract is added to the public test catalog and omission/skip policy checks. The public GPU/audio catalog passed 59/59, and the asset-free contract passed 120 forgery controls. Native qualification evidence is retained privately and is not included in this plan. No private assets, saves, traces, or derived measurements are checked in.
