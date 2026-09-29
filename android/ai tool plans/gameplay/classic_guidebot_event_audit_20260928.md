# Classic Guidebot goal transition audit

> Superseded implementation: the immediate reactor/key goal changes and completion
> changes described below were removed after the user clarified Redux fidelity.
> See classic_guidebot_authenticity_audit_20260928.md for the current investigation,
> restoration, presentation fix and validation. This file records the first attempt

- Inspect the supplied co-op log and distinguish stale goal presentation from routing failure
- Audit automatic keys/reactor/boss transitions, explicit commands, completion indices, mode isolation and co-op ownership
- Refresh Classic automatic intent independently of return-to-player navigation gates
- Extend the real-level Classic reference test with transition expectations that do not inherit old reference defects
- Build Windows/Android and run focused routing, save continuity and Android lifecycle coverage

## Initial evidence

The reactor dies at 14:24:22.720. Guidebot remains in AIM_GOTO_PLAYER with
goal=-1 and the old reactor object index during periodic refreshes. An exit path
to segment 402 is created at 14:24:38.295. The persistent message can retain the
reactor goal until path selection resumes. The frozen Classic frame has the same
distance gate; comparison against it alone does not protect event responsiveness

## Changes and audit findings

- Classic publishes the exit goal on the first escort frame after reactor destruction,
  including while returning to its owner and after the periodic goal reset
- Countdown escape takes priority over uncollected keys; explicit commands retain
  their existing priority. Return paths remain intact until the normal rejoin gate
  allows exit guidance
- Owner key notifications refresh Classic's persistent message immediately. The
  existing owner-inventory selector and non-owner notification filter remain intact
- Key completion now compares the semantic goal with the key color, rather than
  comparing an object index with a goal enum. Another copy of the same key counts
- Completion rejects invalid object indices. A fuel-center visit (-4) completes
  the fuel-center command only; object collisions do not count as segment visits
- Exit and recall commands cannot be accidentally completed by an object whose
  number happens to match the target segment. Recall still completes by docking
- Completed Classic commands publish their next automatic objective instead of
  leaving the completed command in the persistent message

The frozen Redux reference contains the return-to-player delay, object-flags key
selector, key enum/index confusion and unsafe completion indexing. These findings
do not establish that Enhanced routing introduced those defects. Persistent HUD
messages make stale intent more visible. The reference is kept unchanged, with
independent event assertions and an explicit countdown-priority expectation

Reviewed Enhanced frame/event/completion guards, Classic path recalculation,
exit selection, door/key policy and co-op owner inventory. No new saved fields,
network packets or Enhanced planning policy changes were needed. Completion index
safety is shared by both modes

## Validation

- Scoped code quality and git diff whitespace checks passed
- Windows D2 build and Android debug APK build passed
- Real-level native navigation: 3,819 checks on level 1 and 7,539 on level 11,
  each repeated with identical reports. Includes Classic reference path, door,
  frame and movement comparisons, native save mode tests and the new event audit
- New event cases cover single-player and co-op flags, reactor/boss/key/unspecified
  previous goals, periodic refresh boundaries, far-owner intent, rejoin, all key
  colors, explicit commands, fuel visits and exit/recall index collisions
- Same-mode save continuity passed twice on levels 1 and 11 for both modes and
  four shared commands plus Enhanced Unexplored, comparing 120 resumed frames
  against uninterrupted simulation including position, navigation and RNG state
- Six focused CTest suites passed: messages, route decisions/certifier, owner,
  exit and goal policy
- Android test_guidebot_routing_modes passed all 58 steps on emulator-5554 with
  the rebuilt APK, covering mode selection, commands and save restoration

The native reactor fixture sets the countdown flag directly. Its rejoin check
places the bot at the exit because it does not fire reactor-linked door triggers.
This is not a full two-device co-op reactor playthrough
