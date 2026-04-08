# Rejoin Sync Diagnostics v4

## Status: READY FOR TESTING

## Problem
Multiplayer coop rejoin consistently fails with "sync failed" error.
verify_objects sees PLAYER=1, GHOST=0 despite MPDIAG showing ghosts placed at indices 16, 45, 46, 66, 90 during object sync.
After special_reset_objects(), Highest_object_index=24 -- objects at high indices are gone.

## What was added (both D1 and D2)

### Client-side (read_object_packet)
1. Per-packet header: `nobj=N mode=M object_count=C` -- tracks packet flow and mode state
2. INIT marker log: confirms init_objects() called and when
3. Every object placement: `type=T id=I at idx=N owner=O remote=R seg=S` (not just PLAYER/GHOST)
4. Overwrite warning: logs if placing into a slot that is already non-NONE/non-PLAYER
5. MODE0 branch log: logs if the obj_allocate path is taken (should never happen if all owners are -1)
6. PRE-RESET dump: before special_reset_objects(), dumps ALL non-NONE objects with type/id/seg
7. POST-RESET summary: Highest_object_index and num_objects after reset

### Host-side (send_objects)
1. INIT log: player_num and Highest_object_index at start
2. Every object sent: `obj[N] type=T id=I owner=O remote=R seg=S`

## What to look for in logs
- Do PRE-RESET objects match what was placed? If ghosts at 45/46/66/90 are missing from PRE-RESET dump, something overwrote them between placement and end-marker
- Is init_objects() called more than once? That would clear placed objects
- Does MODE0 branch fire? That would trigger an early special_reset_objects mid-stream
- Do any WARNING overwrite messages appear?
- Compare host send list vs client receive list for count/index mismatches

## Build
Standard Android debug build. Changes are in MPDIAG macros (compile on all platforms).
