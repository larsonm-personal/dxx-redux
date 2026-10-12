# Compact public robot-combat replays

## Goal

Record useful robot combat from ordinary Descent 1 shareware and Descent 2 demo spawns with normal controls, authentic AI, difficulty and starting weapons

## Plan

1. Find naturally reachable early robots and develop short routes with headed Xvfb screenshot inspection
2. Capture identified projectile damage, robot death, exact score/kill awards and justified loot outcomes using read-only observations coupled to full object/world state
3. Independently repeat live captures, admit immutable frame/diagnostic/terminal/SIM oracles, and keep each replay plus oracle at or below 100000 bytes
4. Test true headless and headed playback, strict outcome admission and deliberately broken combat input; preserve old oracles, imported limitations and retail coverage
5. Publish a focused draft stacked after the dual-backend segment and follow exact-head CI to a terminal result

## Constraints

No warps, injected poses, inventory or world state, embedded saves, altered AI, artificial combat tables or replay-specific compensation. Capture controllers may only supply ordinary player controls. A game without a compact authentic route remains explicitly unqualified
