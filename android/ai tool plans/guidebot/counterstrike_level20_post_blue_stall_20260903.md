# Counterstrike level 20 post-blue-key stall

## Goal

Diagnose and fix the deterministic GuideBot route simulation stall after the blue key in Counterstrike level 20 without regressing previously confirmed base-campaign routes.

## Phases

- [ ] Reproduce the current headed observation in a focused headless run and identify the stalled path edge, wall state, and objective
- [ ] Compare the physical path with the strategic route and determine whether the failure is targeting, path polishing, clearance, or world-state activation
- [ ] Implement the narrowest shared correction
- [ ] Add or extend a focused deterministic integration regression
- [ ] Validate level 20 repeatedly, run the full Counterstrike comparison, build D2, run CTest, and run scoped quality checks

## Findings

- Pending
