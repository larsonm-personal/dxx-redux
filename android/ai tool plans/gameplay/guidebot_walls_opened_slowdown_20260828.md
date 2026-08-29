# Obsidian walls-opened slowdown investigation

## Goal

Identify and fix the visible stall at Obsidian level 13 trigger 1 when `Wall opened!` is displayed.

## Plan

- [x] Correlate the trigger message with slowdown detector samples and GuideBot calculation traces
- [x] Trace the corresponding wall-change and route-recalculation code paths
- [x] Determine whether an implementation change or additional instrumentation is warranted
- [x] Add focused wall-trigger timing and a reproducible Obsidian level 13 automation scenario
- [x] Reproduce the costly viewpoint and identify the dominant synchronous phase
- [x] Prevent the slowdown recorder cooldown from hiding a distinct hard stall
- [ ] Use a physical-device capture to identify the expensive renderer subphase

## Findings

- The late switch event is the final log line at `10:53:26.392`: `route_trigger_latched trigger=1`
- The slowdown recorder ended capture 1 at `10:52:17.608` and entered a 300-second cooldown, lasting until approximately `10:57:17`; it therefore could not start a capture for the reported event
- Obsidian level 13 has no placed GuideBot (`guidebot_count=0`), and there are no GuideBot navigation or route-work records during this level. The trigger latch is a global event hook and does not imply route calculation
- `escort_route_record_event` does not dirty route metadata without an active semantic goal or unexplored target, so trigger 1 scheduled no GuideBot recalculation here
- Earlier level-12 trigger 6 did cause a route refresh, but it measured 5.876 ms. The largest earlier route refresh overrun in this file is 20.330 ms at `10:26:44.304`; neither is temporally related to the late level-13 wall switch
- The remaining plausible paths are synchronous open-wall processing and newly exposed rendering or texture loading, but this log cannot distinguish them because no profiling capture was active and the file ends immediately after the trigger latch
- The focused trigger-1 reproduction identifies three linked walls in segment 284 (sides 2, 4, and 5). Their actual transition setup totaled 0.323 ms with 0.002 ms of stuck-object cleanup; route metadata updates remained approximately 1.5--2.5 ms
- With the player facing side 2 in segment 284 and all robots intact, rendering repeatedly cost 106--129 ms. A 123 ms render frame followed `Walls opened!`, but 107--134 ms render frames also occurred at the same viewpoint before the trigger. Simulation remained 0.3--4.4 ms
- The emulator result therefore rules out the wall switch, GuideBot, route metadata, and robot simulation as the direct synchronous cause. It narrows the phone regression to rendering that view, but the current frame record does not subdivide renderer CPU work enough to pick a safe optimization
- Cooldown now still suppresses recurring sustained-slow captures, but a distinct hard stall of at least 250 ms starts a new capture. Severe history is cleared when a capture ends so a capture cannot retrigger itself

## Validation

- Android debug APK built successfully before focused reproduction
- Obsidian level 13 trigger test passes both at level start and while facing segment 284 side 2
- `test_android_slowdown_detector.exe`: pass, including hard-stall-during-cooldown coverage
