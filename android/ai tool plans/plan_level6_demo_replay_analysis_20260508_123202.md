# Level 6 replay analysis 2026-05-08 123202

## Goal
- determine why the new D2 level 6 save-checkpoint demo fails to replay to completion
- identify whether this is the same hidden checkpoint-state issue as the earlier level 6 failure or a different divergence path

## Steps
- [in progress] locate the exact demo file and replay result artifacts for the failing run
- [not started] identify the earliest meaningful replay divergence using result and state-trace outputs
- [not started] compare the divergence against the prior level 6 checkpoint-state failure pattern
- [not started] decide whether the next action is diagnosis-only or a targeted code fix

## Notes
- initial hypothesis: the replay is diverging much earlier than the previous frame-1045 projectile case, since the final actual result stops at frame 445 instead of 2214, so the controlling mismatch may be a different local checkpoint or control-flow issue
- cheap check: inspect the runtime wrapper result directory and state-trace mismatch output to find the first failing frame and whether the replay stops on a state mismatch, an automation stop, or an unexpected early terminal condition
