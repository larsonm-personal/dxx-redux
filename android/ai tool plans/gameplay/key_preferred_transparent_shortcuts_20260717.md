# Key-preferred transparent shortcuts

## Plan

- [x] Reproduce Obsidian level 2's key-skipping route and identify the exact transparent-wall dependency.
- [x] Define a general route preference that preserves valid shortcuts while preferring intended key progression.
- [x] Add a `can_be_bypassed` flag to each preferred key step that has a verified transparent-shot alternate.
- [x] Add integration coverage for Obsidian level 2 and representative corpus behavior.
- [x] Regenerate metadata and compare objective order, route status, and performance across the corpus.
- [x] Run D1/D2 builds, Android verification as needed, and scoped quality checks.

## Constraints

- Do not add mission-specific rules or identify Obsidian by name, level number, segment, trigger, or texture.
- Preserve transparent-wall shooting as a valid alternate route.
- Prefer a key only when it is reachable before the shortcut-dependent objective and would make the same objective conventionally reachable.
- Do not add additional visibility searches to the normal route calculation unless corpus evidence justifies the cost.

## Findings

- Obsidian level 2's permissive route used trigger 10 through a partially transparent, unlocked keyless door after collecting the blue key. The transparency-free route adds trigger 5 before trigger 10 and remains fully calculated.
- The normal planner now classifies only its selected firing pose with one strict confirmation ray. It does not add samples to the existing firing-pose search.
- When a completed route uses a transparent shot after collecting a key, the planner calculates one transparency-free route. It prefers that route only when the strict result is also `ok` and retains the preceding key.
- The preferred key step is serialized as `can_be_bypassed: true`. The level note explains that the key-gated branch can be skipped by shooting through a transparent wall.
- The metadata viewer renders the key objective with `(Can be bypassed)` while Guide-Bot follows the preferred strict route.
- Obsidian level 2 now routes through blue key, trigger 4, trigger 5, trigger 10, reactor, trigger 11, and exit.
- Full-corpus regeneration completed 109 valid archives with one no-descriptor skip and zero failures. The rule activated in 51 levels across 23 mission files, producing 87 annotated key steps.
- Final corpus generation took 187.5 seconds, compared with the previous 194.9-second baseline.
- D1 and D2 Windows builds, metadata scan tests, route snapshot tests, route cache tests, Android assembly, focused host coverage, and the updated Obsidian device test passed.
