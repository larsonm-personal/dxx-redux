# GuideBot Level 21 halting route diagnosis

## Goal

Use the supplied cooperative D2 Level 21 debug log to identify why GuideBot
repeatedly makes brief forward progress and then reverses or rebuilds its route.

## Plan

- [done] Locate the exported log and isolate GuideBot route, ownership,
  invalidation, adoption, path, and recovery events.
- [done] Reconstruct the event timeline and quantify repeated reasons,
  destinations, path generations, and timing.
- [done] Trace the dominant log sequence to the current GuideBot source and
  distinguish root cause from secondary recovery behavior.
- [done] Record the diagnosis, confidence, and narrowly scoped fix direction.

## Results

- The matching export contains 415 GuideBot records: 298 navigation samples,
  112 path records, and four route-adoption records.
- All four route adoptions selected action 1, retained the existing path, and
  kept identical targets. The previous route-adoption fix is working, but it
  addressed a secondary mechanism rather than this regression.
- GuideBot created 107 objective paths in about five minutes. Ninety-five were
  long paths of at least 16 points, seven were short paths, and five failed
  with zero points and used the existing short-path fallback.
- Navigation spent 158 samples in `AIM_GOTO_PLAYER`, 80 in generic
  `AIM_FOLLOW_PATH`, only 40 in `AIM_GOTO_OBJECT`, and 20 in `AIM_STILL`.
  There were 23 sampled transitions from objective mode to player mode and 22
  back from player mode to objective mode.
- The route target stays active and unchanged across those transitions. For
  example, a valid 58-point path to segment 97 at index zero is replaced by a
  four-point player path within the next sample. The same pattern repeats for
  target segment 364 with paths of 65 to 74 points.
- No stalled-edge recovery occurred, stall samples remain zero, and no route
  adoption requested replacement or stop. This rules out the grate recovery,
  certificate audit, and route decision adoption as the reset source.
- The dominant reset is the original escort `time_to_visit_player` behavior in
  `do_escort_frame`, which is still allowed to replace an active semantic route
  with a path back to the player. Returning near the player immediately creates
  the same objective path again, producing the observed turn-around loop.
- The five zero-length objective paths and generic fallback paths are secondary
  failures. Frequent full-path recreation increases path-pool churn and makes
  these failures more visible, but they are not the primary loop.
- The narrow fix is to defer the classic visit-player and periodic goal-repath
  branches while an Android semantic objective path still has a pending step.
  Resume classic recovery when the semantic path is exhausted, invalid, or has
  already fallen back. This preserves the existing invalid-route and stall
  recovery paths.
