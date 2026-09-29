# Classic Guidebot authenticity investigation

## Contract

Clarified by the user: Redux behavior is the compatibility target; literal 1996
behavior is not required. The historical-source differences below remain useful
provenance, but Redux steering/frame-rate adaptations are not restoration defects
under this contract. Investigate before making further gameplay changes

## Correction to the reactor conclusion

The shared Redux distance gate establishes a possible mechanism, not that Redux
would reproduce the observed 15.575-second delay in the same gameplay situation.
The earlier explanation overstated that conclusion

The log shows a return-to-player transition at 14:24:21.726, before destruction
at 14:24:22.720. At 14:24:27.041, 32.059 and 37.090 the bot still uses the same
four-point return path (hide=637), with cursor 3, 2 and 0, while the player changes
segments. It creates an exit path at 14:24:38.295. Thus this is not a fixed
16-second reactor timer: it is time spent in return mode before goal selection

Redux's return-mode branch requires proximity before choosing an objective. Its
return path refresh depends on losing sight of the player for over four seconds;
periodic sightings can retain an old return path while the player moves. The log
samples show recent sightings, consistent with this mechanism, but do not provide
every frame's distance/visibility or an upstream comparison of full AI and physics

Redux cntrlcen.c destruction toggles linked walls and sets the destruction flag;
it has no direct escort goal override. The remaining causal question is why this
bot stayed on that return path so long: expected encounter geometry or a port,
co-op, scheduling or movement difference. It remains unresolved. A controlled
Redux comparison must cover the approach/return period, not just call the goal
selector with an already-distant bot

## Plan

- Establish original source provenance separately from the Redux test reference
- Trace the blue-key regression, reactor behavior and previous uncommitted fixes
- Compare objective selection, commands, return cadence, path following, AI
  scheduling and co-op adapters against both references
- Identify uncovered or deliberately bypassed behavior in current parity tests
- Record confirmed drift, justified extensions and prioritized restoration work

## Sources

- Local original source checkout: android/temp/original-descent2-source-investigation
  from videogamepreservation/descent2 at 75ba13c44259f1e40c0df70961c78abba898eafe
- Current test oracle: Redux 9fd90f03513663ce1372c8cfa723b7a73c4219fa,
  committed September 15, 2026; this is not a 1996 executable oracle
- Existing uncommitted reactor/completion fixes from the preceding turn are
  included in the audit and remain distinguishable from committed history

## Current implementation and evidence

This section supersedes the historical recommendations and earlier work-status
notes below. Redux is the accepted target

### Diagnosis reached

- The recorded session is Descent 1 level 2 in the D2 engine, restored in co-op
- The log confirms local player 0 owns Guidebot, so these are owner-side AI logs,
  not merely a remote pose replica. The D1 enemy AI excludes companion robots
- Guidebot begins returning while the player is visible, before reactor destruction
- Redux deliberately does not announce a visible-player return. The added persistent
  HUD therefore keeps the earlier reactor message despite navigation being in
  return mode. This presentation mismatch is established directly from the log
  and the production message path
- A native simulation now uses the reactor-room segments from the log, real path
  following and physics, and compares the live escort/path decisions with frozen
  Redux functions on every frame. It runs 960 frames for each of two approximate
  player trajectories, in both single-player and co-op modes
- With sampled segment centers, both implementations choose exit at frame 111
  (about 1.85 seconds). With an earlier retreat, both choose exit at frame 402
  (about 6.7 seconds). Each approximation was repeated with identical output
- This verifies return-delay sensitivity and matching Redux decisions/movement
  under those inputs. It does not reproduce the exact 15.575 seconds: the log
  lacks the continuous positions and visibility needed to replay that encounter
- A separate far-owner test covers visibility 0/1/2 and path ages through 16 seconds,
  verifies the Redux return gate, and compares subsequent rejoin behavior

The reasonable approximation is that a retained return path delayed selection,
and persistent presentation misrepresented that interval as still finding the
reactor. A distinct full-AI/physics or co-op defect causing the precise duration
has not been proven. No special reactor delay has been introduced to mimic the log

### Restorations implemented

- Removed the previous immediate-exit/key intent changes and restored Redux
  objective order, return gate and completion semantics. The earlier co-op owner
  inventory fix remains intact and is still covered by real pickup tests
- Retained bounds checks for invalid completion event/target object indices,
  including the fuel-center sentinel during another command. These prevent
  undefined accesses without changing valid Redux completion rules
- Persistent Classic status now reports a silent return without changing goals,
  path creation, transient speech or simulation clocks. Recall and Scram keep
  their own behavior; non-owner peers cannot overwrite the owner's message
- Classic paths of up to 128 points retain their full prefix. A new 124-point
  test failed before this fix and matches Redux after it. Larger paths retain
  compaction because save/network cursors remain signed bytes; runtime cursors
  are already shorts. This is an explicit safety exception, not full long-path
  Redux parity. Enhanced retains its prior compaction behavior
- Restored the existing one-second navigation trace for Classic. Its early exit
  previously skipped distance/visibility/velocity/path-point diagnostics while
  retaining only occasional reset logs. This adds no planner execution
- Extended the native runner with -D1InD2 so the reported mission/level can be
  tested in addition to D2 levels 1 and 11, including save/load continuity

### Test design and validation record

- Before restoration: the new parity test failed all 12 far-return cases,
  countdown selectors affected by the immediate-exit patch, and the 124-point
  prefix case. These were observed failures, not inferred test coverage
- After restoration: D2 levels 1/11 and D1-in-D2 level 2 comparisons pass; the
  mission simulation compares 3,840 frames per run, repeated twice
- HUD tests cover cache-only return updates, non-owner exclusion, and Classic
  Scram preservation. Existing owner inventory/pickup and mode/save tests remain
- Final Windows D2 and Android debug builds passed
- Final native Redux comparisons and mode/save/replay tests passed twice on D2
  levels 1/11 and D1-in-D2 level 2, including the reactor-room approximation
- Final same-mode save/load continuity passed twice on all three levels for both
  modes, four shared commands and Enhanced Unexplored
- Six focused CTest suites passed: goal messages, route decision/certifier, owner,
  exit and goal policy
- Android routing test passed 58/58 steps; persistent-message test passed 36/36
  steps. These ran the final HUD/navigation changes; the subsequent bounds-only
  completion guard was covered by the final native run and both rebuilt targets
- Scoped formatting/lint and git diff whitespace checks passed

The requested investigation and appropriate restorations are complete to the
reasonable-approximation scope above. Exact flight replay and a full two-device
reproduction are not claimed

### Verification scope

The room approximation holds the player at selected segment centers, supplies
visibility from the live line-of-sight query, steps escort/path functions, and
advances the bot with live physics. It does not replay player flight inputs,
other actors, reactor door-trigger animation, or the complete do_ai_frame loop.
Its co-op runs exercise owner-side mode flags, not two networked devices. These
limits are why the result is an approximation rather than a claim to reproduce
the exact observed delay. The small four-point logged path cannot trigger the
separate 120-point compaction defect

## Findings

### 1. The baseline contract was weaker than the label

The original checkout README identifies the code as Descent II 1.2, released as
source on December 14, 1999. It is useful evidence of original behavior, but not
proof of every shipped 1996 executable's behavior. The current oracle generator
pins a September 2026 Redux commit. Tests called "original navigation" establish
selected Redux function parity inside the current engine, not full 1996 parity

A source comparison of the extracted reference functions, normalizing comments,
time/RNG names and diagnostics, found that the original source and Redux retain
the same key selector, goal path creation, exit lookup and return cadence. Thus
the old selector and delay are not merely suspected Redux additions. Movement
does contain substantive port changes, detailed below

### 2. The blue-key incident was a real reintroduction in a new environment

- bee35b2e replaced the object-flags selector with inventory-aware selection
- 80af2244 (September 26) restored the old ConsoleObject->flags checks in the
  Classic branch while separating Enhanced routing
- The same commit's selector test compared directly against Redux, including
  the same incorrect object-flags dependency, so it approved that restoration
- 9a615a5f restored inventory-based selection and added real co-op pickup checks

The original single-player environment normally removes a collected key from
the mine, which masks the faulty flags check. Co-op keys remain available to
other players. Literal source restoration therefore broke an existing co-op
adaptation. Original ESCORT.C explicitly rejects the multiplayer Guidebot menu
with "No Guide-Bot in Multiplayer!"; co-op Guidebot necessarily needs an adapter

Restoration implication: retain owner-inventory handling for co-op. Document and
test any single-player departure separately, including duplicate keys and key
inventory already held when a level starts. Do not treat copying an old faulty
field access as sufficient proof of authentic observable behavior

### 3. The reactor symptom is not established as an AI regression

Original ESCORT.C checks blue, gold and red keys before reactor destruction in
escort_set_goal_object. Its do_escort_frame refreshes intent every five seconds
but defers selecting a new objective while returning to a distant player. The
supplied log's delayed exit path is consistent with that logic

Persistent goal text is a newer feature. Retaining the last reactor message while
the bot returns to the player exposes stale presentation that the original
transient message did not continuously display. This should be treated separately
from whether the bot's original decision timing should change

The preceding turn's uncommitted patch changes both decision timing and priority:
it immediately selects exit, promotes exit above uncollected keys, and refreshes
intent after key acquisition. Those are improvements under a responsiveness
contract, but departures under a strict original-behavior contract. The selector
test was also changed to expect exit instead of the original reference result
after destruction. Passing that test does not establish fidelity

Restoration implication: restore original automatic objective ordering and
decision timing; solve stale persistent presentation without feeding a new goal
back into navigation. Keep any intentional behavioral exception explicit

### 4. Long-path handling leaks a shared routing change into Classic

d2/main/aipath.c, ai_follow_path, discards the consumed prefix when a companion's
forward cursor reaches 120. ae58b368 introduced this for active route goals;
8b6a45a4 (September 16) broadened it to all companion paths, including Classic

This prevents signed-byte cursor overflow, but it changes more than storage:
time_to_visit_player compares cursor against path_length/2, and the endpoint
patrol logic uses the first point as the opposite end of the path. Removing the
prefix changes those meanings. For example, advancing cursor 120 on a 160-point
path produces cursor 1 on a 40-point suffix instead of cursor 121 on the full
path. Their midpoint predicates differ, and the original starting endpoint is
no longer available for a later reversal

This is confirmed source-level behavioral drift, not a reproduced explanation
for the supplied reactor log. The parity test uses path depth 40 and does not
explicitly exercise the 120-point boundary

Restoration implication: add a deterministic long-path comparison and preserve
Classic's full-path midpoint and endpoint semantics while retaining memory/index
safety. Blindly removing the overflow protection would be an unsafe restoration

### 5. "Original" steering currently means Redux steering

d2/main/guidebot_routing.c, guidebot_original_path_smoothing, deliberately matches
Redux's floating-point frame-time adjustment. Original AIPATH.C simply adds
half the target direction per frame. At roughly 60 Hz the Redux formula adds
about a quarter, while the released source still adds half

Path polish throttling also uses d_tick_count rather than original FrameCount.
Both differences predate the recent Enhanced work in the port lineage. They may
be useful adaptations for modern frame rates, but are not literal 1996 behavior

Restoration implication: define the original simulation cadence and validate
steering at that cadence before replacing formulas. Restoring a per-frame
constant alone at arbitrary modern frame rates can produce another mismatch

### 6. Completion safety and behavior changes were bundled together

The previous patch fixes out-of-range object indexing, but also changes defined
outcomes: alternate copies of a key can complete a goal; object collisions no
longer complete a fuel-center or exit goal through index aliasing; completed
commands immediately publish a new goal message. The enum/index and fuel-center
logic exists in the original source as well as Redux

Restoration implication: distinguish bounds safety from intentional changes to
defined original behavior. Safety does not justify claiming all completion
changes are faithful restoration. Original undefined accesses cannot serve as
portable behavior to reproduce

## Isolation reviewed

- Enhanced route frame/replanning/event/completion code has Classic early exits
- escort_route_follows_objective explicitly requires Enhanced mode; waypoint
  repair, segment crossing gates, precise approach and contact recovery use it
- Classic bypasses the Enhanced near-player return suppression, path recalculation
  limiter and Android velocity averaging
- Classic retains short-path wandering and endpoint patrol; scripted endpoint
  stopping is restricted to the route confirmation driver
- Co-op ownership, pose replication, owner key/door policy and owner retry recovery
  are shared extensions. They require separate integration coverage, not blind
  comparison with a game that did not support this co-op Guidebot
- Dock/deploy/recall, persistent HUD and per-player powerup visibility are additional
  explicit extensions. Disabling the Enhanced planner does not remove them

No evidence from this review supports a claim that all Enhanced hooks currently
affect Classic. The long-path branch is a concrete exception worth addressing

## Why the tests missed this

1. Reference provenance: Redux parity was treated as original-game fidelity
2. Environment mismatch: old single-player assumptions were restored into co-op
3. Incomplete boundary coverage: the oracle omits do_ai_frame, physics, collision,
   pickup and network orchestration; both branches share current geometry/math
4. Fixtures: robots are deleted, the bot is forced released, boss goal-path cases
   are skipped, frame cases use near-player distance, and long-path boundaries
   are not explicitly exercised
5. Presentation state is deliberately excluded from Snapshot::same, so navigation
   parity cannot validate persistent HUD accuracy
6. Recent event tests validate requested new behavior, not original parity. The
   separate 120-frame save/load test proves continuity within the current engine,
   not equivalence to a 1996 executable

## Recommended restoration sequence

1. Separate original single-player decision rules from a documented co-op adapter
   and optional presentation/features; list every allowed exception
2. Revise the preceding reactor patch to preserve original AI timing/order and
   independently correct persistent presentation. Reassess completion changes
3. Reproduce and repair long-path midpoint/patrol drift without cursor overflow
4. Pin the original 1.2 source as a second reference; retain Redux comparison but
   label its scope accurately. Use shipped-executable observations where source
   behavior, frame cadence or undefined accesses leave ambiguity
5. Expand parity cases to reactor/key events, far and unseen owners, commands and
   completion, long path reversal, cage release, bosses, full AI/physics stepping,
   and two-peer owner/non-owner key events
6. Keep new desired-behavior tests separate from parity tests. A fidelity mismatch
   must be reported rather than silently replaced with a new expected result

## Work performed and limits

Read source and commit diffs, compared original and Redux reference function
bodies, and reviewed current test fixtures and mode guards. No additional game
code was changed in this investigation. Prior-turn runtime test passes remain
valid for their stated scope; no new runtime fidelity test or 1996 executable
comparison was run. The earlier uncommitted patch is still present for review
