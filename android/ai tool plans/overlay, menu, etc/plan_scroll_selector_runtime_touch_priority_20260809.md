# Scroll selector runtime touch priority

## Goal

Make scroll-strip selector triggers, including Guidebot, take live-game touch
priority over overlapping analog-stick and drag regions just like ordinary
buttons do.

## Plan

- [x] Trace runtime pointer-down routing for buttons, selectors, and sticks
- [x] Reproduce the overlap decision in a focused testable helper
- [x] Apply the foreground selector rule without changing non-overlap behavior
- [x] Run scoped formatting, unit tests, Android build, and emulator verification
- [x] Record the corrected behavior and the earlier mistaken assumption

## Result

The earlier change only affected editor hit-testing. Live `ACTION_DOWN` routing still
offered the pointer to axis regions and sticks before radial/scroll selector triggers.
That also explains why ordinary buttons appeared to work: sticks have a separate
button-latching exception, while a scroll selector must exclusively own its drag
pointer.

Multi-selector triggers now claim their foreground hit area before axis regions and
sticks. Axis-region routing also stops once a foreground control has handled the
pointer. The selector hit boundary has focused unit coverage; the full Android unit
suite and debug APK build pass. The APK was installed and launched into live D2
gameplay on `emulator-5554` with a temporary Guide selector placed inside the left
floating-stick region; the app remained responsive with no fatal exception or ANR.
