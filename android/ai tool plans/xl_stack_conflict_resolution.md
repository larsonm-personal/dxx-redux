# XL stack conflict and private-assets repair

1. Refresh the exact open stack heads and trusted CI failure details; preserve the completed cleanup candidate and original root checkout
2. Use a separate clean worktree to check out GitHub stack #400, record original refs and rebase onto current cmake, resolving conflicts while preserving later fixes
3. Apply the already validated cleanup/private-test repairs to the rebased tip; reconcile any changes introduced by the rebase rather than overwrite them
4. Run affected linked combat/native checks, required public/controller checks and scoped quality; compare the final tree to the validated candidate
5. Publish the repaired stack with explicit leases after validation, check GitHub mergeability and exact-head CI, and record any remaining genuine failure

The expected readiness result remains 4/8. Do not weaken readiness, private asset verification, native assertions or required selections. Do not merge PRs. Preserve independent PRs and existing root branches/stashes. Local conflict resolution is complete. The user explicitly approved publication on 2026-11-09; all 58 branches were then published atomically with explicit leases.

## Result

- All 58 branches rebased detached onto cmake `49e92bbcdf92b31f77c705f65587d3dfe81f299c`; existing local branch refs and root checkout preserved
- PR #450 was the only substantive conflict. Retained actual XL activation for both nonphysical and force-damage controls; Git dropped its redundant follow-on activation commit
- The rebased aggregate before cleanup exactly matches the original tip combined with current cmake; no aggregate feature changes were lost
- Cleanup/private-test fixes committed at `268e711f008cbf5770b74ee365da4d821723fb57`; source tree exactly `d01c46fbe590d56cfb69ba63a1bbf7432e91a74f`, the fully validated candidate
- Trusted run `42684951549`, Linux job `129389769734`, confirms the remote private-assets failure is the five original native failures already repaired locally, rather than an asset provisioning failure
- Fresh rebased-source Linux private controllers: 45/45; artifact boundary: 6/6 on Windows. A preliminary Windows controller invocation lacked CMake on PATH; rerunning the complete controller suite in Linux passed
- Existing matching-tree validation: private native 113/113, public Windows 56/56, public Linux 61/61, Windows D1/D2 builds and suites, Android runtime 65/65, Pumo gameplay and cold persistence, scoped formatting/lint
- No readiness scoring changes. The expected four unsupported campaigns remain explicit
- Initial push was blocked pending explicit approval; after the user approved, all 58 branch updates published atomically. All 58 PRs now report mergeable=true. No PRs merged
- Published tip: `268e711f008cbf5770b74ee365da4d821723fb57`; source tree remains the fully validated candidate
- Hosted checks are queued. Exact-head request run: `42750955930` (the initial run was superseded by base-update events). It remains queued; no new hosted private result is available yet
- Separate PR title/description update initially required explicit approval. After the user approved, #461 was updated to "Fix XL collisions and simplify gameplay validation" with the approved cleanup, repairs, validation and limitations summary

Review worktree: `temp/xl-stack-conflicts-20261109`. Original per-PR heads are in `original-pr-inventory.json`; complete rebased mappings and replay log are in `temp/xl-stack-review-20261109/stack-rebase-progress.json` and `stack-rebase.log`. Published branch mappings are in `temp/xl-stack-review-20261109/published-stack.json`; the atomic push result is in `stack-push.log`. The verified local recovery bundle is `repaired-stack.bundle`. Existing local branch refs and all three root stashes remain intact. Do not rerun the one-shot publication script after success; its original leases intentionally reject a repeat.

Publication verification is recorded in `temp/xl-stack-review-20261109/published-stack-verification.json`. All 58 remote PR heads match the intended published mappings and report mergeable. The private-assets request is queued; local matching-tree validation is complete, but hosted success is not yet claimed.
