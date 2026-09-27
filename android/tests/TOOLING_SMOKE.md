# Windows/Linux tooling smoke checks

From the repository root, with PowerShell 7, Git, and Python 3 available:

Linux requires Python 3.9 or newer and a kernel supporting pidfds (Linux 5.3 or
newer). The shared process pool uses a supervisor to reap owned descendants when
a worker finishes or its PowerShell runner is forcibly killed, including children
that start a separate session. The smoke checks exercise both cases. Direct
process launches outside the shared pool need their own lifetime handling.

```powershell
pwsh -NoProfile -File android/tests/run_tooling_smoke.ps1
```

This profile runs platform selection, workspace/artifact cleanup, process ownership,
managed dependency retirement, SDK package/AVD reference inventory, game-data
discovery, process-output capture, and
replay comparison checks. It needs no native build, proprietary game data, SDK,
or emulator. On Linux it also runs the synthetic Bash installer tests, requiring
the usual Bash archive/hash utilities. Those fixtures simulate Windows archive
packages but do not execute Windows binaries.

After installing the pinned runtime with
`bash android/get_deps/helpers/get_bounded_python.sh`, add `-IncludeBoundedRuntime`
to check runtime admission, bounded process supervision, and archive extraction.
This still needs no SDK or game data; real demo-installer fixtures are checked
when available. The runtime and test workspaces have bounded cleanup.

Each test runs in a separate process with a three-minute deadline. Any failure,
timeout, or unexpected skip fails the profile. Logs and an incremental JSON summary
are written under `android/temp/tooling_smoke/run_<id>`. Startup retains three prior
reports and checks disk space; a producer lock protects the active report. An
explicit `-OutputRoot` must name a new directory with the `run_` prefix.

The `Android tooling` workflow installs the pinned runtime and runs these extended checks on Ubuntu 24.04 and Windows
Server 2022 for relevant pushes/pull requests and manual dispatch. Actions are
pinned to commits, superseded runs are cancelled, and uploaded reports expire
after three days. This workflow does not establish APK/AAB, native/JVM, sanitizer,
or device parity; those remain separate build and test profiles.
