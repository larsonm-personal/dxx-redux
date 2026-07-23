# Coop level 7 slowdown log review

## Goal

Determine why the automatic slowdown capture did not trigger during the reported
2 to 3 FPS slowdown near the level 7 reactor in a two-player coop game.

## Plan

- [x] Read repository instructions and identify the slowdown detector
- [x] Analyze the supplied export for settings, trigger, and timing evidence
- [x] Trace frame delivery and detector reset or suppression paths
- [x] Compare the observed slowdown with the detector's trigger conditions
- [x] Document the root cause and recommended correction

## Findings

- The 608-line export contains no `PROFILING`, `prof_v=2`, or
  `type=capture_start` record, so the automatic capture did not start.
- The export cannot prove whether the automatic switch was armed because the
  preference is not included in the log header and armed operation is
  intentionally silent.
- The likely miss is the detector's 500 ms discontinuity rule. Before recording
  the current frame, any gap at or above 500 ms clears the current one-second
  window and all prior severe-frame timestamps. A 2 FPS stream therefore resets
  on every frame, and a single multi-second rendering stall is discarded from
  the sustained window as though it were a pause or load.
- The late-level log has a 12.484-second gap between periodic in-memory save
  records at 20:40:11.167 and 20:40:23.651. This is consistent with the class of
  long draw gap that the detector suppresses.
- Existing tests cover a slowdown to 20 FPS and three 100 ms stalls, but do not
  cover 2 FPS, a frame at the 500 ms boundary, or a multi-second in-frame stall.

## Recommended correction

Distinguish a lifecycle gap from a slow in-progress frame. One low-risk rule is
to reset for a long callback gap only when the just-completed frame has little
non-wait work; if the frame itself contains severe non-wait time, retain it and
let the severe or sustained trigger evaluate it. Add regression cases for
500 ms frames, sustained 2 FPS, and a single multi-second frame. Also expose the
armed preference in an existing log header or status record so future exports
can confirm configuration without causing armed-mode file I/O.

## Requested implementation

- [x] Add an explicit delivered-FPS trigger below 8 FPS
- [x] Remove the 500 ms blind spot while retaining long lifecycle-gap handling
- [x] Validate the focused detector and Android integration build

Validation passed with scoped code quality, the D2 Windows build, the focused
slowdown detector executable, and the Android debug APK build for all configured
ABIs.
