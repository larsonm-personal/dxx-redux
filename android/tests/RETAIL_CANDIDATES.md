# Known-account candidate qualification

The manual qualification path is available on cmake. The automatic path below remains proposed until separately authorized integration

The existing cmake workflow and retail-ci branch restriction remain mandatory. Michael can dispatch that workflow with campaign xl-fast or xl-originals, candidate_pr, and the full candidate_sha copied from his own same-repository PR. Empty candidate inputs preserve existing behavior. Other campaigns are deliberately outside this initial exception

The authenticated GitHub profile identifies larsonm-personal as numeric account ID 265118235. The trusted controller checks that ID for the original dispatcher, current rerun actor, and PR author. It checks run identity, attempt, controller branch, repository ID 1171374860, open PR, same-repository head, and both live PR and branch SHA. Checks run during selection, planning, and again immediately before private checkout, including partial job reruns. API errors, missing identity fields, head changes and other rerun actors reject access

Supplying the exact SHA is an explicit endorsement of all code at that SHA, including commits by others. This does not claim Git author names or emails identify the authenticated pusher. A PR author alone grants nothing. A later head needs a fresh explicit dispatch; a race after validation still executes only the endorsed immutable SHA

Candidate catalog and setup scripts do not execute before private checkout. Catalog, policy, setup, staging and receipt validation come from the trusted controller. Candidate execution starts after staging and token-scoped checkout, with persist-credentials disabled. The existing immutable asset pin and sanitized public receipt boundary remain in place. The read-only asset token is not passed to candidate commands

This deliberately trusts the endorsed candidate code with the staged assets: it can read and potentially disclose them over the network. It is not a sandbox for malicious code, nor does sanitizing receipts prevent intentional exfiltration. The account must endorse the complete candidate, including build tools and dependencies. A compromised trusted account has the same authority

Manual qualification uses an explicitly authorized dispatch of the trusted cmake workflow. No environment/security settings changes or new credentials are required. Do not dispatch an unintegrated controller branch to evade the existing gate. Trusted controller receipt contracts must also support the selected candidate's test output; this policy does not import candidate validation rules or establish a successful XL smoke

## Proposed automatic XL path

After separately authorized integration into cmake, `xl-asset-request.yml` emits a cheap public signal on every PR open/update/reopen/ready event. It checks out no code and produces no trusted artifact. A separate `xl-pr-assets.yml` workflow runs from the default branch on completion and publishes the distinct `XL asset smoke (private assets)` commit status on the exact admitted head

The controller uses authenticated GitHub API data for both runs, original and rerun actors, same-repository provenance, and the open PR author. All actors and the author must be account ID 265118235. A successful source run must come from the fixed request workflow path and exact branch head. Since GitHub can omit the run's PR association, the controller resolves the open PR through the commit-to-PR API, rejects missing/ambiguous associations, and rechecks the live PR and branch SHA. Other authors, cross-repository forks, other actors, failed sources, moved heads, stale events and missing provenance fail closed

This automatically endorses the complete head associated with Michael's authenticated PR event, including contributions by others already present when he opens/reopens the PR. A later update triggered by another actor is excluded even if Michael originally authored the PR. Commit names/emails are never treated as authentication. The user accepts that eligible code can read and potentially disclose staged paid game assets

The existing xl-fast owner is reused. The candidate checkout stays pinned; selection, planning and the pre-checkout gate revalidate provenance. Assets remain pinned and hash-verified. Candidate setup does not execute before staging; the asset token is supplied only to private checkout, never candidate commands. Reporting uses a fresh runner with status-write permission and no candidate execution. A green status requires the same exact target's positive navigation count and required XL counts; skips, timeouts, parser-only success and missing receipts fail

GitHub reusable workflows cannot elevate token permissions, so the reusable call allows the existing ledger publisher's contents-write ceiling. The actual plan/host jobs explicitly use contents-read, and the publisher is explicitly disabled for workflow_run. No candidate execution job receives a write token. This preserves one executor instead of duplicating asset handling

No new GitHub settings or credentials are intended. Activation requires explicit approval and integration into cmake; this branch does not activate the automatic grant. GitHub's announced default block of pull_request_target makes workflow_run the preferred trigger here. Public PR CI can validate the controller and request job before integration, but a real automatic follow-up requires the reviewed controller to exist on the default branch

Repository identity, not the repository-level `fork` boolean, defines a same-repository PR. dxx-redux itself is a fork of its upstream, so its own branch PRs legitimately have `head.repo.fork == true`; a foreign fork still has a different numeric repository ID and is rejected

XL failure receipts include `xl_stage`, the last entered stage (for example `preview-build` or `navigation-smoke`). This is a bounded diagnostic, not proof that the stage completed or that gameplay ran. The controller prints that fixed stage and owner status to the job log; raw private logs remain excluded. Unknown, missing or invalid progress cannot produce a positive receipt

The XL controller must explicitly forward the named `RETAIL_ASSETS_READ_TOKEN` secret to the reusable sweep. GitHub requires this even when the value exists only in the callee's `retail-ci` environment; selecting that environment is not sufficient. The environment's value takes precedence over a same-named repository secret. The reusable contract requires the name, and a post-admission boolean check diagnoses an unavailable value without exposing it. Only private checkout receives the value; other secrets are not inherited. See [GitHub reusable-workflow secret semantics](https://docs.github.com/en/actions/how-tos/reuse-automations/reuse-workflows#using-inputs-and-secrets-in-a-reusable-workflow)
