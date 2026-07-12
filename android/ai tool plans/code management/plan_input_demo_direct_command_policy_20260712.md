# Input-demo direct-command policy and fixture refresh

## Goal

Minimize the inherited D1/D2 direct-command replay diff by moving common event
iteration, validation, failure handling, and shared commands behind one explicit
boundary. Keep game-specific command behavior in compact adapters, then refresh
affected input-demo fixtures once from the fully validated implementation.

Input-demo compatibility may break in this tranche. Do not add compatibility
branches solely to preserve obsolete regression recordings.

## Existing work to preserve

- Preserve current deterministic simulation and final-state comparison behavior
- Preserve engine-owned command effects rather than simulating them in the demo layer
- Preserve D1/D2 policy differences only where they are intentional and tested
- Preserve unrelated route-planner, Guide-Bot, mission metadata, and secret-area work
- Keep Windows D1/D2 and all configured Android ABIs supported

## Phases

### 1. Live audit and boundary design

- [ ] Read the prior input-demo cleanup, parity, command, and diff-minimization docs
- [ ] Inventory current D1/D2 recording, parsing, iteration, and application paths
- [ ] Inventory direct-command names and classify common versus game-specific policy
- [ ] Record target-file diff metrics against `main`
- [ ] Define a shared API that is smaller than the removed inherited bodies

### 2. Focused fixtures before movement

- [ ] Cover ordered iteration of zero, one, and multiple commands in one frame
- [ ] Cover malformed and unknown command failure without partial later application
- [ ] Cover common death-abort and live-difficulty commands in D1 and D2
- [ ] Cover D2-only Guide-Bot, marker, weapon-drop, and flag policy through an adapter
- [ ] Capture intentional D1/D2 failure and replay-unload behavior

### 3. Extraction and cleanup

- [ ] Move common iteration, validation, dispatch result, and diagnostics to shared code
- [ ] Add compact explicit D1 and D2 policy adapters
- [ ] Remove superseded duplicated command loops and wrappers
- [ ] Keep command effects in canonical engine functions
- [ ] Update the schema/version deliberately if the clean boundary requires it

### 4. Deterministic validation

- [ ] Run focused direct-command writer/parser/dispatcher fixtures
- [ ] Record and replay representative D1 and D2 command sequences
- [ ] Verify exact final state, command order, result, state trace, and RNG trace
- [ ] Build Windows D1 and D2
- [ ] Build all configured Android ABIs
- [ ] Run scoped code quality and `git diff --check`

### 5. One-time demo refresh

- [ ] Identify every committed demo affected by the new command policy or format
- [ ] Re-record affected D1 and D2 regression demos from the final clean build
- [ ] Regenerate associated normalized state and RNG traces
- [ ] Run the complete D1 and D2 input-demo regression matrices
- [ ] Confirm no old compatibility shim remains solely for replaced fixtures

### 6. Campaign closeout

- [ ] Record exact inherited-file reduction and remaining private adapter sizes
- [ ] Update the high-coupling campaign and candidate catalog
- [ ] Run the final combined build and focused regression matrix for H01-H06

## Validation evidence

- Pending
