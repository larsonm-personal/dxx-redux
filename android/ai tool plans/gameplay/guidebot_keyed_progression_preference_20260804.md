# Guidebot keyed progression preference

## Plan

- [x] Reproduce and trace the Descent 2 level 14 yellow-key recommendation
- [x] Define a general keyed-door preference for the next required progression goal
- [x] Implement the shared guidebot routing change
- [x] Extend high-level regression coverage
- [x] Run scoped formatting, builds, and relevant tests

## Findings

- The ordinary planner gave every missing-key and trigger-opened barrier the same
  progression cost, then selected the shorter route. Counterstrike level 14
  therefore chose triggers 13 and 5 and classified the blue key as unnecessary.
- When a top-level required target first routes through a trigger, the planner now
  retries with that trigger excluded. It selects the alternate only when the route
  has no more progression barriers and begins through a missing-key door.
- Alternate planning is capped at 2,048 segments so unusually large custom levels
  retain the bounded single-pass behavior.
- Counterstrike level 14 now routes blue key, trigger 10, gold key, red key,
  reactor, exit. Trigger 5 is absent and the blue key is no longer unnecessary.
- Full host regeneration completed 114 inputs, skipped one archive without a
  descriptor, and failed zero. Only Counterstrike metadata is retained to avoid
  mixing unrelated stale community-metadata refreshes into this change.
