# Binding Limit Enforcement: 2 Touch + 2 Physical Per Action

## Problem

The state-bit system in kconfig.c uses per-binding-slot bits to track which inputs are active.
Each action can have up to 2 joystick/gamepad entries (STATE_BIT3, STATE_BIT4) in kc_joystick[].
Touch buttons currently piggyback on the SAME state bits as physical gamepad buttons -- a touch
button at kc_joystick index N fires SDL button (N + 128), which matches entry N and sets/clears
the same STATE_BIT3 or STATE_BIT4 as a physical button on that entry.

This means:
- If both a touch button and a physical button are bound to the same action (same kc entry),
  they share a state bit. Releasing one clears the bit even if the other is still held.
- Two touch buttons with the same binding fire the same SDL button number. Releasing one
  clears the other's state.
- There's no enforcement in any editor preventing excessive or conflicting bindings.

## Design

### State Bit Layout (ubyte, 8 bits)

| Bit | Value | Source                |
|-----|-------|-----------------------|
| 1   | 1     | Keyboard primary      |
| 2   | 2     | Keyboard secondary    |
| 3   | 4     | Gamepad primary       |
| 4   | 8     | Gamepad secondary     |
| 5   | 16    | Mouse                 |
| 6   | 32    | Touch primary (NEW)   |
| 7   | 64    | Touch secondary (NEW) |

The action fires if ANY bit is set: `if (Controls.btn_X_state)`.
Each input source gets its own bit, so press/release of one source never affects another.

### Touch Offset Scheme

Two touch button offsets to distinguish primary vs secondary touch sources:

- `TOUCH_BTN_OFFSET   = 128` (existing) -- first touch button per action
- `TOUCH_BTN_OFFSET_2 = 256` (new) -- second touch button per action

SDL button IDs: standard button at kc index N fires:
- Touch primary:   N + 128 (range 128-183)
- Touch secondary: N + 256 (range 256-311)
- Physical:        whatever SDL button ID the gamepad reports (range 0-25 typically)

All well within the 1024-element button_state[] array.

### C-side Changes (d2/main/kconfig.c and d1/main/kconfig.c)

1. Add `STATE_BIT6 = 32` and `STATE_BIT7 = 64`
2. Add `TOUCH_BTN_OFFSET_2 = 256`
3. Update `JOY_BTN_MATCHES` macro to also check `(idx) + TOUCH_BTN_OFFSET_2 == (btn)`
4. In kconfig_read_controls, joystick button handler:
   - When the matched button is a touch event (>= TOUCH_BTN_OFFSET), remap the state bit:
     - btn >= TOUCH_BTN_OFFSET_2: use STATE_BIT7
     - btn >= TOUCH_BTN_OFFSET: use STATE_BIT6
   - Physical buttons keep their original state_bit (BIT3 or BIT4)

### Kotlin-side Changes

**TouchBindings.kt:**
- Add `TOUCH_BTN_OFFSET_2 = 256`
- Add `MAX_TOUCH_BINDINGS_PER_ACTION = 2`

**TouchOverlayView.kt:**
- Add runtime primary/secondary tracking via a ref-count map:
  ```
  touchPrimaryRefs: MutableMap<Int, Int>  // binding -> count of primary-offset holds
  ```
- Add helper `dispatchTouchButton(binding, pressed)`:
  - Meta actions (>= 1000): pass through unchanged
  - Standard buttons: on press, if refs == 0, use TOUCH_BTN_OFFSET (primary), increment ref;
    else use TOUCH_BTN_OFFSET_2 (secondary). On release: if refs > 0, use primary, decrement;
    else use secondary. The callback receives the full SDL button ID (binding + offset).
- Replace all `buttonCallback?.invoke(binding, pressed)` calls for standard buttons
  with `dispatchTouchButton(binding, pressed)`

**MainActivity.kt:**
- Update buttonCallback: don't add TOUCH_BTN_OFFSET (it's already included in the button ID)
  ```kotlin
  touchOverlay.buttonCallback = { button, pressed ->
      if (button >= TouchBindings.META_ACTION_OFFSET) {
          NativeMetaActions.nativeMetaAction(button, ...)
      } else {
          nativeJoystickButton(button, ...)  // offset already applied
      }
  }
  ```

**TouchEditorPage.kt:**
- In the binding picker, count how many controls in the layout use each binding value
  (across buttons, stick button-mode directions, stick double-tap, radial segments)
- Exclude the currently-edited control from the count
- When a binding reaches MAX_TOUCH_BINDINGS_PER_ACTION (2), disable it in the picker
  with a "(limit reached)" label

### Limits Summary

| Source         | Max per action | Enforced by            |
|----------------|---------------|------------------------|
| Keyboard       | 2             | kc_keyboard[] structure (2 entries per action) |
| Gamepad buttons | 2            | kc_joystick[] structure (2 entries per action) |
| Mouse          | 1             | kc_mouse[] structure (1 entry per action) |
| Touch buttons  | 2             | Editor enforcement + TOUCH_BTN_OFFSET/OFFSET_2 |
| Total          | 7             | All independent state bits in ubyte |

### Correctness

When two touch buttons for the same action are BOTH held:
- First press sets BIT6 (primary), second press sets BIT7 (secondary)
- Releasing either clears only its own bit; the action stays active via the other
- When both are released, both bits are cleared; action stops
- The ref-count approach ensures press/release pairs balance even if releases happen
  in a different order than presses (because the game only checks "any bit set")

### Files Changed

- d2/main/kconfig.c (STATE_BIT6/7, TOUCH_BTN_OFFSET_2, JOY_BTN_MATCHES, kconfig_read_controls)
- d1/main/kconfig.c (same changes)
- android/.../TouchBindings.kt (TOUCH_BTN_OFFSET_2, MAX_TOUCH_BINDINGS_PER_ACTION)
- android/.../TouchOverlayView.kt (dispatchTouchButton, replace buttonCallback calls)
- android/.../TouchControl.kt (bindingUsageCounts method on TouchLayout)
- android/.../MainActivity.kt (callback update)
- android/.../TouchEditorPage.kt (binding picker limit enforcement)

## Status

All phases COMPLETE. Build verified (assembleDebug) on 2025-06-26.
