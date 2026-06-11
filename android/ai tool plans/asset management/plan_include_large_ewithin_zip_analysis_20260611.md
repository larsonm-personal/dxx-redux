# Include large ewithin ZIP analysis plan

## Goal
Stop `ewithin-versions.zip` from being skipped by the mission ZIP batch analysis size cap while preserving a default safety cap for other unexpectedly large ZIPs.

## Work phases
1. [x] Inspect the current batch skip logic and generated `ewithin-versions.json`.
2. [x] Add a configurable or allowlisted path for large ZIP analysis.
3. [x] Update failure/regression JSON behavior if needed so skipped-large sentinels are not written as analysis output.
4. [x] Run scoped formatting and a script-level validation.
