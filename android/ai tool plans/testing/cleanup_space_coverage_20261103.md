# Cleanup coverage and 50 GiB recovery, November 3 2026

The compatibility goal remains paused. This is the user's separate requested
cleanup-helper enhancement. Preserve all bundles, original game data, recovery
archives, worktree source/changes and three stashes. No device tests or assets
are needed. Use only workspace-contained cleanup and never follow links.

## Evidence

Native allocated-space audit (temp/cleanup-audit-20261103/workspace-space.json)
finds server outputs 20.55 GiB, old registered xl-batch2 worktree 18.74 GiB,
Android app outputs 10.37 GiB allocated/13.60 GiB logical. Original game_data 48.94 GiB,
Git object storage 11.67 GiB and published build-outputs 5.47 GiB are preserved.
The old worktree contains original/private fixtures as well as generated output;
its full size is not a deletion estimate.

## Changes and gates

1. Extend Cargo incremental discovery to nested custom-target and target-triple
   output layouts, preserving binaries, deps, exclusions, locks and nested repos
2. Opt in to generated-build cleanup in registered in-workspace Git worktrees.
   Validate common Git identity, use each owning worktree's Git protection list
   and recheck it before deletion. Outside/unregistered repositories stay intact
3. With explicit zero build retention, descend a source-declared Gradle/Cargo
   output around excluded or Git-protected children, so preserving APK outputs
   does not strand all unrelated build intermediates
4. Add synthetic integration coverage for real linked worktrees, owning-index
   protection, nested caches, exclusions, links/locks and active-process guards.
   Run scoped quality and existing cleanup tests/catalogs
5. Preview actual candidates with explicit preservation exclusions, then apply
   the existing helper toward 50 GiB free. Record physical before/after bytes,
   deleted paths and preserved work. Do not remove unknown fixtures to hit a quota

## Verified result

The enhanced helper's preview identified 52.38 GiB logical generated outputs. The
actual run used BuildsOnly/IncludeWorktreeBuilds/KeepBuildGenerations0, explicit
source/bundle/recovery/output exclusions and a 50 GiB free-space target. It stopped
after 23 artifacts, 51.14 GiB logical, in 145.4 seconds. Actual volume free space rose
from 5971677184 to 54009774080 bytes: 48038096896 bytes reclaimed (44.74 GiB), with
50.30 GiB (54.01 GB) free. Logs and machine-readable receipts are in
`temp/cleanup-audit-20261103`.

Before deletion, 525 native profile/settings files from old generated test trees
were locally archived and read-back/hash verified (850769 bytes; not published).
After deletion all 15 worktrees' staged/unstaged diffs and status matched, all refs
and three stashes matched, retained current APK/test APK and exact GuideBot EXE
hashes matched, and 104 files in 4 checksum-bearing bundles matched. Published
bundles, current recovery work, original game_data, Git metadata and AVD userdata
were excluded. The compatibility goal remains explicitly paused.
