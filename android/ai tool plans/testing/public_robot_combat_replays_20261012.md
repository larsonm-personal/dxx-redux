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

## Validation before draft publication

- Captured from published source `f79e143af6db08f7f4237c3c89fc62f0007c1451`, after explicit CMake configuration; compiled recorder Git identity and executable/header hashes match provenance
- Two independent clean captures are byte-identical, including full recording, RNG trace, causal observations, report and packed replay/oracle pair
- D1: 148 frames, 25962 combined bytes, three authentic laser hits, one 200-point kill, resolved failed 3/16 energy-drop roll and original robot retirement
- D2: 263 frames, 48012 combined bytes, two authentic laser hits, one 200-point kill, zero-count authored loot rule and original robot retirement
- Fresh exact-source playback passes twice per game through both native console and headed backends, comparing every captured frame/diagnostic hash, terminal field, SIM event and causal observation
- Fresh rendered first-hit/death screenshots were inspected; rendered captures match the clean gameplay/diagnostic/SIM/causal oracles
- Earlier nine-scene development matrices passed twice per backend; the full headless wrong-oracle/bad-input wrapper suite and both backends' targeted no-fire combat controls rejected the intended failures
- Original seven replay/oracle pairs remain byte-identical; imported D1 retains its existing exact flight scope and explicit incomplete full-simulation qualification
- Source-before-corpus publication preserves reproducible capture provenance; exact-head CI remains required before claiming the stacked draft complete
