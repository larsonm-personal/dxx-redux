# Launcher text contrast

The LAN hosting IP and other unstyled labels inherited LocalContentColor's black default: LauncherTheme only installed a dark color scheme, while secondary screens return before the main Surface.

- Supply the normal onSurface foreground at the shared launcher theme boundary
- Replace dark brown cleanup-notice text and pale backgrounds with normal light text on dark theme containers
- Replace the file browser's hardcoded dark green filename text with onSurface
- Preserve semantic colors and component-specific content colors, such as readable text on filled buttons
- Run scoped formatting and the Android build; no new tests for this presentation-only change

Completed: scoped formatting and git diff --check passed. Android debug APK built successfully (including D1/D2 native libraries); log: android/temp/launcher-text-build.txt. Existing native warnings were unrelated to these Kotlin presentation changes.
