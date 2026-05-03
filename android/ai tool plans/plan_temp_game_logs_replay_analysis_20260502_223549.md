# Plan: Analyze replay desync for d2_descent2_level2_20260502_223549 (2026-05-02)

## Goal
- Find earliest reliable desync point and likely root cause using state trace, RNG trace, and probe logs

## Steps
- [x] Run replay state trace compare for the demo and record first mismatch frame
- [x] Run replay RNG trace compare and record first mismatch call/frame context
- [x] Correlate mismatch point with probe logs and game logs to identify the divergent system
- [x] Summarize likely root cause and recommend the next targeted instrumentation/fix

## Findings
- First meaningful RNG divergence appears at frame 425, call_count 24872
- Expected consumer at that call: create_random_xlate (aipath.c:337)
- Actual consumer at that call: create_awareness_event (ai.c:1820)
- RNG state_before/state_after match at call_count 24872, so the break is call-site ordering, not RNG generator state corruption
- Around frame 425, expected record logs show create_n_segment_path for obj 53 consuming calls 24871->24878 first, while replay trace shows one awareness-gate draw inserted earlier and subsequent path/awareness draws shifted
- By frame 426, record logs already show awareness events queued (events=3), implying both runs execute awareness draws in that window but in different order relative to path generation

## Root Cause Hypothesis
- Desync is caused by an ordering difference between awareness-event RNG consumption and AI path RNG consumption in frame 425
- Most likely trigger path is create_awareness_event invoked from weapon collision handling (weapon-wall or weapon-robot) in collide/laser code, happening earlier on host replay than in the recorded run
- That reassigns identical RNG values to different systems (awareness gate vs path xlate), producing immediate AI behavior drift and eventual large state/result mismatch
