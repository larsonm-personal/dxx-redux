# D2X-XL PR branch preparation

## Scope

Rebuild `d2xxl_compat` on the fetched `origin/online-restoration` so its PR
contains only XL compatibility work. Preserve the current pilot implementation
and uncommitted changes. Do not merge the website branch or qualify the pilot
as complete. The intended PR base is `online-restoration`, not `cmake`

## Recovery and history

- Original published tip: `e7f9b2c47`
- Old website boundary excluded from the replay: `17ba9ef2a`
- New base: `2d208d7c77a5c161da01e65e27804d1de0fb7b16`
- Original history retained at `backup/d2xxl-compat-before-rework-20261023`
- Uncommitted pilot work captured before replay at
  `backup/d2xxl-compat-with-pilot-work-20261023`
- Replayed only the initial XL commit and the pilot implementation checkpoint
- `git range-diff` reports both replayed patches unchanged

The local post-commit hook tries to amend commit timestamps during replay and
prints an amendment error while cherry-picking. The rebase itself completed;
range-diff and clean status confirm that neither patch was lost

## CI preparation and validation

Track the preserved research ZIP explicitly because the repository ignores ZIPs
by default. It contains reports, decoded measurements and scripts, not mission
binaries. Its absence previously made the documented audit fail on clean clones

Add Linux/Windows asset-free XL CI for the standalone inventory and lighting
contracts plus survey manifest/measurement checks. Existing engine smoke CI
continues to build the ordinary D1/D2 targets. Private pilot gameplay and pinned
reference-source tests remain separate; green public CI is not pilot acceptance

Before publishing, run scoped formatting, standalone configure/build/CTest,
automation catalog checks, survey audit and diff whitespace checks. Verify that
the branch is directly descended from the intended base, website paths are
absent from the PR diff, and only intended work remains. Publish the rewritten
branch with an explicit expected-old-tip force-with-lease; leave PR creation to
the user

Validation completed: fresh Visual Studio x64 Debug configure/build and both
asset-free CTests pass, survey audit passes, scoped formatting/lint passes,
and both automation catalog checks pass (370 top-level entries, 431 support
scripts). Whitespace checks pass. The merge base is exactly the new base above;
the landing tree matches that base and no old website commits were replayed.
Linux and hosted workflow results remain pending CI
