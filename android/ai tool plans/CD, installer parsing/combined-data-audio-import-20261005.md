# Combined game data and CD audio import

- Make extraction plus audio registration the highlighted, initially focused action for mixed data/audio CD images and GOG installers containing CD audio
- Keep the existing individual CD actions and GOG audio-selection workflow below the primary action, using quieter buttons
- Reuse existing extraction and registration code, keep the dialog busy across both CD steps, and retain partial-success state for retry
- Run scoped code quality, compile the Android launcher, and run relevant existing import tests

## Validation

- Scoped `run-code-quality.ps1 -Fix` passed for `SetupDialogs.kt`
- `:app:assembleDebug` passed, including both games for arm64-v8a, armeabi-v7a, and x86_64
- Targeted `:app:testDebugUnitTest` passed: 55 tests across nine CD/CUE/GOG suites, zero failures or errors, one skipped
- `git diff --check` passed
- No device UI interaction was exercised; native build warnings came from unchanged engine/dependency sources
