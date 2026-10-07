# Graphics chooser layout repair

- [x] Reproduce clipped chooser text with production Android views at short landscape sizes and enlarged fonts
- [x] Keep the heading, option details, footer and actions readable within the available space
- [x] Run scoped formatting, Android build and existing graphics UI coverage with layout assertions

The chooser already supplies "Choose graphics options" as its heading. Inspect measured text bounds before changing the layout; preserve the shared confirmation and controller behavior

## Findings and fix

At 640x360 dp with normal text, the original 440 dp panel clips the MSAA row and hides the explanatory footer below it. At 2x text and 640x320 dp, the fixed heading, subtitle and actions consume the entire panel, leaving no usable option viewport and pushing actions below the panel edge

The chooser now uses up to 560 dp of width, 48 dp option targets, tighter gaps and a shorter 14 sp footer. Keep the existing heading, explanatory footer and actions outside the scroll region, but move the subtitle into that region. Below 360 dp height, reduce panel padding and action minimum height to 48 dp. The persistent scroll indicator exposes overflow at larger text sizes. Confirmation details share the scroll region; controller navigation to the actions scrolls the full content height to reveal the last option

The existing graphics capability instrumentation now includes production-view layout checks at 640x360, 640x320 and 360x640 dp, with 1x, 1.3x and 2x text. It verifies text measurement, panel bounds, a usable scroll viewport, pinned footer visibility, last-option reachability and no scrolling at normal font size. Rendered images and build/test logs are under `android/temp/graphics-chooser-layout/`

## Verification

- Android x86_64 debug and instrumentation builds passed, including both native engines
- All nine final layout cases and existing graphics capability/preference instrumentation passed; normal and enlarged rendered images visually reviewed
- First-run Accept and Previous integration cases passed for D1 and D2, including all four cross-engine handled-marker relaunches
- Scoped formatting/lint, both automation catalog checks and scoped `git diff --check` passed

Validation used `emulator-5582`; no physical device was connected
