# Guidebot Navigation CPU Audit

Date: 2026-07-11

## Goal

Find and reduce expensive mid-level guidebot navigation recalculation, especially in large or geometrically complex KCXF2 levels, without delaying reactions to keys, doors, triggers, ownership changes, or explicit goal commands.

## Plan

- [x] Map every route and path recalculation entry point and its maximum call frequency.
- [x] Measure the asymptotic and practical cost of route metadata refresh, unexplored-region selection, firing-position search, and fallback pathing.
- [x] Separate level-static calculations from state-dependent calculations.
- [x] Add narrowly invalidated caching or throttling around confirmed repeated work.
- [x] Add diagnostics or focused tests for recalculation behavior.
- [x] Run scoped formatting, D2 build, and relevant tests.

## Constraints

- Preserve deterministic simulation behavior.
- Recalculate immediately after navigation-relevant world-state changes.
- Keep metadata and live guidebot traversal semantics aligned.
- Avoid mission-specific behavior.

## Findings

- Classic guidebot behavior clears its goal every five seconds. The Android route integration treated every such refresh as a full replan and reran the complete metadata route analyzer from the guidebot's current object.
- Unexplored routing is particularly expensive because it calculates progression prerequisites, groups every unexplored segment into components, and runs direct and optimistic mine-wide path searches.
- Next-step selection calculated live reachability for every preceding route step, including already satisfied steps. A shoot-switch, reactor, or boss reachability check can breadth-first scan the mine and test up to 115 collision rays per visited segment.
- Path creation then repeated the same firing-position search even when the objective and previously selected firing segment had not changed.
- Nearest-point fallback performs two additional mine-wide graph searches, but only after an actual path failure. It is not a periodic source of work.
- Full route analysis exposed through introspection remains intentionally detailed and can be expensive when explicitly requested. It is not called by the normal gameplay frame loop.

## Changes

- Full metadata scans are now dirty-driven. Level start, explicit commands, key changes, restore, guidebot spawn, ownership handoff, and multiplayer target-mode changes mark the route dirty.
- The classic five-second path refresh reuses existing metadata instead of forcing a rescan.
- Unexplored routing marks itself dirty when its selected target becomes explored.
- Normal next-step selection skips live reachability work. Path creation still validates the selected target and retains nearest-point fallback behavior.
- The same objective preserves its validated firing segment across ordinary path refreshes.
- Guidance refresh tests the existing metadata or cached firing segment before launching a mine-wide visibility search.
- Introspection now reports `route_metadata_rescan_count` and `route_guidance_full_search_count`, reset at level start.

## Verification

- D2 Windows build passed.
- D2 metadata scanner test passed.
- Android `:app:assembleDebug` passed, including the Android-only guidebot route code.
- Scoped code-quality checks passed.
