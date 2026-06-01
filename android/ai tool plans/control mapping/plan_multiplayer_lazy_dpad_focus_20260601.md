# Plan: Multiplayer Lazy D-pad Focus

## Status

Implemented.

## Goals

- Keep the same logical starting controller selection on phone touch+controller and controller-only layouts.
- Hide the green controller focus highlight until the user navigates with the D-pad on the current page.
- Reset highlight visibility when navigating to another multiplayer page.
- Make the multiplayer browser default logical selection the Connect button when disconnected.
- Allow Host LAN Game mission focus to be entered and left with the D-pad.
- Keep D-pad focus on the activated control after actions such as Restore.

## Steps

- [x] Inspect existing multiplayer focus policy, tests, and screen focus requesters.
- [x] Design a shared lazy focus-visible model for multiplayer pages.
- [x] Implement browser, host, and LAN view fixes using the shared policy.
- [x] Add or update tests for the default focus and no-focus-snap behavior.
- [x] Run focused Android unit tests and code quality for touched files if practical.
