# First Strike live replanning

1. Diagnose why the live snapshot after level 21's yellow key cannot reproduce the initial complete route
2. Fix shared planning semantics and add a focused simulation regression
3. Verify all four core missions without losing existing completions
4. Keep D1 effect ownership and lava flicker separate from navigation changes

## Cause and fix

- Unrestricted trials reported red key missing even before the actor collected yellow
- The shared secret-area/live metadata adapter bounded object slots by num_objects, the live count, instead of Highest_object_index + 1
- Simulation removal of ordinary robots makes the array sparse; higher-slot keys and other targets were silently omitted
- Corrected adapter object scans and bounds, rejecting empty slots as invalid segments
- Equal-progress diagnostic results now prefer the unrestricted attempt over an excluded-key trial

## Validation

- Focused FirstStrike level 21 integration test completes identically twice
- 88-level core run: FirstStrike 30/30, Counterstrike 28/30, Castaway 2/10, Obsidian 11/18; no lost successes
- Obsidian 10 changes from timeout to unsupported activation after switch 8; it remains unresolved
- All 47 D2 native tests and scoped code-quality checks pass
- Evidence: android/temp/firststrike21_diagnosis_20260906, android/temp/live_objects_core_20260906, android/temp/live_objects_final_20260906
- Lava animation ownership remains a separate unresolved issue; no texture changes are included
