# Unused level key metadata

## Plan

- [x] Trace key presence, preferred-route key use, metadata serialization, and viewer header rendering.
- [x] Define a general per-level representation for keys present but absent from the preferred end-of-level route.
- [x] Calculate and serialize unnecessary keys without conflating them with bypassable preferred keys.
- [x] Render a concise metadata header note such as `gold key not necessary`.
- [x] Add focused tests for an unnecessary key and for Obsidian level 2's distinct bypassable-key case.
- [x] Regenerate affected mission metadata and audit the corpus.
- [x] Run D1/D2 builds and tests, Android verification, and scoped code quality.

## Constraints

- A key is unnecessary only when it exists in the level and is absent from the preferred end-of-level route.
- A preferred-route key marked `can_be_bypassed` is not unnecessary.
- Do not add mission, level, segment, trigger, or texture-specific rules.

## Findings

- The planner records keys physically present as direct powerups or robot-carried powerups and subtracts keys used by the selected end-of-level route.
- Notes are emitted only for complete `ok` routes through the existing level metadata `notes` header.
- Counterstrike level 14 reports `blue key not necessary` because blue exists but is absent from the preferred route.
- Counterstrike level 20 independently reports its gold key as unnecessary while retaining its preferred blue key as `can_be_bypassed`.
- Obsidian level 2 retains its preferred bypassable blue key and does not label it unnecessary.
- Full regeneration passed 109 archives, skipped one archive without a descriptor, and failed zero archives.
- The corpus contains 99 unnecessary-key notes across 70 levels in 36 mission files: 32 blue, 31 red, and 36 gold.
- The corpus audit found zero route changes and zero keys that were both preferred-route or bypassable keys and labeled unnecessary.
- The calculation is one linear pass over the existing snapshot objects and selected route steps and adds no path searches.
- D1 and D2 metadata builds, metadata scan tests, route cache tests, route snapshot tests, focused Counterstrike and Obsidian tests, scoped code quality, and Android `assembleDebug` passed.
