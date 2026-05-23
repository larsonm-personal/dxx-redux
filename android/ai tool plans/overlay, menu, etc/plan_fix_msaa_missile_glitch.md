# Plan: Fix missile-triggered MSAA visual glitch

## Goal
Find and fix the root cause of the single-player render glitch triggered by firing a missile on D2 level 3 when starting on MSAA x2 or x4.

## Repro
- Descent 2
- Single player
- Level 3
- Start with MSAA x2 or x4
- Fire a missile
- View rapidly flips between two nearby viewpoints until MSAA is cycled
- Cycling MSAA clears it, and it usually does not recur after the first change

## Investigation
- [x] Trace Android MSAA setup and runtime MSAA mode changes
- [x] Trace missile / missile HUD / subview render path
- [x] Check for GL viewport, FBO, scissor, resolve, and texture binding state leakage
- [x] Check for one-time lazy init paths touched only after the first missile view
- [x] Identify the exact state mismatch and implement the smallest fix

## Root Cause
- Android MSAA tracked `g_msaa_frame_depth` per `ogl_start_frame()` but decremented it only once in `gr_flip()`
- `render_frame()` runs once for the main view and again for missile / cockpit subviews, so the first missile view made the depth counter drift upward permanently
- Once the counter was stuck above zero, the MSAA FBO resolve path no longer matched the actual render passes, which explains why cycling MSAA cleared the problem
- Follow-up regression after fixing depth accounting: the shared MSAA FBO color clear still triggered on the missile HUD subview pass, which wiped the already-rendered main scene and produced a black screen
- Final fix: balance depth in `ogl_end_frame()` and clear MSAA color only on the first MSAA-backed 3D pass of the frame

## Validation
- [x] Android debug build passes
- [x] Code quality checks run for touched files
- [x] Re-review relevant render code for regressions

## Notes
- `gradlew assembleDebug` passed after the fix
- `run-code-quality.ps1 --fix` passed for C/C++ and other touched areas; ktlint still reports the pre-existing `BuildInfo.kt` whitespace issue
