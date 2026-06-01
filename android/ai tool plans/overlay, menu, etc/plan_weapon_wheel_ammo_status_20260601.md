# Weapon Wheel Ammo Status Design

Goal: show useful ammo status in the closed weapon wheel triggers, centralize ammo
status cutoffs, and make D2 paired secondary wheel slots display the weapon that
the slot will actually select.

## Current Shape

- `MainActivity.nativeGetWeaponState()` returns one `WeaponState` snapshot with
  flags, ammo, max ammo, current primary, current secondary, current bomb, laser
  level, and D2 `Primary_last_was_super` / `Secondary_last_was_super` flags.
- `WeaponWheelLabels.kt` owns current weapon labels and D2 paired-slot label
  prediction.
- `TouchOverlayView.drawWeaponWheel()` draws open wheel ammo directly with local
  rules and a local `ammoColor(pct, eff)` gradient.
- Closed radial triggers currently only show `rm.quiescentLabel`.

## Design

### 1. Add A Shared Ammo Status Helper

Add `android/app/src/main/java/com/dxxredux/app/WeaponAmmoStatus.kt`.

This file should be the single easy-to-find Android UI source for D1 and D2
weapon status thresholds:

```kotlin
internal const val LASER_STATUS_GREEN_ENERGY = 70
internal const val LASER_STATUS_YELLOW_ENERGY = 30
internal const val VULCAN_STATUS_GREEN_ROUNDS = 5000
internal const val VULCAN_STATUS_YELLOW_ROUNDS = 2000
internal const val DEFAULT_AMMO_STATUS_GREEN_FRACTION = 0.50f
internal const val DEFAULT_AMMO_STATUS_YELLOW_FRACTION = 0.25f
```

Use strictly greater-than comparisons for green and yellow so the requested
cutoffs read as:

- laser: green when energy is `> 70`, yellow when `> 30`, red otherwise
- vulcan or gauss: green when rounds are `> 5000`, yellow when `> 2000`, red otherwise
- all other ammo weapons: green when ammo/max is `> 0.50`, yellow when `> 0.25`,
  red otherwise

The default ammo fractions are the design proposal for the unspecified
missile/bomb cutoffs. They replace the current open-wheel continuous gradient
with explicit status buckets.

The helper should expose a small pure-data result:

```kotlin
internal enum class AmmoStatusColor { GREEN, YELLOW, RED }

internal data class WeaponAmmoStatus(
    val color: AmmoStatusColor,
    val countText: String?,
)
```

`countText` is null for laser and vulcan/gauss, because those counts already
exist elsewhere on the HUD. It is `"xN"` for missiles, bombs, and other counted
weapons.

### 2. Resolve A Weapon Index Before Rendering

Introduce a pure helper in `WeaponWheelLabels.kt`, probably named
`weaponWheelSlotSelection()` or `resolveWheelSlotWeaponIndex()`, that returns
the concrete weapon index a slot represents for a given snapshot.

For D1:

- primary slot `0..4` resolves to the same index
- secondary slot `0..4` resolves to the same index

For D2 primary and secondary:

- if the current weapon is the base or super member of this slot, resolve the
  other member when it exists and is usable, matching the in-game toggle mental
  model
- if the current weapon is not in this slot, prefer `*_last_was_super[slot]`,
  then fall back to the other member
- for secondary weapons, usability means owned and ammo count greater than zero
- for primary weapons, usability means owned; laser/super-laser and vulcan/gauss
  special ammo status is handled by the status helper
- if neither member is usable but one is owned, return the owned member so the
  wheel can still show a red/zero state rather than silently hiding useful
  information

Then make both `weaponWheelSlotLabel()` and open-wheel ammo rendering consume
that resolved index. This fixes the current D2 secondary bug where a paired slot
can show base ammo even when the selected wheel action will choose the super
weapon, or vice versa.

### 3. Separate Current Weapon Presentation From Slot Presentation

Add two presentation helpers:

```kotlin
internal data class WeaponWheelPresentation(
    val label: String,
    val ammoStatus: WeaponAmmoStatus?,
)

internal fun weaponWheelCurrentPresentation(...)
internal fun weaponWheelSlotPresentation(...)
```

`weaponWheelCurrentPresentation()` is for the closed trigger. It must always use
`currentPrimary` or `currentSecondary`, not the slot toggle prediction. This is
what makes the closed state show "Flash xN" while Flash is selected, even if
opening the wheel should show Concussion as the next slot target.

`weaponWheelSlotPresentation()` is for open wheel segments. It uses the
resolved target index from step 2, so the displayed name, count, and color all
refer to the weapon the segment will request.

### 4. Closed Trigger Rendering

Extend `RadialMenuState` or the draw path to carry a `WeaponWheelPresentation`
for `PriWpn` and `SecWpn` while closed.

Suggested visual treatment:

- keep the existing weapon name centered in the small circle
- draw a small colored status dot or lower arc inside the ring
- draw `xN` beneath the label only when `countText` is not null
- for laser and vulcan/gauss, show only the colored status indicator

This avoids cramming three lines into the smallest trigger when the count is not
needed.

### 5. Open Wheel Rendering

Replace local ammo math in `drawWeaponWheel()` with
`weaponWheelSlotPresentation()`.

- Segment label comes from `presentation.label`
- Segment count text comes from `presentation.ammoStatus?.countText`
- Pie/dot color comes from `presentation.ammoStatus?.color`
- Remove or replace `ammoColor(pct, eff)` so the open wheel and closed trigger
  share the same green/yellow/red thresholds

The wheel can still draw a small pie fill if desired, but its color should come
from the centralized status bucket.

### 6. Tests

Extend `WeaponWheelLabelTest.kt` or add `WeaponAmmoStatusTest.kt`.

Cover:

- laser status: 71 green, 70 yellow, 31 yellow, 30 red
- vulcan/gauss status: 5001 green, 5000 yellow, 2001 yellow, 2000 red
- counted secondary status uses centralized fractions and returns count text
- D2 secondary slot target with Flash and Concussion both available:
  - if neither is selected and `lastWasSuper` prefers Flash, slot shows Flash
    and Flash ammo
  - if Flash is selected, slot shows Concussion and Concussion ammo
- closed current secondary presentation still shows Flash and Flash ammo while
  Flash is currently selected
- D1 labels and counts keep resolving through the same helper path

### 7. Validation

- Run focused unit tests for weapon wheel/status helpers.
- Run Kotlin formatting for touched files.
- Run an Android debug native build only if JNI shape changes. The preferred
  implementation above should not require JNI changes.

## Implementation Notes

- Keep this Android-side unless a later implementation uncovers a mismatch with
  native `do_weapon_select()`. The existing JNI snapshot already contains the
  state needed for the design.
- Avoid adding separate D1 and D2 threshold files. D1/D2 differences should be
  data inputs to one helper.
- Keep the closed trigger compact. The useful signal is current weapon name,
  color, and count only when the count is not already available elsewhere.

## Status

- [x] Code paths inspected
- [x] Design written
- [x] Implement shared status helper
- [x] Update D2 paired-slot target resolution
- [x] Update closed trigger rendering
- [x] Update open wheel rendering
- [x] Add unit tests
- [x] Run focused validation

## Implementation Result

- Added `WeaponAmmoStatus.kt` as the shared Android-side source for laser,
  vulcan/gauss, and counted-ammo status thresholds
- Extended `nativeGetWeaponState()` and `WeaponState` with current energy so
  laser status can use the requested energy cutoffs
- Split current weapon presentation from open wheel slot presentation so the
  closed `PriWpn` / `SecWpn` trigger shows the actually selected weapon, while
  open D2 paired slots show the weapon the slot action targets
- Replaced open-wheel local ammo color math with the shared green/yellow/red
  status helper and added the same status dot plus counted ammo text to the
  closed weapon triggers
- Validation passed:
  - `android/run-code-quality.ps1 -Fix -Paths ...`
  - `gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.WeaponWheelLabelTest --no-daemon`
  - `gradlew.bat :app:externalNativeBuildDebug --no-daemon`
