# Skippable screen input centralization research

## Goal

Identify a clean shared boundary for post-level, death, movie, briefing, and level-complete skip input without merging their different completion actions.

## Plan

- [x] Inventory each screen's accepted events, transition arming, and completion action in D1 and D2
- [x] Map Kotlin overlay behavior against native touch and controller routing
- [x] Separate reusable input admission policy from screen-specific actions
- [x] Recommend a staged centralization design and regression matrix

## Constraints

- Preserve desktop behavior and keep D1/D2 source edits small
- Held input at a transition must never count as a fresh skip
- Multiplayer readiness and abort semantics remain owned by their native screens

## Findings

- Android has two independent 500 ms guards: `SkipButtonView.armAtMs` in Kotlin and the native cutscene deadline plus held-input release gate in `android_input.c`.
- The Kotlin guard is presentation-local. When it rejects a touch-down, `dispatchTouchEvent` can continue to the game surface, so it cannot protect a native screen that did not arm the native guard.
- Death is the largest correctness gap. `HandleDeathInput` accepts mouse and joystick button-up after the ship explodes, so an input held before death can abort the sequence when released. Death does not arm the native guard.
- The end-level flythrough has a separate function-local ESC timestamp whose lifecycle is inferred from the first key event instead of being armed at the transition.
- Title, briefing, movie, level-complete, post-level, death, and end-level expose state through several globals and JNI getters. Kotlin recombines those flags to choose `SKIP`, `CONTINUE`, or `NEXT`.
- The actions cannot be centralized safely: close a window, advance a briefing page, abort a movie, set `Death_sequence_aborted`, stop an end-level sequence, close a newmenu, and accelerate a multiplayer timer are different engine operations.

## Recommended boundary

Centralize the Android-only admission and presentation contract while leaving completion actions in their owning engine handlers:

1. Add one skippable-screen context with a kind, active flag, ready flag, transition generation, and native request bit.
2. Begin the context at the actual transition. Beginning it arms the native deadline and records every already-held touch/controller source until release.
3. Define activation as a fresh touch-down, controller A down, or an explicit overlay request. Do not use release as activation.
4. Replace the multiple JNI polling getters used for button presentation with one context-state getter that supplies kind, ready state, and label category.
5. Change `SkipButtonView` to issue `nativeRequestScreenAdvance(generation)` instead of synthesizing ESC. The owning screen consumes the request and performs its existing local action.
6. Keep per-screen readiness local: death requires `Player_exploded`, post-level requires `end_time != -1`, and briefing retains its page-advance rules.

Avoid a registered callback or function pointer in the shared context. These screens use nested window loops and transition rapidly, so a generation-tagged request consumed by the owning handler is safer than retaining a callback with a screen lifetime.

## Staging

- Phase 1: centralize native begin/end, held-input tracking, fresh-press admission, and state introspection
- Phase 2: migrate death and end-level first because they have real stale-input risks
- Phase 3: migrate movie, briefing, level-complete, and post-level adapters
- Phase 4: reduce Kotlin polling to one getter and replace synthetic ESC with a generation-tagged request

## Implementation tranche 1

- [x] Add the Android-only screen-advance context and expose its lifecycle/admission API
- [x] Begin/end the context at death and end-level transitions in both D1 and D2
- [x] Replace death release-to-skip and the end-level local timestamp with shared fresh-press admission
- [x] Add context state to introspection and focused automation coverage
- [x] Build Android and Windows D1/D2, then run the focused emulator tests

## Tranche 1 results

- Death readiness remains local to `Player_exploded`; a held controller release is ignored and a fresh A down is accepted.
- End-level begins its guard at the real sequence transition and accepts fresh touch/A down through the shared policy.
- The old function-local end-level timestamp and Android death button-up activation are removed.
- Android D1/D2 compiled and linked for `arm64-v8a`, `armeabi-v7a`, and `x86_64`; the debug APK assembled successfully.
- The required Windows D1/D2 host build completed successfully.
- `test_death` passed for D1 and D2, and `test_levelcomplete_touch_skip` passed with its new held-touch end-level case.

## Implementation tranche 2

- [x] Confirm the lifecycle, readiness, and completion action for movie, briefing, level-complete, and post-level in D1 and D2
- [x] Add a generation-tagged native advance request and a compact JNI presentation snapshot
- [x] Migrate movie, briefing, level-complete, and post-level to the shared lifecycle/admission contract
- [x] Consolidate Kotlin transient-screen polling and replace synthetic Escape with the native request
- [x] Extend focused automation for held input and stale requests
- [x] Run scoped code quality, Android builds, Windows D1/D2 builds, and focused emulator tests

## Tranche 2 results

- One native context now presents and admits death, end-level, title/movie, briefing, level-complete, and post-level advance input.
- Kotlin replaced four transient-screen JNI polls with one packed state snapshot and submits generation-tagged requests instead of synthetic Escape.
- The 500 ms deadline and held-input release gate are armed before a new context is published, so neither a fresh overlay request nor queued touch/controller input can race the transition.
- Screen actions remain local: briefing page advance, movie abort, death abort, end-level stop, level-complete close, and post-level timer acceleration are not conflated.
- The final Android APK built D1/D2 for `arm64-v8a`, `armeabi-v7a`, and `x86_64`; the required Windows D1/D2 host build passed.
- `test_levelcomplete_touch_skip` passed its held touch/A, stale request, current request, and end-level handoff cases. `test_death` passed for D1 and D2.
- Scoped code quality passed. The 618-test Kotlin suite continued to expose unrelated mission ZIP music atomic-publication failures (`Could not retain the previous cache generation`); the first failing case passed when isolated, and no changed screen-advance code is exercised by those tests.

## Regression matrix

For every screen kind, cover touch and controller A with input held before transition, released during the guard, fresh press during the guard, fresh press after the guard, stale overlay request from the prior generation, and the screen-specific not-ready state.
