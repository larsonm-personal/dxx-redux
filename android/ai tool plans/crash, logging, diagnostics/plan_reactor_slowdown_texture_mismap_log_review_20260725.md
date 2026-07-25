# Reactor slowdown and texture mismatch log review

## Scope

Review the supplied Android debug log for evidence explaining:

- slowdown near a reactor and its projectiles
- a hidden-door-adjacent wall displaying the wrong texture

The initial work was a read-only diagnosis. The follow-up implements the
instrumentation requested for the next automatic slowdown capture.

## Plan

- [x] Read repository instructions and preserve unrelated working-tree changes
- [x] Extract profiling captures and identify the dominant slowdown subsystem
- [x] Trace reactor, robot, projectile, and object activity around the slowdown
- [x] Trace invalid texture references and hidden-door texture events
- [x] Correlate the evidence with the responsible source paths
- [x] Report findings, confidence, and the most useful next instrumentation

## Findings

### Reactor-area slowdown

- The automatic profile capture starts at 11:25:22.272 on D2 level 9 with
  `viewer_seg=506`, near the level 9 reactor in segment 492.
- Its apparent 4.983 FPS trigger comes from the cooperative restore:
  - the flight history contains one 851 ms interval during `StartNewLevelSub`
  - the next two-frame history bucket attributes 812 ms to non-wait work
  - after restore, every full reactor-area profile window is approximately the
    configured 25 FPS cap
- During the reactor-area capture, average non-wait work is generally below
  1.1 ms per frame. The largest individual non-wait time is 3.165 ms, well
  below the 40 ms frame budget at 25 FPS.
- The reactor is explicitly identified as the maximum-cost rendered object on
  several frames (`max_type=9`, `max_model=103`). Its measured render cost is
  only 44 to 111 microseconds.
- Object draw counts rise from 5 to roughly 39 in the busier reactor frames,
  but FPS remains capped and simulation/render time remains small. The capture
  contains no projectile texture load or other event implicating reactor
  shots.
- The only later low-FPS profile window is the D2 level 10 transition:
  21 frames over 2.032 seconds, plus first-use texture uploads. This is loading
  and paging, not reactor combat.
- Therefore this log does not support the reactor model or its projectiles as
  the cause of sustained slowdown. It captured a restore hitch followed by
  normal capped performance. It cannot rule out an earlier pre-restore
  slowdown that was already gone when the capture began.

### Texture mismatch

- The four D2 level 9 `tmap=910` diagnostics are the same known authored
  `WALL_OPEN` portal-side values in segments 491, 493, 495, and 496. They are
  non-rendered fields and are unrelated to the visible mismatch.
- D2 level 10 loads with stable texture and segment signatures and no invalid
  texture diagnostic.
- The log identifies the likely observed hidden door as clip 45 between
  segment 13 side 5 (wall 130) and segment 1 side 4 (wall 129).
- Clip flags `0xc` are `WCF_TMAP1 | WCF_HIDDEN`, so this animation writes the
  primary texture directly and does not use the Android merged-wall cache.
- Every recorded event shows both connected door sides using the same primary
  texture. They begin at frame 849, later reach frame 850, and return to 849
  when the door is opened again. The log does not show texture 849 or 850
  assigned to any adjacent rock side.
- There is a real one-sided wall-flag mismatch: wall 129 is initially `0x98`
  while wall 130 and the incoming packet are `0x90`. The difference is
  `WALL_DOOR_LOCKED`. `multi_do_door_open()` overwrites only the addressed
  wall's flags, so paired sides can retain different lock flags. This is a
  cooperative door-state defect, but the flag assignment does not itself
  change a texture index.
- No face snapshot or ordinary-primary-face trace was taken when the visual
  mismatch appeared. The log therefore narrows the issue to the primary hidden
  door / ordinary face path, but cannot identify the adjacent face or prove
  whether its side data changed versus the renderer binding the wrong texture.

## Most useful next capture

- Add a compact primary-wall trace for clip-45 door animation updates that
  records the target segment/side pair and the immediately adjacent rendered
  sides before and after `wall_set_tmap_num()`.
- Add a tap/snapshot path for ordinary primary-only faces. Existing merged-wall
  snapshot diagnostics do not cover this `tmap_num2=0`, `WCF_TMAP1` case.
- Add reactor combat counters to each profile frame: live object counts by
  robot, weapon, fireball, and control-center type, plus reactor shots created
  and processed. This would directly test the projectile-density hypothesis on
  a future occurrence.

## Follow-up: visible frame rate versus recorded frame rate

- [x] Audit how the profiler derives frame count, wait time, and FPS
- [x] Locate multiplayer polling, packet processing, and any blocking waits
      relative to the profiler boundaries
- [x] Determine whether network lag can reduce distinct visual updates while
      the profiler still reports capped frame cadence
- [x] Correct the interpretation and specify instrumentation that measures the
      visible symptom directly

### Corrected interpretation

- The recorder counts completed game draw callbacks using their wall-clock end
  timestamps. This measures engine draw cadence, not distinct scene poses and
  not frames actually presented by Android's compositor.
- `gr_flip()` runs after `android_profile_frame_end()`. Its elapsed time affects
  the interval before the next recorded frame and therefore the one-second FPS
  calculation, but it is absent from that frame's `total_us`. The logged
  `swap_us` is the previous flip's value.
- The window's three "worst frames" are ranked by `total_us - wait_us`, not by
  the longest presentation interval. A short present/compositor hitch can be
  averaged into a one-second window and omitted from the printed worst-frame
  details.
- Remote robot positions are network updates rather than locally authoritative
  AI poses. Updates are sent through the robot mdata path, ordinarily serviced
  at 10 Hz, and receive applies a new `shortpos` directly before reconstructing
  thrust from velocity. There is no general interpolation buffer.
- Thus a robot-heavy view can show repeated or corrected remote poses at a much
  lower effective visual-update rate while the renderer continues drawing at
  25 FPS. The existing recorder calls all those draws distinct frames.
- UDP polling itself is nonblocking (`select()` with zero timeout), so ordinary
  latency does not make the frame thread wait for a packet.
- However, `net_udp_listen()` drains every currently queued datagram and
  processes every packet synchronously inside `GameProcessFrame()`. There is no
  packet-count or elapsed-time budget. Latency, batching, or scheduling can
  therefore turn delayed robot traffic into a main-thread processing burst.
- For the host in this log, the local camera, locally owned robots, and reactor
  simulation should not freeze merely because a peer packet is late. Network
  lag is a strong candidate if the visible symptom is remote robots moving in
  coarse steps. It is a weaker explanation if the local camera and all level
  geometry visibly present at under 10 FPS together, unless packet bursts are
  stalling the main thread.
- A correct follow-up recorder must log:
  - draw-begin to next-draw-begin and flip-begin to flip-end intervals
  - a monotonically changing scene/simulation identifier
  - current `FrameTime`
  - packets received, packets processed, and network-processing microseconds
    per frame
  - live robots by owner and per-robot time since last received position
  - Android presentation timestamps if the EGL/platform path exposes them

## Implementation: capture the visible slowdown correctly

- [x] Preserve and inspect overlapping uncommitted multiplayer changes
- [x] Extend the shared slowdown frame/window schema with cadence, simulation,
      network, and remote-robot freshness metrics
- [x] Measure draw-to-draw and flip-to-flip cadence independently of CPU work
- [x] Measure per-frame UDP packets and synchronous packet-processing time
- [x] Summarize remote robot ownership and position-update age in D1 and D2
- [x] Improve trigger/worst-frame selection for brief cadence stalls
- [x] Extend detector unit tests for cadence stalls and new aggregate fields
- [x] Run scoped formatting, focused tests, Windows builds, and diff checks

### Implemented capture fields and behavior

- Automatic capture records are now `prof_v=3`.
- Each frame records `begin_gap_us` and `flip_gap_us`, while the existing
  `swap_us` continues to record time spent in the flip/swap path. This exposes
  cadence stalls that the old CPU-work ranking could hide.
- Each frame records a monotonic simulation frame identifier and `FrameTime` in
  microseconds, allowing repeated presentation/draw activity to be separated
  from simulation advancement.
- UDP queue draining records processing microseconds, datagram count, and byte
  count per frame. Since every received datagram is synchronously passed to the
  packet processor, the datagram count also describes the processed burst.
- Robot state records local versus remotely owned live counts, remote position
  update count, stale remote count, unknown-age count, and maximum remote
  position age.
- Scene-density fields record active objects, weapon/projectile objects, and
  control-center/reactor objects on the same frame as the slowdown metrics.
- Worst-frame selection now ranks the maximum of non-wait CPU time,
  draw-begin cadence, and flip cadence. One cadence stall of at least 250 ms
  triggers a capture; the existing three 100 ms stalls within two seconds also
  remain a trigger.
- The rolling history was reduced from 1024 to 768 frames to keep the detector
  below its 128 KiB memory budget after adding the new fields. It still retains
  at least 6.4 seconds at 120 FPS, exceeding the five-second history written to
  a capture.
- Exact Android compositor presentation timestamps are not available in the
  current EGL wrapper. `flip_gap_us` plus `swap_us` measure the application-side
  presentation cadence, but do not claim that SurfaceFlinger displayed every
  submitted buffer.

### Validation

- Scoped code-quality checks passed.
- Android native debug builds passed for `arm64-v8a`, `armeabi-v7a`, and
  `x86_64`.
- The focused slowdown-detector test passed in both D1 and D2 Windows build
  trees, including the single-hard-cadence-stall trigger, network/robot
  aggregation, and memory-budget checks.
- D1 and D2 Windows game executables compiled and linked.
- The aggregate Windows build remains red because the unrelated
  `test_coop_player_session` target lacks required SDL/PhysFS include paths.
  This target fails before linking in both build trees; the changed games and
  slowdown tests still link successfully.
- `git diff --check` reported no whitespace errors. Its only output was
  pre-existing line-ending conversion warnings on modified D1/D2 files.
