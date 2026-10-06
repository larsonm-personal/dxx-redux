# Intro movie skip controls

- Reproduce the missing D2 movie overlay with real movie assets and inspect engine pause/overlay state
- Restore launch-intro tap dismissal without changing in-game movie tap protection
- Keep explicit movie Skip available regardless of gameplay-overlay visibility
- Extend input integration coverage to use real touch events and assert visible overlay state
- Run scoped formatting, Android builds, and relevant D1/D2 input tests

Changes validated

- D2's movie handler now admits fresh startup-intro taps through the existing transition guard; in-game movies still consume ordinary taps
- Movie Skip visibility no longer depends on gameplay controls being hidden; existing intro preference behavior is preserved
- Intro automation now sends ordinary taps instead of bypassing input with a direct screen-advance request
- Added Skip visibility/label introspection and real overlay-button touch targeting
- Added a real-movie integration test with declared intro/mission movie dependencies: startup tap, visible controls, protected mission movie taps, paused-movie Skip, and preservation of the following briefing pages
- Final Android ARM64/x86_64 build passed, both D1/D2 launch touch/controller tests passed (16 steps each), and D2 real movie integration passed (29 steps)
- Scoped formatting, automation catalog, master-suite catalog, and whitespace checks passed
- The baseline normal mission movie already showed Skip on the emulator; complete overlay disappearance on the user's device was not reproduced, so its exact cause remains unconfirmed
