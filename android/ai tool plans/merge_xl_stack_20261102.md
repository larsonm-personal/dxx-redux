# Merge open XL stacks into cmake

User explicitly authorizes merging all open personal-fork PRs and proceeding without
waiting for in-progress CI. Preserve uncommitted metadata-browser work and user stash.

GitHub exposes two native stacks: #295-#304 (after already merged #294), then
#305-#315. Repository settings allow squash merges only. The synchronous connector
rejected native stacks without mutation; use GitHub's documented merge-async endpoint
at each stack tip, which includes its open downstack PRs.

1. Snapshot open PR refs and cmake; retain the original first/second stack trees.
2. Merge through #304 with expected SHA, then verify cmake tree equality. Passed:
   cmake 7ed644849988cabccc60799b002502d4678c06c9 exactly equals #304's tree.
3. GitHub automatically moved the second stack's base to cmake. Merge through #315
   with expected SHA 8d228564aab9cb99903328965b37825a5e13eded.
4. Verify every requested PR is merged, no open PR remains, and final cmake exactly
   equals the original complete stack-tip tree. Preserve local edits and stash.

Receipts: temp/pr-stack-merge-20261102. No upstream writes or branch deletions.

## Completed

All 21 requested PRs (#295-#315) are confirmed merged; no open PRs remain.
Final cmake: b4e7c53b1977ae1fc689b4d72a4ffa5d5f12dcfd.
Its tree exactly equals original complete stack tip
8d228564aab9cb99903328965b37825a5e13eded (no file/content differences).

The second stack initially conflicted because its base history predated the first
stack's rebase/squash. A content-preserving parent merge fixed the base conflict,
but native stack merging required a linear chain. Rebuilt #305-#315 as one commit
per PR, retaining each exact original file tree, and pushed atomically with explicit
old-head leases. GitHub then merged the full stack successfully.

Local tracked diff is byte-for-byte identical before/after; HEAD and preexisting
stash remain unchanged. New local metadata-browser work is still uncommitted and
therefore is not part of these PR merges. No upstream writes or branch deletion.
CI was not awaited, per user instruction. No new source changes or tests were needed
for the merges: every reconstructed PR tree and the final combined tree were checked.
Detailed per-PR merge receipts, old/new heads and trees are in the evidence directory.
