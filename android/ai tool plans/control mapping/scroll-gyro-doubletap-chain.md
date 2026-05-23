# Scroll Indicators, Live Gyro in Editor, Chain Double-Tap

## Task 1: Add ScrollArrows to AddControlDialog
- [x] Wrap Column in Box(Modifier.heightIn(max = 300.dp)), extract scrollState, add ScrollArrows
- File: TouchEditorPage.kt, AddControlDialog function

## Task 2: Live gyro diagnostic data in touch editor
- [x] Add mutableFloatStateOf for gyroYaw/Pitch/Roll in TouchEditorPage
- [x] Add DisposableEffect to create/manage GyroInputManager when diagnostics exist and gyro enabled
- [x] Pass gyro values through drawAllControls
- [x] Replace static "Gyro Diag" text with live Yaw/Pitch/Roll % display
- File: TouchEditorPage.kt

## Task 3: Gyro absolute angle API
- [x] Already done in prior session -- GyroInputManager uses TYPE_GAME_ROTATION_VECTOR with RATE/ABSOLUTE modes

## Task 4: Chain double-tap detection
- [x] Change s.lastTapTime = 0 to s.lastTapTime = now in all 3 sites (mouse, floating, fixed)
- File: TouchOverlayView.kt

## Task 5: Build, lint, test
- [ ] Build
- [ ] Lint (run-code-quality.ps1 --fix)
- [ ] Deploy and test
