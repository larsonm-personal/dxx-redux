# Mouse-mode edge continuous movement design - 2026-08-22

Goal: Design an optional mouse-mode touch-region behavior that adds continuous axis movement near region edges while preserving ordinary drag movement.

1. [done] Trace touch-region configuration, editor UI, persistence, runtime pointer tracking, and axis dispatch
2. [done] Define activation, edge ramp, additive motion, clamping, and per-axis boundary behavior
3. [done] Identify exact implementation and test touch points, then record the recommended design and open choices

Initial scope was design only. The follow-up implementation request completed the runtime work described below.

## Implementation progress

1. [done] Add configuration, persistence, validation, and editor controls
2. [done] Add pure edge policy and integrate it with mouse pointer/drain state
3. [done] Add focused unit and serialization coverage
4. [done] Run scoped formatting, tests, and Android build verification

Implementation verification completed:

- Scoped `android/run-code-quality.ps1 -Fix` passed
- Direct ktlint format and validation passed for the two newly added Kotlin files
- Focused `TouchMouseEdgeMovementTest`, `MouseModeTuningTest`, and `TouchEditorZoneEdgeTest` run passed
- Full `testDebugUnitTest assembleDebug` run passed, including CMake builds for arm64-v8a, armeabi-v7a, and x86_64

## Recommended configuration

Add these fields to `AnalogStickControl`:

- `mouseEdgeContinuousMovement: Boolean = false`
- `mouseEdgeRegionPct: Float = 15f`
- `mouseEdgeMaxRatePct: Float = 100f`

The feature defaults off so existing layouts retain their current behavior. Persist the three values in the mouse-mode JSON block and parse them in both `AnalogStickControl.fromJson` and `HumanReadableConfig.parseStick`. Add numeric validation for the percentage fields. Recommended editor ranges are 1% to 50% for edge size and 0% to 100% for maximum rate. Limiting the edge to 50% prevents the opposite edge ramps from overlapping.

In `StickPropertiesPanel`, show `Edge Continuous Movement` only while `Mouse Mode` is selected. When that second checkbox is enabled, show `Edge Size` and `Edge Max Rate` percentage sliders immediately below it.

## Runtime model

Keep the behavior in the existing mouse drain loop. Track the last pointer position in addition to the touch-down origin already recorded by `beginMouseDrag`. On every 16 ms drain tick:

1. Drain ordinary accumulated mouse drag exactly as today
2. Compute independent X and Y edge contributions from the last pointer position
3. Add each edge contribution to that tick's drag output
4. Apply the existing axis inversion to the combined value
5. Clamp the final value to `[-1, 1]` and dispatch it once per axis

The edge contribution should not pass through mouse acceleration, response curve, deadzone, or per-axis drag sensitivity. It has its own explicit maximum-rate control. A drag opposite to the edge contribution can therefore cancel or reduce it for that drain tick, while ordinary drag remains fully operational.

For one axis with zone bounds `[low, high]`:

```text
edgeWidth = (high - low) * edgeRegionPct / 100
minimumStartTravel = edgeWidth / 2

negativeRamp = clamp((low + edgeWidth - position) / edgeWidth, 0, 1)
positiveRamp = clamp((position - (high - edgeWidth)) / edgeWidth, 0, 1)

negativeEnabled = origin - position >= minimumStartTravel
positiveEnabled = position - origin >= minimumStartTravel

edgeAxis = (positiveRamp if positiveEnabled else 0)
         - (negativeRamp if negativeEnabled else 0)
edgeAxis *= edgeMaxRatePct / 100
```

This yields zero at the inner boundary of the edge band and a linear ramp to the configured maximum at the zone boundary. Positions beyond an interior zone boundary remain clamped at maximum. X uses the zone width and Y uses the zone height, so rectangular regions behave consistently.

The start-travel gate is directional and measured from the original touch-down coordinate, not cumulative path length. It is evaluated separately for all four directions. A touch beginning at the right screen or zone edge cannot travel half an edge width farther right, so rightward continuous movement remains disabled for that gesture. It can still activate leftward movement after sufficient leftward travel. No extra latched state is required because an edge position that passed the directional threshold continues to satisfy it; leaving the edge band returns the contribution to zero.

## State and transfer details

- Initialize current pointer position and origin on mouse-mode down
- Update current position on every move, even when the drag delta is zero
- Clear current/origin state and pending drag on up, cancel, layout replacement, or overlay deactivation
- Preserve the original gesture origin if a pointer is temporarily stolen by an overlapping axis region and later returned to the mouse region. Do not reset the edge activation origin to the re-entry position
- Continue the drain while a mouse-mode pointer is held, even with no pending drag, since the edge contribution is continuous
- Use the configured floating-zone rectangle as the edge reference, not the full view and not the visual stick circle
- Compute the axes independently, allowing diagonal continuous movement near a corner

## Suggested code organization

- Put the pure per-axis ramp and activation calculation in `TouchMouseEdgeMovement.kt`, alongside the existing extracted mouse acceleration policy
- Keep pointer state, drain scheduling, addition, inversion, and callback dispatch in `TouchOverlayView.kt`
- Extend `TouchControl.kt` for the model, JSON output/input, and numeric validation
- Extend `HumanReadableConfig.kt` for imported human-readable layouts
- Extend `TouchEditorPage.kt` for conditional controls
- Update the bundled `advanced.json` only if the desired test preset should ship with the feature enabled. Otherwise its omitted fields retain the off/default behavior

No d1 or d2 engine changes are needed. This remains Android overlay behavior and uses the existing shared axis callback path.

## Verification plan for implementation

1. Add pure JVM tests for the inner-boundary zero, midpoint 50%, outer-boundary maximum, beyond-boundary clamp, both signs, rectangular X/Y sizing, and disabled mode
2. Test the half-edge threshold just below, exactly at, and above the boundary
3. Test a touch starting at a zone edge cannot activate farther outward but can activate the opposite direction
4. Test drag plus edge addition, opposite-sign cancellation, final clamping, and inversion ordering
5. Add native and human-readable JSON round-trip/default/invalid-range tests
6. Add an overlay integration or extracted-state test confirming held edge output continues across drain ticks and returns to zero on release
7. Run the focused Gradle unit tests, scoped code quality pass, then the normal Android build and integration smoke required by the repository

## One product choice

The recommendation treats `Edge Max Rate` as independent of the X/Y sensitivity sliders. This makes 100% mean full joystick-axis turn rate regardless of drag sensitivity and gives each control a single clear job. If the desired meaning is instead "100% of the current mouse sensitivity," only the scaling step changes, but the UI label should then say `Edge Rate Scale` to avoid ambiguity.
