# Mouse edge region visualization plan - 2026-08-26

Goal: Make mouse-mode continuous-movement regions understandable in the touch editor by drawing low-opacity directional strength bands and small stick-direction labels without changing the in-game overlay or touch behavior.

1. [done] Trace the mouse-mode edge calculation, touch-region geometry, runtime drawing, editor preview, and focused tests
2. [done] Add editor visual-band geometry alongside the existing edge calculation
3. [done] Draw the strength bands and directional labels in the touch editor preview
4. [done] Add focused geometry tests and complete automated Android verification
5. [done] Show both configured stick-axis labels at every stick's default editor position
6. [done] Rename controller mapping, reduce band intensity, and use configured-axis direction labels

## Intended appearance

- Draw the horizontal-axis regions in yellow and the vertical-axis regions in green
- Treat 18% alpha as the nominal visual maximum for a 100% movement region. This is 40% less intense than the original 30% cap. Continue to multiply that cap by the existing layout global opacity and stick opacity, so this addition never makes a control more opaque than its configured overlay
- Fade each ramp linearly from transparent at its inner boundary to its configured movement strength at the outer boundary
- Keep the screen-edge standoff portion at a uniform value. With `Edge Max Rate` at 100%, that portion uses the full yellow or green color at the 30% alpha cap
- Scale the entire band by `mouseEdgeMaxRatePct`. For example, a 50% maximum-rate standoff band reaches half of the 30% visual alpha cap
- Allow horizontal and vertical bands to composite in corners. This communicates that both axes can contribute there without introducing another corner color or special case
- Draw small labels near the center of each mouse-region edge using the configured axis function and direction, such as `Turn Left`, `Turn Right`, `Pitch Up`, and `Pitch Down`
- Draw the same configured-axis direction labels at the four cardinal positions inside every ordinary touch stick's default editor circle
- Swap each opposite label pair when that axis is inverted, so the text describes the axis output produced at that physical edge or stick direction
- Use a small density-aware 9sp label size with a restrained high-contrast text treatment and respect the existing layout/stick opacity
- In the editor preview, replace the current centered mouse-mode axis abbreviation with the four edge labels while edge continuous movement is enabled
- Independently draw both configured axis names as a compact two-line label centered on every stick's default `xPct` and `yPct` position, including floating and mouse-mode sticks

The visualization is a static map of the configured movement available at each coordinate. It does not blink on and off based on the current touch-down origin or the half-edge start-travel gate. Those gesture conditions affect whether movement is active now, but hiding configured regions until activation would make the control harder to discover and tune.

## Geometry and single source of truth

Refactor `TouchMouseEdgeMovement.kt` so movement and visualization use the same clamped boundaries:

- Ramp width remains `touch region span * mouseEdgeRegionPct`
- The negative full-rate boundary remains the clamped `screen low + screen edge zone width`
- The positive full-rate boundary remains the clamped `screen high - screen edge zone width`
- Negative ramp spans from the negative full-rate boundary through one ramp width toward the region interior
- Positive ramp spans from one ramp width inside the positive full-rate boundary through that boundary
- Any region between the physical touch-region edge and its full-rate boundary is the uniform standoff band

Expose only the small pure-data result needed by the editor drawing path, such as the two ramp endpoints and full-rate boundary for one axis. Keep origin-dependent activation and signed runtime output calculation unchanged. Keep the helper beside the movement policy so representative unit tests can prevent their geometry from drifting apart.

Invalid or zero-width regions should produce no visual bands. If continuous movement is disabled or the configured max rate is zero, skip the colored bands and edge labels and retain the ordinary mouse-region rendering.

## Touch editor preview

Update the mouse-mode branch of the Compose preview in `TouchEditorPage.kt` to use the pure band boundaries, colors, alpha scale, and four small labels. Keep all editor hit testing and zone-edge selection behavior unchanged. The preview should make `Ramp Size`, `Edge Max Rate`, and `Screen Edge 100% Zone` changes immediately visible while their sliders are adjusted.

Do not change `TouchOverlayView.kt`. The in-game mouse-region fill, border, centered axis labels, extreme-action state, and afterburner display must remain exactly as they are now. The new highlighting and four directional labels exist only on the editor Canvas.

Do not add a new persisted visualization setting in this tranche. The bands and four labels are conditional on the already explicit `Edge Continuous Movement` setting, so layouts with the feature off are unchanged and no JSON/config migration is needed.

## Focused tests

Extend `TouchMouseEdgeMovementTest.kt` with pure geometry assertions covering:

- Left/right and top/bottom use the same policy with different spans
- Ramp inner edge is zero intensity, midpoint is 50%, and full-rate boundary is 100%
- The standoff interval remains at full configured strength
- A touch region inset from the physical screen has a ramp but no artificial full-rate interval beyond its own edge
- Full-rate boundaries clip correctly when a touch region overlaps a screen standoff zone
- `mouseEdgeMaxRatePct` scales visual strength and zero suppresses the visualization
- Degenerate bounds return no visual geometry
- Inversion swaps label direction without changing band magnitude or placement
- The computed band boundaries match representative values already asserted for runtime movement, preventing visual/runtime drift

No `TouchOverlayView.kt`, d1, or d2 engine changes are required. This remains Android touch-editor-only presentation work.

## Verification

1. Run the focused `TouchMouseEdgeMovementTest` JVM test
2. Run the complete Android unit-test suite
3. Run `android/run-code-quality.ps1 -Fix` scoped to the changed Kotlin and test files
4. Build `:app:assembleDebug` with JDK 21
5. On the emulator, verify a mouse-mode layout with continuous movement enabled at 100% and a non-100% max rate. Confirm the editor's linear fades, 18% alpha ceiling, solid-value standoff bands, corner composition, configured-axis direction labels, slider-driven preview, and unchanged editor hit testing. Launch the game once to confirm none of the new highlighting or labels appear in-game

Automated verification completed:

- Scoped code quality passed for the changed Kotlin, test, and plan files
- Focused `TouchMouseEdgeMovementTest` passed
- Full `testDebugUnitTest` passed
- `assembleDebug` passed for arm64-v8a, armeabi-v7a, and x86_64
- Focused tests and `assembleDebug` passed again after adding the two-line stick-axis labels
- Scoped code quality, focused direction-label tests, the full unit suite, and `assembleDebug` passed after the configured-axis label and 18% alpha update

The emulator was available, but visual navigation was not run because it would disturb the existing shared emulator session. The source diff confirms `TouchOverlayView.kt` is unchanged, so the new presentation cannot appear in-game.
