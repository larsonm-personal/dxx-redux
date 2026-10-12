# Compact public door obstruction replays

## Goal

Record automatic door lifecycle behavior from ordinary Descent 1 shareware and Descent 2 demo starts, using only normal controls and preserving the existing nine recordings and oracles

## Plan

1. Develop compact control-only routes under real SDL/OpenGL Xvfb, inspecting screenshots and authored wall pairs
2. Prove original closed walls, opening, both collision flags, player obstruction beyond the automatic wait, clearance, natural closing, reopening and passage; preserve the D1 frozen-closing and D2 waiting/reversal differences
3. Add read-only geometric observations tied to full object/world state, strict semantic admission and deliberate bad-control rejection
4. Independently capture twice from published source, admit immutable frame/diagnostic/terminal/SIM and door observations within 100000 bytes per pair
5. Validate both true headless and headed playback and all previous cases, publish a focused stacked draft and follow exact-head CI to terminal

## Constraints

No warps, injected poses, inventory, health, world state, embedded saves, modified AI or replay-specific simulation fixes. Small waypoint/aim/hold capture helpers may supply ordinary controls only
