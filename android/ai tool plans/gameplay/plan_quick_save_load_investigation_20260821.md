# Fixed-slot quick save and load

## Decisions and behavior

- Use a fixed slot for quick save and quick load instead of the last manually selected slot
- Use the fourth visible slot, which is zero-based engine slot index 3
- Leave engine slot 4 and the existing periodic-autosave allocation unchanged
- Quick-save description must be `[quick] level N`, using the current level number for `N`
- Quick save happens immediately on button press with no engine save menu or Kotlin confirmation
- Quick load first opens a Kotlin overlay prompt reading `Load: yes/no`
  - `Yes` closes the overlay and requests an immediate restore from the fixed quick slot on the game thread
  - `No` closes the overlay without restoring
  - A tap anywhere outside the prompt dismisses it without restoring
  - The game is paused for the entire time the prompt is open
  - The normal Kotlin paused-overlay indicator is visible while the prompt is open
  - Reuse the existing overlay pause ownership and balancing so dismiss, No, Yes, lifecycle changes, and failed loads cannot leave time paused or resume a pause owned elsewhere
- If the fixed quick slot does not contain a valid save for the active game/player/mission context, do not restore and give the user concise feedback
- Apply the native save/load behavior consistently to D1 and D2 while keeping new shared Android logic centralized

## Completed investigation

- [x] Trace save and restore commands and key bindings in D1
- [x] Trace save and restore commands and key bindings in D2
- [x] Compare any immediate quick paths with Redux's current slot-prefill behavior
- [x] Report the relevant functions, constraints, and likely reusable entry points

## Implementation phases

- [x] Resolve slot 4 numbering: use engine index 3 and leave periodic autosave unchanged
- [x] Add shared native fixed-slot quick-save and guarded quick-load requests, descriptions, validation, and logging
- [x] Wire touch and controller Quick Save to the immediate native save path
- [x] Add the dismissible Kotlin `Load: yes/no` overlay with pause ownership and paused-indicator integration
- [x] Wire confirmed Quick Load to the game-thread restore request and handle missing/invalid saves
- [x] Add Kotlin tests for quick-action press routing and shared overlay pause ownership
- [x] Extend D1 and D2 automation coverage for the fixed-slot save and restore round trip
- [x] Run scoped code quality, relevant unit/integration tests, and Android build verification; fix implementation issues until clean

## Styled quick-load prompt follow-up

- [x] Identify the existing pause-overlay card colors, border, typography, and rounded-corner proportions
- [x] Replace the platform alert with a transparent custom dialog matching the pause-overlay card
- [x] Limit visible copy to `load quick save?`, `yes`, and `no`
- [x] Preserve outside-tap cancellation, pause ownership, paused-indicator visibility, and controller focus
- [x] Run focused Kotlin tests, scoped code quality, and Android build verification

## Admin Save and Load menu separation follow-up

- [x] Restore the admin overlay `Save` item to the engine save-slot dialog
- [x] Restore the admin overlay `Load` item to the engine load-slot dialog
- [x] Keep fixed-slot quick save/load exclusive to explicit touch and controller bindings
- [x] Run focused Kotlin tests, scoped code quality, and Android build verification
