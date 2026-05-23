# Thief checkpoint state audit 2026-05-03

## Plan
- [x] Inventory thief runtime globals and state transitions in d2/main/escort.c and d2/main/collide.c
- [x] Compare thief state usage against what savegame + checkpoint metadata persist
- [x] Verify whether existing escort checkpoint schema includes thief state
- [x] Produce recommendation for minimal deterministic parity with guidebot checkpoint restore

## Notes
- Full savegame AI data stores ai_local arrays and object AI data
- Input demo checkpoint metadata stores escort and buddy fields only
- Thief ring index is used in behavior and not persisted
