# Half-Axis Trigger UI Overhaul

## Problem
When assigning a function to an analog trigger (LT/RT), the `ButtonFunctionPickerDialog`
shows two sections: "Axis Functions" (Slide U/D, Pitch U/D, etc.) and "Button Functions"
(Fire Primary, Accelerate, etc.).

Issues:
1. "Axis Functions" are bidirectional names like "Slide U/D" -- but a trigger is a
   single-direction half-axis. The user has no way to say "LT = Slide Up only".
2. One axis function can only be assigned to one trigger. You can't put "Slide Up" on
   RT AND "Slide Down" on LT because `assignAxisFunction` deduplicates.
3. The half-axis combiner infrastructure already exists in `buildJoyPairs()` and
   `MainActivity.onGenericMotionEvent()` but is driven by button-function names
   ("Slide Up", "Slide Down") in `HALF_AXIS_MAP`. Assigning axis functions bypasses it.

## Solution
Replace the "Axis Functions" section in the trigger picker with direction-specific
half-axis options that use readable direction names. Also update the stick picker's
axis-as-buttons labels: "Left (neg)"/"Right (pos)" etc. should use "(single direction)"
instead of "(pos)"/"(neg)" -- e.g. "Up (single direction)" instead of "Up (neg)".

### New Trigger Picker Options

Instead of:
```
Axis Functions
  o Pitch U/D
  o Turn L/R
  o Slide L/R
  o Slide U/D
  o Bank L/R
  o Throttle
Button Functions
  o Fire Primary
  ...
```

Show:
```
Single-Direction Axis
  o Pitch Up
  o Pitch Down
  o Turn Left
  o Turn Right
  o Slide Left
  o Slide Right
  o Slide Up
  o Slide Down
  o Bank Left
  o Bank Right
  o Throttle Forward
  o Throttle Reverse
Button Functions
  o Fire Primary
  ...
```

These map to the same underlying button-function labels that `HALF_AXIS_MAP` already
handles: "Slide Up", "Slide Down", "Accelerate", "Reverse", etc. But we add new
half-axis entries for functions not yet in HALF_AXIS_MAP: Pitch, Turn, Bank.

### Key constant: TRIGGER_HALF_AXIS_OPTIONS

A new ordered list of direction-specific half-axis options for triggers:
```kotlin
private val TRIGGER_HALF_AXIS_OPTIONS = linkedMapOf(
    "Pitch Up" to Pair("Pitch U/D", false),     // negative = up in kconfig
    "Pitch Down" to Pair("Pitch U/D", true),
    "Turn Left" to Pair("Turn L/R", false),
    "Turn Right" to Pair("Turn L/R", true),
    "Slide Left" to Pair("Slide L/R", false),
    "Slide Right" to Pair("Slide L/R", true),
    "Slide Up" to Pair("Slide U/D", true),
    "Slide Down" to Pair("Slide U/D", false),
    "Bank Left" to Pair("Bank L/R", false),
    "Bank Right" to Pair("Bank L/R", true),
    "Throttle Forward" to Pair("Throttle", false),
    "Throttle Reverse" to Pair("Throttle", true),
)
```

Note: "Slide Up/Down" and "Accelerate/Reverse" already exist as button functions in
BUTTON_KC_INDEX. We use the same names from HALF_AXIS_MAP for those (existing ones
keep using "Slide Up", "Slide Down", "Accelerate", "Reverse"). For Pitch/Turn/Bank
we add new entries to HALF_AXIS_MAP.

### Implementation Steps

1. **Expand HALF_AXIS_MAP** to include Pitch, Turn, and Bank directions:
   - "Pitch Up" -> ("Pitch U/D", false)   (negative half = pitch up in kconfig)
   - "Pitch Down" -> ("Pitch U/D", true)
   - "Turn Left" -> ("Turn L/R", false)
   - "Turn Right" -> ("Turn L/R", true)
   - "Bank Left" -> ("Bank L/R", false)
   - "Bank Right" -> ("Bank L/R", true)
   - "Throttle Forward" -> ("Throttle", false)  (rename from "Accelerate")
   - "Throttle Reverse" -> ("Throttle", true)    (rename from "Reverse")
   Keep existing "Slide Up/Down/Left/Right" entries.
   Keep existing "Accelerate"/"Reverse" entries as aliases (they're also button funcs).

2. **Add TRIGGER_HALF_AXIS_OPTIONS** ordered list for the picker UI.

3. **Modify ButtonFunctionPickerDialog**:
   - When `axisFunctions` is non-empty (trigger mode), replace "Axis Functions" header
     with "Single-Direction Axis" header
   - Show TRIGGER_HALF_AXIS_OPTIONS as the axis options (not AXIS_FUNCTIONS)
   - When user selects one, store the half-axis label (e.g. "Slide Up") as the binding
   - This naturally flows through existing HALF_AXIS_MAP -> buildJoyPairs() -> combiner

4. **Remove assignAxisFunction dedup for triggers**: When a trigger picks a half-axis
   option, use `assignButtonFunction` (or a new `assignHalfAxisFunction`) instead of
   `assignAxisFunction`. This allows LT="Slide Down" and RT="Slide Up" simultaneously
   because they're different function labels.

5. **Update canvas labels**: Triggers with half-axis bindings should show the direction
   abbreviation (e.g. "Sld^" for "Slide Up"). Add entries to abbreviate().

6. **Update `isAxisFunc` detection**: The dialog's radio button selection logic
   currently checks `currentFunc in AXIS_KC_INDEX`. Half-axis labels like "Slide Up"
   aren't in AXIS_KC_INDEX. Adjust so half-axis options are detected separately.

### Files Changed
- `android/app/src/main/java/com/dxxredux/app/ControllerConfigPage.kt`
  - Expand HALF_AXIS_MAP
  - Add TRIGGER_HALF_AXIS_OPTIONS
  - Modify ButtonFunctionPickerDialog axis section
  - Modify trigger assignment flow to use button assignment path
  - Add abbreviations for new half-axis labels
- No C code changes needed (combiner infrastructure already works)
- No changes to d1/ or d2/ source

### Sign Convention Notes (kconfig.c axis handling)
From kconfig_read_controls():
- Pitch: negative axis = pitch up (nose up), positive = pitch down
- Turn: negative = turn left, positive = turn right  
- Slide L/R: negative = slide left, positive = slide right
- Slide U/D: negative = slide down, positive = slide up (INVERTED from screen coords)
- Bank L/R: negative = bank left, positive = bank right
- Throttle: negative = accelerate (forward), positive = reverse (backward)

The `isPositive` flag in HALF_AXIS_MAP means "this trigger adds to the positive half
of the combined virtual axis". The combiner computes: value = pos - neg. The kconfig
axis assignment + invert flag then maps this to the game function.
