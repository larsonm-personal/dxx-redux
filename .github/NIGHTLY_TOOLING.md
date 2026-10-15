# Nightly tooling

The scheduled/manual workflow runs reviewed existing tests from one pinned `cmake`
revision. `android/run_all_tests.ps1 -ListTests` discovers master owners;
`android/tests/nightly_tooling_policy.json` selects supported owners and estimates,
and `host_test_suites.json` defines fixed host adapters.

Four lanes (host, JVM, Android single, Android dual) each have an estimated
45-minute test budget; setup/build time is separate. A single per-revision ledger
on `ci/nightly-tooling-state` records completed attempts. Interrupted or transient
setup remains pending. Failures stay visible. New revisions require fresh passes.
A passing owner does not certify skipped or unreported child cases.

PR validation is read-only and uses shareware/generated fixtures. It requires a
PASS receipt from every selected representative owner. Scheduled/manual execution
on trusted `cmake` can publish validated progress; PRs cannot. Detailed results and
logs are workflow artifacts, not committed inventory reports.

For host adapter arguments and prerequisites, see
[`HOST_TEST_SUITES.md`](../android/tests/HOST_TEST_SUITES.md).

## Private retail fixtures

The `retail-ci` Environment must allow only the `cmake` branch. Its
`RETAIL_ASSETS_READ_TOKEN` secret is read-only Contents access to
`larsonm-personal/dxx-redux-test-assets`; the repository variable
`RETAIL_ASSETS_COMMIT` pins an immutable forty-character commit.

Only the exact scheduled/manual `cmake` coordinator can select that Environment.
`retail_asset_sources.json` maps supplied paths to isolated staging;
`standard_game_data.ps1` defines required file hashes. Missing configuration or
mismatched files remain pending. PRs never receive these fixtures. Retail lanes
upload only bounded result/verification metadata, exclude broad logs/screenshots,
and do not save caches. Token functionality and retail tests require a trusted
post-merge run; source admission alone is not runtime qualification.
