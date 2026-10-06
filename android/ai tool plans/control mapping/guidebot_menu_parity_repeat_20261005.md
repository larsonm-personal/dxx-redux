# Guidebot menu parity and navigation repeat

- [x] Match native Guidebot commands to the touch menu, including Enhanced routing and secret-cheat visibility, retaining message suppression
- [x] Share Android navigation repeat timing and generate held D-pad repeats for native and overlay menus
- [x] Extend existing menu integration coverage and repeat dispatch tests
- [x] Run scoped formatting, Android build/tests, menu integration, and relevant Windows builds

The user's third numbered item is empty; implement the two described requests

## Validation

- Scoped mixed-language formatting and `git diff --check` passed
- Windows D1 and D2 builds passed; D2 rebuilt after the close-callback fix
- Four focused guidebot/escort CTest cases passed
- Android x86_64 debug build and 1,135 JVM tests passed
- `test_guidebot_routing_menus.ps1 -Serial emulator-5580` passed, covering native/touch visibility, wraparound, held repeat, Unexplored/Next activation, hidden Secret shortcut, and save restore
- Automation catalog validation and master-runner catalog tests passed
- The stock broad controller test encountered an unrelated empty-demo-list assumption (five entries remain), and its gameplay section expected a nonzero legacy raw-axis value; a scratch copy runs the relevant menu-only cases without those unrelated sections
- Menu-only controller smoke tests passed on the emulator: D1 145/145 steps, D2 146/146 steps

The ABI-injected Gradle build writes its fresh test-only APK to `app/build/intermediates/apk/debug/app-debug.apk`; install that path with `adb install -r -t` instead of the stale APK in `outputs/apk/debug`
