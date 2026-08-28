# GuideBot gold-key stale objective log review

## Goal

Determine why GuideBot retained the gold-key objective after the player picked
up the key, using the supplied Android debug log and current route-state code.

## Work

- [x] Correlate the pickup, route notifications, live certification, and goal
  publication in the supplied log.
- [x] Compare the observed sequence with key-state invalidation and adoption
  logic.
- [x] Identify the root cause and propose a narrowly scoped correction with
  regression coverage.

## Results

- The log is Obsidian level 12 (`lususfls.rl2`), build 21282. At
  23:01:09.797 the live view changes from blue-only keys (`0x1`) to blue plus
  gold (`0x5`), and the replan reason is explicitly `key_change`. The pickup
  notification therefore worked.
- Before the pickup, the published route is canonical step 6 at segment 277,
  the gold-key objective. After the pickup, the certifier correctly skips that
  completed step but rejects the new first-required canonical step 7 at
  segment 567 as unreachable (`blocking_reason=2`).
- Android gameplay currently forbids the full-planner fallback. The invalid
  certification is therefore recorded as `full_plan_forbidden`; the live
  metadata layer retains its previously valid incumbent, which still describes
  step 6. Goal selection reads that incumbent, and adoption consequently logs
  `action=1`, `same_guidance=1`, and target `277 -> 277`. This is why the HUD
  continues to say it is finding the gold key.
- The result is not stable. From the pickup through the end of the log, the
  identical step-7 rejection occurs 62 times, all 62 attempts retain target
  277, and all 62 reachability passes overrun. Refresh time is 7.4-22.4 ms,
  averaging 17.4 ms, so the stale-objective bug also causes a pathological
  foreground recertification loop.
- The narrow correction should treat completion of the incumbent objective as
  a hard invalidation: never republish or adopt that incumbent after its key,
  trigger, object, reactor, or boss completion predicate becomes false. When
  canonical certification then finds a different required step unreachable,
  schedule a resumable/budgeted next-objective replan rather than the currently
  forbidden synchronous full planner. Cache the completed negative
  certification by route-input hash so unchanged state does not retry it.
- Regression coverage should model a key pickup where the next prepared target
  is not currently certifiable. It should assert that the key objective is
  removed immediately, the bounded replan advances to the prerequisite or
  publishes a calculating/frontier state, and repeated ticks with unchanged
  inputs neither restore the key objective nor repeat certification work.
