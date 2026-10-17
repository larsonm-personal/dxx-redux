# Auto-level native replay prerequisites

## Scope

The public auto-level owners record on an Android emulator and then invoke the full native SDL/OpenGL replay executable. The single-device nightly lane prepares the Android toolchain but currently omits the native Linux build dependencies and a display server.

## Plan

- Detect only selected public auto-level demo/cheat owners
- Provision the existing native Linux prerequisites and a bounded software-rendered Xvfb display
- Keep the Android lane, replay runner, comparisons, fixture boundaries, and terminal result rules unchanged
- Add the failing D2 public cheat owner to the existing PR sample, within its current 2700-second lane budget
- Test both selected and unrelated execution paths, then verify the draft PR's public CI

## Evidence limits

Trusted run 39054123176 exposes only sanitized final owner statuses. The master process can turn individual PASS receipts into FAIL when its aggregate exit is nonzero, so the four master rows do not identify the first failing case. This setup repair addresses a verified unmet auto-level prerequisite without claiming that the original failure reached native replay or changing those receipts.

## Verification

- 60 sweep controls, 21 Android fixture controls, and 12 retail-boundary controls pass
- Actionlint and whitespace checks pass
- Scoped formatter invocation passed; unrelated baseline Python reformatting was excluded from this narrow patch
- Independent review found no blocking defect
- The Android-single sample grows from 1440 to 1740 estimated seconds, within the existing 2700-second budget
- Public PR CI must still establish actual GLX startup, Android recording, and cold native build/replay completion within the unchanged 600-second owner timeout
