# Coop stutter logging

- [x] Add an independent isolated-hitch detector under Automatic slowdown capture
- [x] Retain the worst frame and its predecessor, with wall-clock cadence and major frame contributions
- [x] Add matching Android-only D1/D2 simulation timing hooks and update the setting description
- [x] Validate synthetic gameplay traces, both native engines, Android compilation, and scoped formatting

The existing recorder requires sustained low FPS, three 100 ms stalls, or one
250 ms stall. Add bounded stutter summaries independently of its capture and
cooldown. Detect wall-clock end-to-end intervals at least 50 ms and at least
1.5 times the learned normal cadence. Account for intentional low FPS and
ignore level/pacing transitions and resume. Report all hitch counts and the
worst sample per second, plus a ten-second heartbeat even when no hitches occur.

Reuse existing coarse timing and network counters. Add movement (including
per-object AI/physics/collisions), global AI, multiplayer frame work, sound sync,
effects/walls/triggers, and rewind timings. Keep nested timings explicitly
labeled; distinguish frame work from the time outside the game frame callback.
Retain the preceding sample because swap/event work can occur between callbacks.
Do not enable expensive per-object instrumentation to catch the first hitch.

This is evidence collection for D1-in-D2 coop level 6, not a speculative fix.

## Capture and interpretation

In the launcher Advanced settings, enable **Automatic slowdown capture**. The
manual Profiling category can stay disabled. Play the affected coop level, then
export the debug log from Advanced. Search for `stutter_v=1`.

- `type=window`: frame and hitch counts, mean/maximum end-to-end interval,
  counts at or above 100/250 ms, total excess time over baseline, and the
  threshold/baseline used for the worst frame
- `type=frame role=worst`: the worst sample's contribution breakdown, engine,
  level, segment, game mode, frame IDs, scene counts, and network traffic
- `type=frame role=before`: its immediate predecessor, retained for cross-frame
  attribution; these two lines are emitted only for windows containing hitches

All durations are microseconds of elapsed wall time, not CPU utilization. Frame
end-to-end cadence includes the interval outside the measured callback. For
the worst sample, `max_interval_us = total_us + outside_us` in ordinary monotonic
operation. Outside time includes event processing, presentation if it occurs
outside the callback, scheduling, and the preceding profiler/logging work.

Inside the callback, `total_us` is divided into `wait_us` (frame pacing),
`sim_us`, `render_us`, `replay_us`, `rewind_us`, and `other_us`. The remaining
timings are nested diagnostics and must not be added to those parent totals:

- `multi_us`: `multi_do_frame`, including receive/send processing
- `move_us`: `object_move_all`, including per-object AI, physics, and collisions
- `ai_us`: `do_ai_frame_all` global AI work, separate from per-object AI
- `sound_us`: main-thread sound synchronization, not the audio mixing thread
- `effects_us`: exploding walls, effects, wall and trigger processing
- `record_us`: input-demo recording diagnostics, within simulation or replay
- `net_us`: network receive-loop work, potentially overlapping multiplayer work

`latest_swap_us`, `latest_gpu_us`, `latest_resolve_us`, `latest_glerr_us`, and
`latest_flip_gap_us` are the existing renderer's latest measurements. They are
corroborating evidence, not additive current-frame contributions: swap can
occur outside this callback and GPU timer results can arrive later. The report
does not claim to measure actual display presentation timestamps or network
jitter that moves remote objects while local frame timing remains smooth.

The baseline is a 1/32 moving average of normal end-to-end intervals. The first
three seconds after enabling, resume, or a level/pacing/mode change warm up the
baseline without reporting hitches. Once armed, detected spikes cannot inflate
their own threshold. Gaps of 30 seconds or more reset as discontinuities.
Normal frames yield a heartbeat every ten seconds. A window containing hitches
is emitted once it spans at least one second; every hitch is counted, but only
the worst receives detailed lines. Exit and transitions flush partial windows.
This path has its own fixed-size state and does not consume the sustained
recorder's byte budget or honor its five-minute cooldown.

## Validation

- Windows D1 and D2 builds passed, including an incremental rebuild of both
  engines after the gameplay-focus reset hook
- Android arm64 Debug D1 and D2 native libraries built and linked; the final
  changed-file rebuild produced no warnings
- `test_android_slowdown_detector` passed through CTest in both host builds:
  existing sustained detection plus isolated 80 ms/90 ms stalls, exact worst
  and predecessor retention, subsystem contributions, 20-hitch bursts, 25 FPS
  pacing, transition/resume suppression, duration bins, and partial flush
- `:app:compileDebugKotlin` passed with JDK 21
- Scoped C/Kotlin/Markdown formatting and diff whitespace checks passed

No phone gameplay reproduction or on-device overhead measurement was performed.
The user's D1-in-D2 coop level 6 capture remains the evidence needed to identify
the cause. The change is built at the library/launcher compilation level; no
APK was packaged or installed for this task.

## Network attribution follow-up

The phone capture `debuglog_20261009_133136.txt` contains three 85-98 ms frame
intervals in 53.6 seconds. The receive/dispatch loop takes 68-81 ms with only
3-6 packets per affected frame. This local elapsed time does not establish
Wi-Fi congestion, packet-handler CPU cost, or an OS scheduling delay.

- [x] Add wall/thread-CPU measurements for polling, receiving, dispatch, sends,
      optional packet-file logging, and the complete listen loop
- [x] Retain the three slowest dispatched packets with engine-provided type
      names, socket, byte count, and both clocks for the worst frame and predecessor
- [x] Keep instrumentation conditional on automatic capture, preserve socket
      behavior/errno, and keep output bounded in the existing Profiling batch
- [x] Test aggregation, nested timings, missing CPU clock data, error metadata,
      packet ranking, and retained stutter snapshots; build both engines

The follow-up adds `type=net_stage` and `type=net_packet` lines to the same
`stutter_v=1` batch for the worst frame and predecessor. No extra switch or
continuous Network-category logging is required. Each report remains bounded
to one 16 KiB buffer and retains at most three individual packet handlers per
frame, ranked by elapsed time. Numeric packet IDs and names come from the engine.

Stage meanings:

- `poll`: the existing zero-timeout `select` call
- `receive`: `recvfrom` itself, excluding optional packet-file logging
- `dispatch`: one received packet's complete processing, including any nested
  proxy handling, responses, and logging
- `send`: `sendto` itself, including responses and ordinary frame sends
- `traffic_log`: the optional `LogNetTraffic` file logger, when enabled
- `listen`: the complete receive/dispatch loop, inclusive of nested stages

Each stage reports call/failure counts, elapsed and thread-CPU totals, and its
slowest call's elapsed/CPU time, socket descriptor, packet type/name, bytes,
return value, and errno. Dispatch has a void result; its failure count does not
represent rejected packets. Packet lines identify the three slowest dispatches,
which may have the same type. No packet contents or peer addresses are logged.

Large `wall_us` with similarly large `cpu_us` points toward local CPU work.
Large wall time with small CPU time means the thread was mostly not executing;
the stage narrows where to investigate blocking, scheduling, or contention.
CPU-clock failure is explicitly `cpu_us=-1`. Nested stages must not be summed.
These clocks cannot measure radio congestion or remote packet transit, and
Android's local forwarding proxy is also part of the transport path. Socket
flags, timeouts, packet processing, and scheduling are unchanged; the clock
hooks preserve errno and do no clocks or aggregation when automatic capture is off.

Network follow-up validation: Windows D1/D2 full builds and Android arm64 Debug
D1/D2 native builds passed. Both host CTest instances of
`test_android_slowdown_detector` passed the extended network attribution and
formatting cases. Scoped formatting/lint passed; native warnings were confined
to existing upstream sites. No APK was packaged or installed, and the next
phone capture is still needed to attribute the actual stall.
