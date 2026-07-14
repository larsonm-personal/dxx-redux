# Guide-Bot Phase 7 Dynamic Ownership Invalidation

## Goal

Begin Phase 7 by making live route invalidation follow the effective Guide-Bot owner and moving boss state without adding periodic planner scans or changing classic Guide-Bot movement.

## Plan

- [x] Make key-change invalidation player-aware so only the effective Guide-Bot owner's key inventory dirties the shared route.
- [x] Invalidate an active boss waypoint when the boss teleports locally or through a multiplayer boss-action packet.
- [x] Keep invalidation passive: clear only high-level semantic intent and let the existing classic path scheduling rebuild movement normally.
- [x] Make owner packet generations wrap-safe and reject zero, duplicate, stale, and implausibly old state packets.
- [x] Extend native owner-policy tests for generation wrap and owner-relevant key changes.
- [x] Expose enough counters/reasons through existing introspection to verify no nonowner planner execution and no repeated rescans.
- [x] Run scoped quality, D1/D2 builds and native suites, Android JVM/all-ABI build, and focused live tests where practical.
- [x] Update the master plan with results and identify the next Phase 7 cache/save/multiplayer tranche.

## Boundary

These hooks may invalidate shared high-level route meaning after a real world-state or ownership change. They must not construct paths immediately, alter AI timing, move or orient Guide-Bot, fire a flare, or consume simulation RNG.

## Results

- Key changes now carry the changed player slot through local pickup and multiplayer status paths. Only the effective Guide-Bot owner's key mask invalidates route meaning; ignored nonowner changes are counted for diagnostics.
- Local and replicated boss teleport paths invalidate an active boss semantic waypoint only on the authoritative Guide-Bot owner. The hook clears high-level goal state and performs no search, movement, AI timing, flare, or RNG work.
- Owner-state packet generations use wrap-safe serial arithmetic while reserving zero. Native tests cover initial, duplicate, stale, wrap, and reverse-wrap cases.
- The two-peer ownership fixture now verifies an ignored nonowner key change, transfer to the joiner, preserved Unexplored intent, disconnect adoption, and owner-local recomputation from a 193-segment automap component. Stale pre-unification expectations for a red-key/Reactor intermediate were replaced with stable direct-Unexplored assertions where the shared planner proves the endpoint reachable.
- Verification passed: scoped quality and automation-catalog checks, supported Windows D1/D2 builds, all 19 D1 and 22 D2 host tests, the 1,274-route corpus, base mission statuses, Android JVM tests, all configured Android ABIs, and the focused two-emulator ownership/handoff scenario.

## Next Tranche

- Feed wall, trigger, object, and reactor event generations into the live route-state cache key, rebuilding static topology only on level/geometry changes.
- Coalesce owner-local automap invalidation so a changed largest unexplored component replans without a per-frame scan.
- Verify disappearing progression objects and reactor/control-center state changes invalidate only relevant semantic work.
- Run observer-host, voluntary abdication, save/restore, slot-remap, and host-migration scenarios with planner-count and owner-authority assertions.
