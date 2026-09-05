# Workspace cleanup

Run from the repository root:

```powershell
.\android\clean-workspace.ps1 -Preview
.\android\clean-workspace.ps1
```

The normal interactive run automatically removes ignored loose `.log`, `.tmp`,
and `.temp` files at least 7 days old in known scratch directories. It also
automatically removes superseded build generations without confirmation:

- Each Android module's `.cxx/<configuration>/<build-id>` is ranked separately
  for Debug, Release, and other configurations
- CMake build trees in recognized build/temp locations are grouped by source
  project, configuration, architecture, generator/toolchain, and feature flags
- Versioned APK/AAB/ZIP outputs are grouped by parent folder, package name,
  and extension, including the unique suffixes used by deployment builds

The newest generation in each family is always kept, including when it is old.
Ordering uses the newest write anywhere in each generation, not hash-name order
or the parent directory's timestamp. Ties and builds touched within the last
24 hours are kept. `-KeepBuildGenerations 2` retains two generations;
`-BuildGraceHours` adjusts the 24-hour protection window. These automatic build
rules are independent of `-ArtifactDays` and apply with `-AutoOnly` as well.
The retained replacement must still exist when an older build is deleted.
Discovery looks up to four levels below known output roots; source trees and
unidentified build layouts are not guessed to be interchangeable generations.

Other eligible artifacts require confirmation. Enter keeps them,
`noToAll` skips the remaining review items, and `quit` stops the run.
Choose `folder` to list the remaining review candidates of the same category in
that parent folder, then confirm that exact group with `yes`. Only the listed
old candidates are approved; the parent folder and recent siblings are kept.
There is no global yes-to-all option. Automatic deletion may already have
happened before `quit`.

Review items must be at least 30 days old, including their newest descendant.
These include repository build directories, Android native and Gradle outputs,
Rust target outputs, local download caches, packages in `android/build-outputs`,
and scratch/regression output directories. Scratch collections with recent runs
are inspected up to two further levels to find older independent artifacts.
Sizes shown are logical file sizes; compression and hard links affect disk savings.

```powershell
# Old loose temporary files and superseded builds, without interactive questions
.\android\clean-workspace.ps1 -AutoOnly

# Preview while keeping the newest two builds per family
.\android\clean-workspace.ps1 -Preview -KeepBuildGenerations 2

# Restrict cleanup to superseded builds and versioned build packages
.\android\clean-workspace.ps1 -BuildsOnly

# Preview a different retention window
.\android\clean-workspace.ps1 -Preview -TempDays 14 -ArtifactDays 60
```

`-Preview` and `-WhatIf` never delete or prompt. Preview a shorter retention window
before applying it. Recent files remain protected even inside old directories.
Git-tracked files and untracked non-ignored files protect their containing trees.
Trees with links, unrelated nested repositories, held locks, directory lock markers, or
unreadable entries are preserved. Released lock files left by CMake/Gradle do
not block review of an old build tree. Git state, timestamps, size, and file count are rechecked before
each deletion. Active build/test/formatter processes block deletion; the helper
does not stop processes. Close tools before cleanup, and do not start new work
during a cleanup run. Windows process checks are required for deletion; other
hosts support preview only.

Discovery is confined to known generated directories inside this repository.
It does not clean `game_data`, recorded regression demos, test fixtures, source
assets, credentials, SDK installations, external dependency directories, or
global Gradle/vcpkg caches. An ignored file alone is not evidence of disposable
content. Use the SDK manager to remove SDK packages and their dependencies.

Identified build generations include their CMake FetchContent dependency clones:
`<CMake binary directory>/_deps/*-src/.git`, with `CMakeCache.txt` present in
the owning binary directory (or the matching live ABI sibling for `.stale-*`
recovery directories). These downloaded sources, including build-local
patches and dependency submodules, are disposable with that build. Submodule
`.git` files are deleted as files; their referenced paths are never traversed.
Other nested repositories remain protected.

For the existing narrower timestamp-generation retention policy, use
`android/helpers/clean-old-artifacts.ps1`; its behavior is unchanged.

Test the new helper without touching real artifacts:

```powershell
.\android\tests\test_clean_workspace.ps1
```
