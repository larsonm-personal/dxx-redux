# Feature-based XL admission

Replace mission identity/hash admission with detected requirements and explicit
runtime capabilities. The original Counterstrike level is a regression fixture,
not an exception in production policy. Unrecognized versions/features and missing
asset dependencies must reject with a useful reason

1. Separate format inspection, translation eligibility, runtime capability and
   gameplay qualification. Inventory warnings must not silently disappear
2. Detect XL bytes at level load and apply the same policy under arbitrary mission
   and level names. Validate source object/resource references before the stock
   loader can substitute or clamp them
3. Report actionable rejection reasons in the game UI. Remove the production
   level-one completion shortcut; preflight progression through the same policy
4. Keep optional diagnostic fixtures explicit. Do not make development probes
   or visual evidence imply general campaign qualification
5. Verify renamed/rehashed supported content, unsupported feature mutations,
   resource failures, stock isolation, ordinary/XL builds, original pilot gameplay
   and Android selection/save/restore/rejection behavior

The current parser supports level 15/game data 33. Other dialects stay rejected
until their format and semantics are implemented. Admission is per loaded level;
accepting level 1 does not certify the rest of a campaign

Implemented 2026-10-24:
- Shared runtime blockers cover serialized unknown/unsupported requirements,
  versions and engine capacities. Identity/hash/level-one admission removed
- Preflight checks source object IDs, dependency graphs, palette/PIG availability,
  descriptor options and unsupported custom HAM/HXM definitions before translation
- New-game, normal progression and secret-entry UI report concrete rejection
  reasons. Loader fallback retains diagnostics for other entry paths
- The production level-one stop is removed. Only explicit fixture navigation
  tests retain that completion shortcut
- Browser distinguishes feature eligibility from runtime/gameplay verification;
  metadata analysis remains unavailable for XL sources

Validation:
- Windows XL runtime and XL-disabled build compile
- All 37 host CTests pass, including gameplay/escape/routes and original archive
- Final inventory and lifecycle rerun passes after capacity/reactor guards
- Lifecycle accepts different mission name, filename, level number and source hash;
  rejects unsupported version and missing robot definitions with specific reasons,
  preserves active state during preflight, and recovers to supported/stock content
- Android XL build passes; emulator-5554 runtime regression passes 36/36 steps
  (run feb9fce723e443659249269d7b5ded11, 20471 ms). This run precedes only the
  final reactor-ID guard, covered by the final host lifecycle rerun
- Scoped formatting/lint passes

Limits: Counterstrike's original first level remains the gameplay-qualified pilot.
Per-level admission does not certify the other 29 levels or arbitrary campaigns.
Multiplayer, custom gameplay overrides, unknown descriptor options and unaudited
boss spawning reject conservatively. Rejection dialogs are wired but have not yet
had a separate on-device negative-path automation run. Save/restore and ordinary
selection were exercised by the Android regression; other loader callers retain
an explicit fatal-error fallback rather than the new-game recovery dialog
