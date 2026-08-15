# Automap Show Secrets Label

## Goal

Use the fixed label "Show Secrets" for the automap secrets checkbox regardless of its checked state.

## Plan

- [x] Locate the state-dependent automap secrets label and confirm its scope.
- [x] Replace the state-dependent wording with the fixed label.
- [x] Run focused formatting and Android unit-test/build verification.

## Boundaries

- Preserve the checkbox state, green active highlight, and toggle behavior.
- Do not change secret reveal behavior or other automap settings.

## Result

- The automap secrets checkbox now always uses the label "Show Secrets".
- Checkbox state, active highlighting, and toggle behavior are unchanged.
- Scoped code quality checks and all 25 `AdminTrayUiTest` tests pass.
