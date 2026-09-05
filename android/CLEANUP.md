# Workspace cleanup

Run from the repository root:

```powershell
.\android\clean-workspace.ps1 -Preview
.\android\clean-workspace.ps1
```

Mission metadata and guidebot runs can retain gigabytes of duplicate extracted
assets. The cleaner automatically removes per-mission directories under `raw`
and `stages` in timestamped `android/temp/mission_zip_host_metadata` and
`android/temp/guidebot_simulation_regression` runs, including metadata workers.
It keeps JSON files directly under `raw`, summaries, logs, and simulation results.
These payloads need no newer replacement run: the original mission archives remain
in `game_data`. A one-hour grace period uses both creation and modification times,
because asset copies preserve old source modification dates. Active-process and
Git/link/lock protections still apply.
Payload-only trees are checked and deleted as groups; folders mixed with reports
are cleaned per mission so the reports remain available.

```powershell
# Fast, focused preview and cleanup of reproducible regression payloads
.\android\clean-workspace.ps1 -PayloadsOnly -Preview
.\android\clean-workspace.ps1 -PayloadsOnly -AutoOnly
```

`-PayloadGraceHours` adjusts the one-hour grace period. `-PayloadsOnly` skips build
and general artifact scans and cannot be combined with `-BuildsOnly`.

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
or the parent directory's timestamp. The default keeps exactly one generation
with no age exemption; ties are resolved by path. `-KeepBuildGenerations 2` retains
two generations; `-BuildGraceHours 24` explicitly opts into a grace period. These automatic build
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

## Producer startup retention

Producers trim their own output family to the newest three prior generations
before creating the next output, leaving at most four after a new generation is
created. No age threshold applies. Reusing an existing output does not create a
fourth generation. The current/planned output is excluded from prior history,
including when a wrapper and child runner share it.

The shared `helpers/retain-recent-artifacts.ps1` accepts planned output paths.
Hooks cover metadata and guidebot batches, regression/test reports, demo matrix
outputs, warning logs, guidebot browser runs, and timestamped/deployment AAB copies.
On Windows, Gradle build/assemble/bundle/native tasks trim `app/.cxx` at root-project
configuration, before AGP configures the next native hash. Native configurations
are separate families; fixed build directories are reused rather than rotated.
Producer native cleanup excludes its initiating process chain and Gradle clients
from the idle check, but still refuses cleanup during active native builds or
unrelated tests. It never kills processes. Other hosts retain their existing native
build behavior; workspace native deletion currently requires Windows safety checks.

Manual cleanup still defaults to one newest generation. Producers keep three prior
outputs so running them offers more history without requiring cleanup flags.
