# Touch multi-selector initial design

## Scope

Design a linear, drag-selected alternative to the existing touch radial menu for
guidebot, primary weapon, and secondary weapon controls. Identify how it fits the
current model and editor, and record decisions that remain underspecified. No
runtime implementation is part of this tranche.

## Plan

- [x] Inspect the touch control schema, layout persistence, radial-menu runtime,
  and editor placement/configuration flows
- [x] Propose the multi-selector geometry, live magnification, gesture state
  machine, commit/cancel behavior, and D2 alternate-row behavior
- [x] Recommend editor controls, defaults, placement constraints, migration, and
  test seams
- [x] List open product decisions with recommended initial answers
- [x] Mark this design tranche complete

## Feedback refinement plan

- [x] Confirm how existing Android direct actions cross onto the game thread and
  whether an exact weapon-selection hook already exists
- [x] Revise magnification, bounded scrolling, D2 fixed-tier rows, and inventory
  filtering based on product feedback
- [x] Replace resolved open questions and update implementation/test phases
- [x] Mark the feedback refinement complete

## Existing implementation findings

- `RadialMenuControl` already owns the concepts that should be shared: trigger
  position and size, opacity, ordered items, preset identity, optional center
  action, and haptic feedback.
- The `Guide`, `PriWpn`, and `SecWpn` IDs activate runtime behavior beyond their
  stored item lists. Guide items are filtered by game state. Weapon items get
  dynamic labels, inventory filtering, ammo state, and current-weapon trigger
  text.
- D2 weapon wheel items currently represent five paired weapon slots. Pressing a
  digit can select the preferred member of a pair or toggle to its partner. The
  current presentation helper predicts that next result rather than exposing two
  rows.
- Both engines already expose an exact internal
  `select_weapon(weapon_num, secondary_flag, print_message, wait_for_rearm)`
  function. It performs the actual state change, HUD feedback, rearm timing,
  multiplayer status update, D2 higher-tier memory update, and original demo
  recording. There is no Android/JNI request that selects a full weapon index
  directly.
- The existing Android direct-action pattern is suitable for that missing
  bridge. `NativeMetaActions` accepts UI-thread requests, parameterless actions
  use pending state, and `gamecntl.c` consumes them during `EVENT_IDLE` on the
  game thread. The input-based demo system also already has parameterized direct
  command recording/replay infrastructure.
- `SliderControl` already means an analog-axis slider. The new presentation
  should therefore be called `SCROLL_STRIP` in code even if the editor calls it
  "Slider".
- The runtime and editor currently use different wheel constants, and the editor
  radial hit target is much larger than its drawn trigger. New scroll-strip
  geometry should be centralized in pure helper functions and shared by both.
- Editor dragging currently moves the selected control without checking that the
  drag began on that control. A non-interactive extent rail requires an explicit
  drag-start hit on the selector trigger.
- The automation API is named `select_radial` and calls
  `automateRadialSelection`. It should keep working as a compatibility alias when
  the underlying control becomes a multi-selector.

## Recommended control model

Generalize the existing selector rather than building on `SliderControl`:

```text
MultiSelectorControl
  id, xPct, yPct
  presentation: WHEEL | SCROLL_STRIP
  triggerSizeMult, opacity, hapticFeedback
  items
  centerLabel, centerBinding       # wheel behavior
  wheelRingSizeMult               # wheel behavior
  stripOrientation                # HORIZONTAL by default
  stripDragSpanWidthPct           # total span, default 20
  stripLabelAngleDeg              # relative to cross-axis, default 0
  stripSelectedScale              # default 2.0
```

`items` should retain label, binding, binding type, icon, and weapon slot index.
The D2 paired rows are derived at runtime from the weapon preset and weapon
state. They are not duplicated into saved layout data.

For the least disruptive implementation, the Kotlin type and JSON key can first
remain `RadialMenuControl` and `radialMenus`, with the presentation and strip
fields added. A later naming-only refactor is optional. Existing records default
to `WHEEL`, so current layouts do not silently change behavior.

## Editor flow

1. Rename "Radial Menu" in the add dialog to "Multi-selector".
2. The next dialog asks for `Wheel` or `Scroll strip`.
3. Place the trigger, then use the normal properties panel to choose `Custom`,
   `Primary Weapons`, `Secondary Weapons`, or `Guide Bot`.
4. Allow presentation to be changed later without losing items or common
   settings.

Common properties remain preset, trigger size, opacity, items, and haptics.
Wheel-only properties remain ring size and center action. Scroll-strip properties
are orientation, drag span, label angle, and selected scale. The first version
can expose selected scale later and use the 2.0 default initially if panel space
is a concern.

The current custom-wheel limit of 12 items should not apply to a scroll strip.
A defensive serialization limit such as 32 is reasonable, but should not be a
normal editor UX limit.

## Editor geometry and placement

The trigger stays at `(xPct, yPct)`. The fixed center selection spot is also at
that point. Draw a faint rail through it, with end caps showing the maximum main
axis drag. The rail is preview-only and is never a hit target.

`stripDragSpanWidthPct` is defined as total endpoint-to-endpoint length, not
length on each side. At the default 20, the endpoints are 10 percent of the
screen width on either side of the trigger. A vertical strip still uses exactly
that pixel length, as requested.

```text
dragSpanPx = canvasWidth * stripDragSpanWidthPct / 100
halfSpanPx = dragSpanPx / 2
```

For a horizontal strip, clamp the trigger X to
`[halfSpanPx, canvasWidth - halfSpanPx]`. For a vertical strip, clamp trigger Y
to `[halfSpanPx, canvasHeight - halfSpanPx]`. Clamp the cross-axis coordinate by
the trigger radius. Use the actual overlay/editor canvas width after insets, not
raw physical display width.

Changing orientation, span, or trigger size immediately reclamps the trigger.
Dragging begins only when the pointer goes down inside the trigger. Starting on
the rail, endpoint marks, or the selected control's other preview decoration
must not move or select it.

For D2 paired weapons, add faint cross-axis threshold ticks in the editor so the
second gesture dimension is discoverable. These are also non-interactive.

## Runtime gesture state machine

Recommended first version is a single press, drag, release gesture like the
wheel:

1. `DOWN` inside the trigger opens the strip and captures that pointer.
2. The current weapon slot is initially centered for weapon presets. Other
   presets center the last item committed during this process lifetime, or the
   first visible item if none has been committed.
3. The gesture begins neutral. A touch and release with no movement does
   nothing, matching the current wheel's accidental-activation protection.
4. Main-axis movement scrolls the items under the fixed center marker. Content
   follows the finger, so dragging left reveals/selects items to the right.
5. Clamp at both rail endpoints. Reaching the left or right maximum selects the
   corresponding first or last item. There is no wraparound and no inertial
   fling in the initial version.
6. Crossing an item boundary changes the active item and performs one haptic
   click when enabled.
7. `UP` commits the centered enabled item only after movement beyond touch slop.
   Cross-axis movement that changes weapon row also arms the gesture.
8. `CANCEL` closes with no action.

Map each half of the configured drag span to all available items on that side of
the initial item. This guarantees that every item is reachable in one held
gesture even if long, rotated labels make the visual content wider than the
rail. It also makes the 20 percent setting describe finger travel consistently.

Keep the current multi-touch behavior: an open selector owns only its pointer,
so another finger can continue to operate a stick or fire button.

## Item layout and magnification

Angle is measured from the axis perpendicular to scrolling:

- Horizontal strip at 0 degrees: labels read vertically, top to bottom.
- Vertical strip at 0 degrees: labels read horizontally, left to right.
- At 90 degrees: labels run along the scroll axis, end to end.
- A signed range of -90 through 90 permits either lean direction. If only the
  requested direction is desired, the editor can initially expose 0 through 90.

Rotate the label and its selection card together. Draw the cards as slanted
parallelograms at intermediate angles. Measure every complete label, including
multiple lines, before positioning it. If `w` is text width, `h` is text height,
and `a` is the absolute cross-axis-relative angle from 0 through 90, its
unscaled projection along the scroll axis is:

```text
mainSpan = h * cos(a) + w * sin(a)
```

The cross-axis projection uses the complementary terms. Place neighboring cards
using half of each card's current projected span plus a fixed gap. Recompute
these spans with live scale so the enlarged center card pushes its neighbors
away instead of overlapping them.

Interpret "within half a space" as the active item's nearest-center cell. Scale
is 1 at the midpoint to a neighbor and rises continuously to
`stripSelectedScale` at the fixed center marker. Use a smoothstep curve so size
and displacement have zero-slope endpoints. Scale font, card width, and card
height together. At an exact boundary neither item receives full emphasis; the
active index changes there.

Clip or softly fade items at the rail endpoints. The selected center card is
drawn last so it remains visually dominant. Reuse weapon ammo counts and
green/yellow/red state from the wheel presentation layer.

## D2 higher-tier and lower-tier weapon rows

Paired weapon rows are special behavior for D2 weapon selectors. They do not use
the player's weapon sort/autoselect order or the `Primary_last_was_super` and
`Secondary_last_was_super` preference bits.

The engine already marks the pairing structurally with `SUPER_WEAPON == 5`:
indices 0 through 4 are the lower tier and their partners at 5 through 9 are the
higher tier. Super Laser at primary index 5 is the exception and is not a
separate useful selector choice.

Build the visible rows in canonical slot order:

- Primary main row: Laser, Gauss, Helix, Phoenix, Omega.
- Primary alternate row: Vulcan, Spreadfire, Plasma, Fusion.
- Secondary main row: Flash, Guided, Smart Mine, Mercury, Earthshaker.
- Secondary alternate row: Concussion, Homing, Proximity, Smart, Mega.
- D1 has one row only.

Filter each row independently before measuring or laying it out. Hide every
weapon the player does not own. Also hide every secondary weapon with zero ammo.
Do not reserve a blank card or scroll position for a hidden weapon. Owned primary
weapons remain visible even if their shared ammunition or current energy is
zero. Laser remains visible as the unpaired primary choice.

Because filtering can give the rows different item counts, retain the canonical
pair slot as metadata rather than as a blank visual column. On a cross-axis row
change, center the paired slot when it is visible in the destination row.
Otherwise center the nearest visible canonical slot. If the higher-tier row is
empty, open on the lower-tier row and disable row switching until a higher-tier
weapon becomes visible.

For a horizontal strip, vertical motion chooses the alternate row. For a
vertical strip, horizontal motion does so. Either sign should work, which keeps
the gesture usable near any screen edge. The sign only controls which side the
alternate row animates from.

Recommended threshold is the larger of 1.25 trigger diameters and 25 percent of
the configured drag span. Enter the alternate row at that distance and return to
the main row below 70 percent of that distance. This hysteresis prevents row
flicker. Animate both rows continuously with cross-axis displacement, and click
haptically once when the active row changes.

Selecting a row must select that exact full D1 or D2 weapon index, not merely its
pair slot. Add a parameterized Android hook instead of injecting one or more
digit keys:

```text
NativeMetaActions.nativeSelectWeaponExact(weaponClass, weaponIndex)
```

The JNI entry point must only validate and pack the request into one pending
integer so the weapon class and index cannot be observed from different
requests. A shared game-thread consumer decodes it during `EVENT_IDLE`, verifies
that gameplay and the player are valid, bounds-checks the index, then checks the
same visibility rule used by the strip: primary requires ownership; secondary
requires ownership and ammo. It finally calls the engine's existing
`select_weapon(index, secondary, 1, 1)` exactly once.

This is a new JNI bridge, but not a new engine selection mechanism. It should
follow the existing pending direct-action pattern and be shared by D1 and D2,
with only a small `EVENT_IDLE` call site in each `gamecntl.c`. Calling the engine
function on the JNI/UI thread is not safe.

Add an exact-weapon-selection direct command with weapon class and full index to
the input-based demo recorder and replay path. Record it immediately before the
game-thread call to `select_weapon`. The original Descent demo recorder is
already handled inside `select_weapon`, but the newer input-based recorder would
otherwise miss this UI-originated command.

## Special selector behavior to preserve

- Hide Guide in D1.
- Preserve the Guide secret-area item filter.
- Preserve Guide owner/locked trigger labels.
- Preserve weapon trigger label and ammo status for the actually selected
  weapon.
- Preserve the existing `select_radial` automation action as an alias. Add a
  presentation-neutral internal method such as `automateSelectorSelection`.
- Preserve cancel-on-overlay-disable, automap transition, and `ACTION_CANCEL`.

The locked Guide deploy action needs a linear safety equivalent. Recommended:
show `Deploy` only in the outer third at either rail end, with the middle two
thirds neutral. This matches the existing wheel's guarded outer-ring intent and
prevents a simple tap from spawning the Guide-Bot.

## Tests for an implementation tranche

- Pure geometry tests for horizontal and vertical rail endpoints, width-based
  vertical sizing, clamping, rotated label projections, and no overlap while
  magnified.
- Pure gesture tests for item boundary selection, endpoint clamping, neutral
  tap, cancel, haptic transitions, row hysteresis, and orientation mapping.
- Weapon-row tests for every ownership/tier/current-selection combination,
  including Laser, fixed high/low tier membership, one owned partner, both
  partners, hidden unowned weapons, and hidden zero-ammo secondaries. Explicitly
  verify that user autoselect order and last-was-super state do not reorder the
  rows.
- Native request tests for packed request integrity, index bounds, ownership and
  secondary-ammo validation, one exact engine selection, and D1/D2 behavior.
- Input-demo recording/replay coverage for exact primary and secondary selection
  so the new direct hook cannot introduce a replay desync.
- JSON and human-readable config round trips for the new fields, plus old wheel
  records defaulting to `WHEEL`.
- Editor tests that dragging the rail does not move the control and that changing
  orientation/span reclamps it.
- Extend a high-level Android automation script to select a Guide item through a
  scroll strip and select both main and alternate D2 weapon rows. Assert state
  through introspection, not screenshots.

## Resolved product decisions

The initial implementation now has the following agreed baseline:

1. The 20 percent setting is total rail length, not length on each side.
2. Interaction is one held press-drag-release gesture.
3. A no-movement release performs no action.
4. The list is bounded with no inertia. Its endpoints select the first and last
   visible items, so wraparound is intentionally excluded.
5. Label angle is signed, with 0 as the requested perpendicular default.
6. Selected scale is stored, defaults to 2.0, and remains a tuning value.
7. Long content softly fades at the rail endpoints.
8. D2 main weapon rows are the fixed higher tier, independent of user weapon
   sorting and last-was-super state. Lower-tier weapons form the alternate row.
9. Unowned weapons and zero-ammo secondary weapons are absent rather than shown
   disabled.
10. Either cross-axis direction selects the alternate row.
11. Converting a custom wheel center binding offers to append it as an item and
    never silently discards it.
12. Authored custom multiple rows are excluded from version 1. Paired rows are
    special behavior for D2 weapon selectors.
13. Existing bundled selectors remain wheels until a scroll strip is explicitly
    selected.
14. Locked Guide deploy uses the linear outer-third safety gesture.
15. Version 1 adds no D-pad/controller navigation beyond existing wheel support.

The center scale, row-switch distance, fade length, item gap, and magnification
curve are expected tuning constants rather than unresolved interaction design.

## Suggested implementation phases

1. Shared selector model/config/editor placement and pure geometry helpers.
2. One-row generic scroll-strip runtime with Guide preset and automation.
3. Parameterized exact weapon-selection JNI request, safe game-thread consumer,
   and input-demo direct-command recording/replay.
4. Fixed-tier D2 weapon rows, ownership/ammo filtering, exact selection, and
   weapon-state presentation reuse.
5. Bundled configuration option, integration tests, formatting, Android tests,
   and host build/test verification required by repository guidance.
