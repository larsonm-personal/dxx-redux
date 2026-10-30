# Owned XL robot disarming

Wall and supported object type-34 actions set an XL-owned neutral state on
live robot slots. Redux retains its existing alternate. The implementation
lives in `runtime/robot_disarm.cpp`; engine hooks only consult that state at
ranged firing, melee and thief decisions. Reprogramming (type 35) is separate
and remains rejected.

## Source semantics

The existing trigger source owner executes the actual `DoDisarmRobots`,
`CGameData::Object`, inline attack-mode methods, attack predicates and melee
consumer from XL commit `2576a98b5de310e3ac7c9777df2677d1627b8bae`.
It also executes the exact ranged and thief return guards. The adapter compares
1,120 cases across attack modes, object types, firing permission, positive and
negative side values, runtime object-table endpoints and missing slots. This
does not claim whole-AI equivalence. Consumed source files are SHA-256 pinned.

Only negative-side links select objects. Positive-side links, missing slots
and nonrobot targets are inert. Target indices address current runtime slots,
including generated robots; they are not limited to the authored object table.
The initial draft restricted that table and rejected missing slots. Source
lookup inspection corrected both behaviors before publication.

Neutral state suppresses ranged gunfire, melee attacks and thief decisions.
It neither freezes movement nor removes previously fired weapons. A thief
already attacking can still attempt theft on player contact: the actual
collision path has no disarm guard. Run-away bomb dropping also uses a separate
source path without a disarm guard (`AIMRunFromObjectHandler2`). That observation
is a static source audit, not a new qualification of all run-away AI behavior.
Disarming does not implement friendly/reprogrammed robots.

## State and lifecycle

Neutral entries bind the object index to its native lifetime signature. A
deleted or reused slot loses its prior disarm state; a subsequent action can
disarm its new robot. Stale entries are omitted from snapshots. Level and
mission transitions clear owned state before new objects activate.

Save schema 19 adds canonical, sorted `neutral_robots` pairs to the existing
XL trailer. Restore requires live robot/signature agreement and validates the
whole trailer before publishing any owned state. Delayed wall actions retain
their earned elapsed time; object actions retain the source's immediate-event
time behavior. Ordinary disk and rewind use the same state path.

## Controlled verification

The labeled derived fixture uses unchanged Neptune geometry, four explicitly
encoded ordinary robots, eight wall actions and two powerup-owned actions.
It supplies both 23/40 and 27/40 layouts. Original mission bytes are not edited.
The fixture is a mechanics control, not Neptune or Pumo campaign acceptance.

Native Windows controls pass in both layouts, covering:

- Player wall contact, master chains, one-shot, disabled, alternate and autoplay
  behavior, plus actual powerup removal and inert/missing targets.
- Actual armed and neutral ranged, melee and thief consumers. Ranged checks
  require a real weapon object, retain an existing projectile, and check that
  a suppressed shot changes neither RNG consumption nor firing state.
- Object deletion and slot reuse, saved neutral lifetimes, exact delayed
  continuation and ordinary disk restoration after a stock mission.
- Twelve malformed trailers rejected atomically, checked by comparing the
  complete recaptured trailer bytes, and continued rejection of reprogramming.

The first Android run reached stock transition but exposed an inherited
`AIB_STILL` behavior in the test's converted thief. Collision can legitimately
change that behavior's mode after the theft attempt. The control now selects
the normal thief `AIB_SNIPE` behavior and reports mode/behavior/flags on failure.
This changes the controlled setup, not the runtime implementation.

Android passes all 67 steps in 152,327 ms, run
`0504eb7904aa4a9888009cb375696a0d`: seven actual attack-consumer sequences,
two event sequences, two lifetime-reuse sequences, one stock sequence and
eighteen state verifications. All 26 protected app-file hashes match before
and after the run. The owner includes ordinary disk and checkpoint rewind in
both layouts. Receipts are under `temp/xl-disarm-android/`.

Both affected public contracts pass on Windows and Linux after the final
runtime-slot correction. Full public suites passed 43 Windows and 46 Linux
controls before that correction; unaffected owners were not needlessly rerun.
Both Android catalogs and the 19 private-owner Python controls pass.

The final Linux private owner passes all 106 required controls, sixteen original
format outcomes and three modern source comparisons in 444.797 seconds. Its
CTest phase takes 440.78 seconds; the new disarming integration takes 6.22
seconds. Both layouts pass all actual attack, event, lifetime, malformed-save
and disk-through-stock controls. The trigger source owner records all 1,120
comparisons. Receipts are under `temp/xl-disarm-20261029/`.

Windows and Linux generate identical fixture bytes. The HOG SHA-256 is
`58ca2f69cea79ae6ade83f17019b5ee58e9f198771e88c59011ed2ade94efd43`;
the descriptor SHA-256 is
`c18be494d3dcc22471d42c37f200a30c4ce6175309dd32fb0b932e3bf060c689`.
The Android owner requires these identities before installation. Scoped mixed
formatting, direct PowerShell analysis and diff checks pass.

## Original Pumo inventory

Fresh inspection of unchanged HOG
`6e3e723e549897188a8fdc19db8b04630de9d428d2b31f8e3f48af108915b043`
removes exactly one type-34 rejection: `mvlev5.rl2`, trigger 23, targeting robots
83 and 93. Every other inventory field and blocker remains identical to the
countdown checkpoint. All fourteen original levels retain independent blockers.

The browser counts wall and object disarming and keeps owner/robot-profile
qualification separate. Unsupported object flag bits and reprogramming remain
blocked. Neither a successful derived fixture nor the removed original action
rejection establishes a playable Pumo campaign.
