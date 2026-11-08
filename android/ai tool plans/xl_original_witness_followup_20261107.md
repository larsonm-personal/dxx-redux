# Original XL witness follow-up

## Evidence and scope

- The sanitized diagnostic run at `d5a9e61047e294dff8066393808de698b8742529` stopped the Android owner in the texture witness and the host owner in native tests
- The texture fixture copies its incoming texture into mission A but leaves the input in its higher-priority PhysFS root mount; a synthetic reproduction using pinned PhysFS 3.2.0 confirms that it shadows the intended global control
- The host receipt reached the outer 1800-second owner timeout; it does not establish which native test consumed time or how much time preceding builds used
- Preserve engine behavior, qualification gates, timeout budgets, process-group handling, private log exclusions, and trusted receipt filtering

## Plan

1. Consume only the isolated texture fixture's input copy after staging it into mission A, and exercise the same staging path in the public synthetic fixture
2. Assert that global lookup resolves to the explicit global control before renderer tests
3. Preserve bounded host stage-entry timing and allowlisted last-observed native CTest progress on interruption, without exporting raw output
4. Add synthetic privacy/interruption regression coverage, run relevant Python suites and scoped code quality, and obtain independent review
5. Publish a draft development PR and check exact-head CI; do not merge

## Validation boundaries

- Synthetic PhysFS reproduction establishes the fixture lookup collision, not the exact private assertion or a successful original-asset GPU rerun
- Public Android fixture CI must exercise both games; trusted private qualification remains separate
- New host progress is diagnostic only and must not supply coverage, passing tests, or readiness evidence
