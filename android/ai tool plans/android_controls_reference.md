# Touch controls reference

- Verify the bundled Claw layout and bindings
- Capture the actual Android game overlay using the emulator
- Create android/android_controls.svg with embedded screenshot and editable yellow callouts
- Render and visually verify labels and connector placement

Completed: captured the bundled default overlay in D2; created a self-contained SVG; checked XML and browser rendering. Labels verified against claw.json, TouchBindings.kt, and gesture handling.

Follow-up: align Claw control centers and button rows with small position changes, verify only geometry changed, capture the actual updated overlay, and refresh SVG leaders.

Alignment completed: changed only x/y and Map/Flare size fields; scoped quality checks passed; emulator launch passed with temporary updated active layout; refreshed and visually checked embedded screenshot and leaders. Removed temporary active-layout override afterward. No native build needed for JSON/SVG-only changes.

Preset cleanup: renamed bundled Claw to touch_default.json (Touch Default in the picker), removed Simple/Advanced, changed roll/vertical double-tap to Drop Bomb, and reordered the SVG look instructions. Updated the default-selection test; JSON catalog and SVG render verified.
