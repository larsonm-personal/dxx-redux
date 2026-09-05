# Saturn and outer simulation timeouts

1. [x] Align CD HOG staging with metadata and reproduce Destination Saturn 15
2. [x] Reproduce the six outer timeouts with saved assets and inspect where execution stalls
3. [x] Verify fixes with focused runs and preserve controlled failure reporting

## Findings and fixes

- Saturn's CD-specific descent.hog was staged only under missions, while the D1-in-D2 loader searches the virtual root for that base HOG
- CD simulation staging now exposes HOGs in both locations, matching metadata staging
- All six outer timeouts occurred after reactor destruction
- Live debugger inspection found loops in obj_detach_all/obj_detach_one, with a fireball's attachment pointing to a slot already reused for a weapon
- The dead-reactor effects reference survived object deletion, allowing fireballs to be attached to a freed or reused slot
- Both engines now clear Dead_controlcen_object_num when deleting that object, before its slot can be reused
- No route constraints, door behavior, or timeout budgets were relaxed

## Verification

- D1 and D2 Windows builds passed
- Scoped code quality passed
- All 47 D2 CTests passed
- Destination Saturn level 15 completed identically twice
- Each former outer-timeout case ran twice with identical results
- BRDECON, twisty, eris0001, shipport, and megaloma now finish ok
- neural-i now terminates normally with a deterministic route-progress timeout instead of hanging the process
- New integration tests cover Saturn and all six former outer timeouts, using temporary outputs without rewriting checked-in regression data
