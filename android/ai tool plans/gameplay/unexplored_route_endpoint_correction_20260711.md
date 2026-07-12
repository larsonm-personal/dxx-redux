# Unexplored Route Endpoint Correction

Date: 2026-07-11

## Goal

Make `Unexplored` use the same step-by-step dependency resolver as end-of-level routing, with the largest reachable contiguous unexplored region as its endpoint instead of an unconditional boss/reactor prefix.

## Plan

- [x] Separate common route initialization from end-of-level primary progression.
- [x] Select an unexplored endpoint from the initialized player or guidebot state.
- [x] Resolve only the keys, triggers, hidden doors, and other blockers required to reach that endpoint.
- [x] Add regression coverage proving an unrelated reactor is omitted.
- [x] Run scoped formatting, metadata scanner tests, desktop build, and Android build.

## Constraints

- Keep one shared dependency resolver for exit and unexplored routes.
- Preserve largest-component selection and deterministic tie-breaking.
- Preserve route-step ordering and live guidebot consumption.
- Do not add mission-specific exceptions.

## Result

- Common initialization now inventories keys and appends `Start` without moving to the boss or reactor.
- End-of-level routing explicitly performs primary progression before selecting the exit.
- Unexplored routing selects the largest candidate component from the initialized state and first resolves a direct dependency route to it.
- If direct routing fails, the same endpoint is retried through boss/reactor progression. This retains control-center links as real dependencies without forcing them into unrelated routes.
- Optimistic unexplored component discovery treats control-center links as potentially traversable so post-reactor regions remain eligible.

## Verification

- Direct unexplored regression: `Start -> Unexplored` with an unrelated reactor omitted.
- Control-center-link regression: `Start -> Reactor -> Unexplored` when destroying the reactor is required.
- Key and hidden-door unexplored regressions continue to pass.
- D1 and D2 metadata scanner tests pass.
- D1 and D2 Windows builds pass.
- Android `:app:assembleDebug` passes.
- Scoped code-quality checks pass.
