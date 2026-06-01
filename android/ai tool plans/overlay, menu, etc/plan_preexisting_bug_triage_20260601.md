# Plan: Pre-existing Bug Triage

## Status

Drafted.

## Scope

This plan covers the pre-existing top-of-file bugs in `android/outstanding_bugs.md`
that were present before the multiplayer controller-navigation fix pass:

- weapon wheel quiescent ammo/status display and D2 paired weapon prediction
- multiplayer controller-navigation default focus, lazy highlight reveal, mission picker focus trap, and restore focus snap

`android/outstanding_bugs.md` says not to edit it, so closure should happen by
validating behavior and then updating whatever external tracking flow owns that
file.

## Bug 1: Weapon Wheel Ammo Status

### Detailed Behavior

- The closed primary and secondary weapon wheel buttons need to describe the
  currently selected weapon, not just the generic radial control.
- The closed state should show ammo health:
  - lasers: color only, based on current player energy
  - vulcan or gauss: color only, based on displayed rounds
  - missiles, bombs, and other counted weapons: color plus count text
- The open wheel should use the same status thresholds as the closed state.
- D2 paired slots are tricky because one wheel segment can represent two
  in-game weapons, such as Concussion and Flash or Homing and Guided. The open
  wheel must show the weapon that pressing that segment will actually select.
- The closed state has the opposite requirement: it must show the actually
  selected weapon, even when opening the wheel would show the paired alternate.

### Likely Root Causes

- The overlay previously stored only `quiescentLabel`, so the closed button had
  no place to carry status color or count.
- Ammo coloring was local draw-time math in the open wheel, not a shared policy.
- D2 paired-slot label prediction and current-selected labels were mixed
  together, so a helper useful for open wheel targeting could produce the wrong
  answer for the closed button.
- The JNI weapon-state snapshot did not expose current energy, so laser status
  could not be computed from the requested thresholds.

### Plan

- [x] Keep all status thresholds in a small shared Android helper.
- [x] Extend the weapon-state snapshot with current energy.
- [x] Split current-weapon presentation from slot-target presentation.
- [x] Use the presentation helpers in closed and open wheel drawing.
- [x] Add unit tests for thresholds and D2 paired-slot prediction.
- [ ] Re-run focused validation from the existing weapon-wheel plan.
- [ ] Do one emulator or device visual check for closed primary/secondary
      buttons and a D2 paired secondary slot.
- [ ] If validation passes, mark the external tracker for this bug resolved.

## Bug 2: Multiplayer Controller Navigation

### Detailed Behavior

- Controller-only and touch-plus-controller devices should share the same
  logical initial selection.
- On touch-plus-controller pages, the green highlight should be hidden on page
  entry, then revealed on the first D-pad navigation for that page.
- On the base multiplayer screen, disconnected initial focus should be Connect,
  not the server URL field or LAN.
- If the first D-pad action is Right from the base screen, the visible highlight
  should appear on LAN.
- The Host LAN Game mission selector must be navigable into and away from.
- Activating Restore or Start fresh must not snap D-pad focus back to Stop
  Hosting or any other default page control.

### Likely Root Causes

- The initial focus policy explicitly picked LAN for the disconnected browser.
- Compose text fields and disabled fields were participating badly in D-pad
  focus traversal, which made focus land on the URL field or get stuck on the
  mission display.
- Requesting focus forced keyboard input mode immediately, making focus visible
  even on pages opened by touch.
- Some pages re-requested initial focus when hosting or discovery state changed,
  so activating a control could cause focus to jump back to the top action.
- Selected restore/fresh buttons were being swapped between `Button` and
  `OutlinedButton` without preserving focus across the recomposition.

### Plan

- [x] Change disconnected browser logical focus to Connect.
- [x] Add lazy focus reveal for multiplayer pages.
- [x] Set explicit Connect to LAN horizontal traversal.
- [x] Replace the disabled mission field with a focusable button.
- [x] Avoid re-requesting initial LAN focus on hosting/discovery state changes.
- [x] Preserve focus after Restore or Start fresh activation in LAN and online
      lobby restore offers.
- [x] Add or update focused policy tests.
- [x] Run focused unit tests and code quality.
- [ ] Do one on-device or emulator smoke test for the exact D-pad sequences.
