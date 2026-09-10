# Comprehensive unattended temp cleanup

User authorizes all generated temporary workspace files to be removed without prompts. Pause routing and FVI review

1. Extend clean-workspace.ps1 to discover scratch roots at arbitrary repository depths and clean every ignored file type regardless of run names or age
2. Preserve tracked/nonignored work, original game assets, nested repositories, links and live jobs; clean eligible siblings rather than losing entire collections to one protected child
3. Remove interactive prompts, retain Preview/WhatIf and producer build retention, add TemporaryOnly for focused emergency reclamation
4. Test with synthetic repositories including fresh/custom/nested outputs, protected siblings, links, locks, busy jobs and build retention
5. Run cleanup to completion on this workspace, verify recovered disk space and a repeat scan, document default behavior


Implementation: default all-type ignored scratch cleanup with TempDays0, arbitrary-depth discovery and custom run names, TemporaryOnly mode, no Read-Host calls and ConfirmImpactNone. Preview/WhatIf remain read-only; Verbose lists candidates. Native/package retention is still separate, with scratch ownership taking precedence to avoid overlapping candidates. Reuses Git/path/link/lock containment checks and identifies reproducible CMake dependency clones. Known orphan emulator marker directories can be cleaned only through the idle-checked temporary path. Expanded process guard includes native game binaries and emulators, waits up to60seconds automatically for transient jobs, and never kills them

Synthetic test_clean_workspace.ps1 and scoped PowerShell formatter/linter pass, covering current/custom/nested temp payloads, tracked and untracked source protection, fresh files, nested repositories, links, held locks, generated dependencies, stale AVD markers, busy/temporarily busy jobs, scan-to-delete content and Git staging races, existing build-family retention and narrow payload-only behavior. First cleanup pass safely stopped on two transient producer PIDs after freeing about 25 GiB; processes exited. The second pass used automatic idle wait and completed successfully, removing 2797 remaining artifacts (217.94 GiB logical). Drive C has 243.69 GiB free, up from about 200 MB. A subsequent TemporaryOnly preview found zero eligible artifacts; only protected nested source repositories and links remain. No tracked files were deleted and scoped git diff --check passes. User explicitly authorized deletion of the temp reports and backups from previous routing/review runs; durable conclusions remain in plans and checked-in mission JSON
