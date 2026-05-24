# Video overlay height scaling plan

## Goal
Scale the video info overlay by height so every visible row fits on short displays, with touch hit regions matching the scaled button rows.

## Requirements
- Use view height as the fit constraint
- Shrink non-touch info text first so buttons fit below it
- Scale touch button drawing and hit rectangles together
- Keep existing controller and touch behavior intact
- Validate with focused unit coverage plus Kotlin formatting/tests

## Steps
- [ ] Inspect current overlay layout and nearby tests
- [ ] Add a height-only layout calculator for info rows and action rows
- [ ] Apply separate info and button metrics in drawing and hit rectangles
- [ ] Add unit tests for normal, debug, and very short heights
- [ ] Run code quality and focused Android unit tests
