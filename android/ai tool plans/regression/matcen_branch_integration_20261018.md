# Matcen branch integration

- Merge compact metadata PR #90 into lifecycle validator PR #91 with a normal merge commit
- Reuse compact_capture at the lifecycle comparison operands; retain raw native evidence and historical provenance
- Test compact metadata acceptance, mismatch rejection and unchanged behavior vectors with existing source fixtures
- Run scoped quality checks and existing public validation, then push only the PR #91 branch and monitor public CI
- Leave both PRs targeted at cmake and unmerged; do not run private corpus campaigns
