# Trusted native public tool cache

## Goal

Reduce repeated release-download provisioning failures without changing dependency pins,
verification, private-asset admission or gameplay qualification

The trusted `cmake` controller at `258d9a1ee6e81f5830b35598b37844ab44fea6b6`
runs the host job in `nightly-tooling-sweep.yml` through `xl-pr-assets.yml`.
Run 42566251628 failed before candidate compilation on bounded Python HTTP 504,
then on 7-Zip HTTP 504 after one failed-job retry. Runs 42567620407 and
42568639096 also failed release downloads before candidate compilation.
A successful setup, such as run 42567644091, is still separate from passing owners

## Change

1. Restore only the public bounded-Python and 7-Zip installation directories using
   the existing SHA-pinned official Actions cache restore/save actions
2. Restrict both operations to the owned repository's `cmake` schedule/manual
   controller or `xl-pr-assets` workflow-run controller. PR and other branch runs
   neither restore nor save this cache. Enable it only for the two existing XL
   host owners that already require both tools
3. Use an exact key containing runner OS, architecture and the trusted controller's
   `tool_versions.conf` hash. Never use fallback keys or target-controlled versions
4. Run the trusted existing installers even on a cache hit. Python checks its full
   pinned tree and runtime version; 7-Zip checks its pinned executable hash
5. Save explicitly immediately after successful verification, before any target
   setup script, private checkout or candidate executable. Never save post-job or
   cache the dependency root, private inputs, fixture data or candidate outputs
6. Keep cache service failures optional; pinned installation/verification remains
   mandatory. A cold cache still needs one successful upstream download and setup

## Verification and limits

Extend the existing planner/workflow contract tests; run scoped quality checks,
actionlint, relevant Python tests and the existing automation catalog checks.
No new test runner, toolchain, workflow trigger or infrastructure service is added.
No timeout or qualification policy changes. Hosted cache reuse is not proven until
this controller change is separately reviewed and merged; this PR does not authorize
that merge or trigger extra native qualification retries

Local validation: final planner/workflow suite 110/110 and retail-boundary suite
19/19 pass. Scoped pinned Ruff/Prettier, actionlint and whitespace checks pass.
The automation catalog validates 99 standalone JSON tests, 546 support scripts and
315 standalone PowerShell tests. The aggregate catalog runner remains blocked by
its existing 4 GiB output reserve with 3.60 GiB available; no reserve was bypassed.
Independent security/correctness review found no blocking issue.
