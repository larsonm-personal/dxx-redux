# Plan: input demo replay analysis 20260507_210511

## Goal
- determine whether `android/regression_demos/d2_descent2_level6_20260507_210511.dximdemo` reproduces the robot-behind agitation-path desync or a different divergence

## Steps
- [in progress] rerun the demo with preserved sandbox output and inspect the recorded/replay agitation-path probes
- [not started] identify the earliest meaningful replay divergence using the existing result, probe, and trace outputs
- [not started] decide whether the controlling path matches the known agitation-path gate or a different local cause
- [not started] implement and validate the smallest fix if the control path is clear

## Notes
- the demo is a D2 level 6 save-checkpoint replay failing with broad player, position, and level-summary divergence by frame 3777
- prior D2 level 3 investigation established a specific agitation-path gate probe for behind-behavior robots; this run may or may not hit the same path