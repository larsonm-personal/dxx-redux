# Consolidated custom button visibility audit

- [x] Inventory custom button views and their visibility owners
- [x] Identify every control driven by the central overlay poll
- [x] Compare the wait-screen consolidation with the dormancy regression
- [x] Check regression coverage and record affected screens

## Findings

The initial-resume bug disabled the central Kotlin overlay poll on a fresh activity.
It affected both D1 and D2 and could be cleared accidentally by a later stop/resume cycle.

Controls that could disappear or remain stale:

- `SkipButtonView` as `SKIP` for movies, briefings, end-level sequences, and post-level matrices
- `SkipButtonView` as `CONTINUE` for death waits
- `SkipButtonView` as `NEXT` for level-complete menus
- `SkipButtonView` as `BACK` for save/load slot menus
- `SkipButtonView` as `Skip every launch` during the title/intro sequence
- `StartGameButtonView` on host player selection
- `AcceptJoinButtonView` for mid-game join approval
- The gameplay touch interface and its admin controls

Related state that could remain stale:

- Standalone exit-button visibility
- Joystick enablement associated with touch-overlay activation
- Automatic network-events overlay visibility
- Demo-recording state and music label shown by the touch overlay

Not affected through this mechanism:

- Native menu items such as the player-select `OK` item
- Whole-screen native touch/controller advancement admitted by `android_screen_advance`
- Launcher Compose buttons
- Loading progress UI
- Independently started coop stats and warp polling, except where central policy later hides them

## Coverage

- The new initial-resume central-poll assertion covers the common visibility owner
- Native automation covers several screen-advance actions but does not verify Kotlin view visibility or labels
- A future UI-state introspection surface should expose custom button visibility and label values for direct assertions
