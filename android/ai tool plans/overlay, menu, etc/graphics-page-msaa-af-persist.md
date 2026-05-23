# Graphics Page + MSAA/AF Persistence

## Goal
1. Move Graphics settings out of AdvancedSettingsPage into its own top-level launcher page
2. Add MSAA and AF level selectors to the new Graphics page
3. Make in-game overlay MSAA/AF changes persist to SharedPreferences
4. Apply saved MSAA/AF on game startup

## Plan

### Phase 1: Create GraphicsSettingsPage.kt
- [x] New composable page with BackHandler, header ("Graphics"), scrollable body
- [x] Move from AdvancedSettingsPage: Resolution picker, Texture Filtering, Color Depth
- [x] Add MSAA level selector (Off / 2x / 4x) backed by SharedPreferences "msaa_level"
- [x] Add AF level selector (Off / 2x / 4x / 8x / 16x) backed by SharedPreferences "aniso_level"
- [x] "Takes effect on next launch" note for all

### Phase 2: Remove graphics from AdvancedSettingsPage
- [x] Remove GraphicsSettingsSection composable and its call from AdvancedSettingsPage
- [x] Remove the divider after graphics section
- [x] Remove unused prefs parameter from AdvancedSettingsPage composable
- [x] Keep computeResolutionOptions in AdvancedSettingsPage.kt (still internal, used by GraphicsSettingsPage)

### Phase 3: Add navigation in SetupActivity
- [x] Add `showGraphicsPage` state variable alongside existing page states
- [x] Add if-block for GraphicsSettingsPage in navigation section
- [x] Add `onGraphicsSettings` callback to ControllerSection
- [x] Add "Graphics" button in the button row (next to Advanced Settings)

### Phase 4: JNI for MSAA/AF (non-debug)
- [x] Create `nativeSetGraphicsOption(String name, int value)` in jni_main.c outside INTROSPECT_ON
- [x] Handles "msaa_level" and "aniso_level" same as current debug flag handler
- [x] Declare in MainActivity.kt
- [x] In surfaceCreated, after gameStarted=true, apply saved prefs values via JNI

### Phase 5: Overlay persistence
- [x] In VideoInfoOverlay, add settingsSaver callback: ((String, Int) -> Unit)?
- [x] In cycleAnisotropy/cycleMsaa, call settingsSaver with key and new value
- [x] In MainActivity, wire settingsSaver to save to SharedPreferences

### Phase 6: Build, lint, test
- [x] Build successfully
- [x] Run linter (clean, only third-party warnings)
- [x] Run integration test (EXIT: 0)
