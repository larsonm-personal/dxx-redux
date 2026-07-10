# Guidebot save restore route goal reset

## Goal
Make restored saves recompute the guidebot's metadata route goal instead of reusing a stale pre-restore boss/reactor/exit goal.

## Plan
- [x] Inspect save restore and guidebot goal lifecycle
- [x] Patch restore normalization to clear stale runtime guidebot goals on normal save restore
- [x] Use existing hidden-door route coverage as the focused regression
- [x] Run scoped quality and relevant build/test checks
- [x] Guard the runtime goal reset out of input-demo replay restores to preserve deterministic save/checkpoint state
