# D1 in-level game menu tap registration

## Plan

- Reproduce Options selecting Abort Game using existing touch logs and menu introspection on an isolated emulator
- Compare menu geometry, rendering, and touch mapping in D1 and D2 and fix only the evidenced mismatch
- Build affected native targets, run scoped code quality, and verify game-menu taps plus representative existing menus

## Notes

- Existing pause and overlay edits belong to separate work and are outside this fix
- Use emulator-5590 to avoid other running emulator tests

## Findings

- User supplied the active Retroid Pocket 4 Pro, serial JYPR42510121028, running the GitHub debug package
- The D1 Game Menu at 640x480 renders with source (196,141 248x198), destination (64,36 511x408), on a 1334x750 surface
- Tapping Options at display (667,380) produced Android key 66 (Enter), without a native touch event
- The gameplay overlay remained active because simulation pause unconditionally enabled its visibility; its tap passthrough callback injected Enter and activated the current Abort Game selection
- This is an overlay input-routing bug, not a coordinate or hit-box bug
- The existing pause-presentation changes in MainActivity.kt already remove the gameplay overlay over native menus; no new engine/menu hit-testing change is needed

## Verification

- Restored the abort autosave after baseline reproduction and preserved the installed APK and saves/settings under temp/game-menu-verification
- Built both ARM64 engine targets and the GitHub debug APK, then installed the signed build on the Retroid with app data retained
- The identical (667,380) tap now maps to native (320,241), hits row 1 on down/up, and opens Options with level 1 still loaded
- Actual coordinate taps also opened Graphics Options, Save Game, and Select Game to Restore; a repeated Options tap passed
- Added coordinate-tap regression coverage to the existing D1/D2 pause integration runner, starting with Abort Game highlighted
- Scoped PowerShell code quality and both automation catalog checks passed
