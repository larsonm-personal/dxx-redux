# Recovering an interrupted coding task

Recover existing work before provisioning another executor or repeating a remote
operation. Keep checkpoints small enough to read and restore quickly.

## 1. Check status and try resuming

1. Check the task UI and latest completed turn. A completed assistant turn is not
   evidence of a disconnected executor: read the result and check whether the
   task finished or is waiting for input.
2. If the executor disconnected, try resuming the existing task first. Files,
   credentials, build trees and jobs may survive. Verify what is still present.
3. Provision a replacement only if resuming is unavailable or unsuccessful.
   Avoid duplicate builds or competing edits while the original task is active.
4. Read `AGENTS.md`, `.github/copilot-instructions.md` and relevant
   `.agents/skills` if present. Locate the last checkpoint before changing files.

## 2. Capture a checkpoint

Record the task reference, working directory, branch, full HEAD SHA and base SHA.
Run these read-only commands in the checkout; review output before sharing it:

```sh
git branch --show-current
git rev-parse HEAD
git status --short
git diff --stat
git diff
git diff --cached
git ls-files --others --exclude-standard
```

Keep a short record of:

- Changed paths and purpose, including staged, unstaged and untracked work.
- Exact build/test commands, relevant tool versions and build directories.
- Completed results and exit codes; label running or unexecuted checks clearly.
- PR number, Actions run ID, attempt number, job IDs and artifact IDs when known.
- The next action and any access failure, with a short sanitized error excerpt.

Checkpoint after useful edits, before long builds, after tests and after remote
writes. Preserve concise results durably, not transcripts or bulk log archives.

## 3. Preserve work before replacing the executor

Prefer a small reviewed commit pushed to an authorized topic branch, never
`cmake`. Stage explicit paths, review `git diff --cached`, and verify the remote
SHA. Do not force-push, rewrite history or merge as a recovery shortcut.
If publishing is not authorized or available, make a patch backup:

```sh
# Replace these example paths with an explicit reviewed allowlist.
git diff --binary HEAD -- path/to/changed-file > /durable/backup/work.patch
```

This captures net tracked changes against HEAD, not their staging state. Save
the base SHA alongside it. Copy reviewed untracked source files separately;
`git diff` does not include them. A local stash or `/tmp` file alone does not
protect against executor loss. Use an approved durable location and verify
retrieval before discarding the original environment.

Inspect the patch and copied files. Exclude credentials, tokens, environment
dumps, private configuration, retail game assets, build outputs and bulk logs.
Restore into a clean checkout of the recorded base, run `git apply --check`
first, then apply and review. Do not overwrite another active task's work.

## 4. Verify the base and remote state before retrying

The default checkout or saved configuration ref can be stale. Fetch and verify
the intended target instead of assuming it is the latest `cmake`:

```sh
git fetch origin cmake
git rev-parse origin/cmake
git log -1 --oneline origin/cmake
# For new work only, choose an unused branch name and directory.
git worktree add -b recovery-topic ../recovery-topic origin/cmake
```

For resumed work, recover its branch and changes before updating its base.
Keep separate work in separate worktrees; do not reset an active checkout.
Before retrying a push, compare HEAD with the remote topic branch using
`git ls-remote --heads origin <topic-branch>`. Before creating a PR, look for an
existing PR with that head/base. Before rerunning Actions, inspect the latest
run, attempt and job status. A lost response does not mean the write failed.
Reuse an existing PR or running attempt when appropriate; avoid duplicate runs.

## 5. Check existing GitHub authentication first

Start with `gh auth status` and a harmless authenticated read such as
`gh api user --jq .login`. Do not print tokens, credential files or environment
values. Check existing CLI configuration before initiating any login.
An injected `GH_TOKEN` can return HTTP 401 while a saved CLI login still works.
If that override is invalid, test the saved login without it for one command:

```sh
env -u GH_TOKEN gh auth status
env -u GH_TOKEN gh api user --jq .login
```

Only after verification, omit that invalid override per command that needs the
saved login. Do not erase other overrides or change global credentials blindly.
If CLI auth is absent, preserve the tested local diff and use an already working,
authorized GitHub connector to publish, or request the normal user handoff.
New credential persistence requires the normal user approval/setup flow. Never
copy tokens between environments or store credentials in the repository.

Global Git configuration can be read-only. Where permitted, use command-scoped
`git -c key=value ...` or repository-local configuration instead of retrying
writes to the global file. Keep secrets out of configuration arguments.

## 6. Separate task admission failures from GitHub access

On 2026-10-19, attempts to resume or create coding tasks failed before execution
because automatic review reported capacity/timeouts. GitHub connector reads and
authentication still worked. Repeated backoff did not restore task admission;
this was not evidence of a GitHub credential failure or a failed build.

Use a bounded retry for a transient capacity/timeout error, then identify the
failing layer and check another permitted route. Confirm whether an uncertain
request created a task or changed remote state before repeating it. Never use an
alternate route to bypass a denied review, permission check or security control.
Working read access does not authorize writes.

The successful alternative used the existing authorized GitHub connector and an
isolated local scratch directory:

1. Read the exact source commit and tree. Fetch source files and required
   dependencies through the connector at pinned revisions, preserving paths.
   Keep scratch work separate from active checkouts; do not copy credentials.
2. Implement and test the change locally. Record commands, exit codes and the
   coverage actually exercised. Unit tests do not establish full-corpus replay
   or Android integration coverage.
3. Before publication, verify authorization for the exact repository and topic
   branch. For new work, branch from the verified current base. For resumed
   work, preserve the original topic head as the parent.
4. Use `create_tree` with `base_tree_sha` from that parent and inline content
   for only the reviewed changed files, preserving file modes. Use
   `create_commit` with the original head as its parent; inspect the resulting
   tree/diff so unrelated files are not dropped or changed.
5. Fetch the raw Git commit and verify both author and committer name/email
   against the authorized identity, plus the parent, tree and message. Check
   that no unintended co-author trailers were added. A profile or PR display
   name is not a substitute for this check.
6. Advance only the authorized topic ref with `update_ref`, `force=false`
   and `expected_sha` equal to its original head. If the head changed, stop
   and reconcile instead of overwriting it. Fetch the ref afterward and verify
   it equals the new commit SHA, even if the write response was lost.
7. Reuse or open the authorized PR, then inspect checks and Actions runs for
   that exact SHA through terminal status. A stale PR cache is not evidence
   that a write succeeded or that the new commit passed CI.

This route needed no new login, credential transfer or security-setting change.
It did not require pushing to `cmake` or merging; those remain separate actions
requiring explicit authorization.

## 7. Inspect CI evidence before diagnosing or rerunning

Prefer the newly authorized GitHub connector for Actions logs and artifacts.
In the 2026-10-18 PC setup, it retrieved PR #93 rerun job `117739476608` logs and
artifact `12144819295`. The evidence showed native building consumed the
600-second test budget. Inspect artifact contents and timing before guessing
that tests hung or repeatedly spending another run.

CLI/API metadata success does not prove log or artifact blob downloads work.
Locate the failing stage and retain only a short sanitized error excerpt:

- Redirect failure: metadata works, but the download destination is inaccessible.
- HTTP authentication failure: check the credential used by that request.
- Proxy CONNECT failure: the tunnel failed; this alone does not prove bad auth.

For a completed run, inspect an authorized whole-run log archive when individual
job/blob downloads fail at transport. Reusable-workflow jobs can have nested log
entries that an older CLI's `--log-failed` output omits; an aggregate failure alone
is not the underlying test diagnosis. Reuse an already downloaded archive before
requesting another download. Never retain signed download URLs in checkpoints.

Do not bypass network restrictions or repeatedly edit allowlists without evidence
that a permitted change addresses the failing destination. Use the working
connector or the normal user access handoff when necessary.

The Android connector screen requested MFA despite user-reported Advanced Account
Security; the PC flow succeeded. This observation does not establish a root cause
or justify disabling security. Try the working approved flow and preserve work
if access remains blocked. Filing a support report is not a default recovery step.

Finish with the branch/SHA, restored paths, verified test results and next action.
Keep account identifiers, transient URLs, device codes, internal assistant notes
and raw transcripts out of repository documentation. See
[CI coverage and local reproduction](CI.md) for the test entry points.
