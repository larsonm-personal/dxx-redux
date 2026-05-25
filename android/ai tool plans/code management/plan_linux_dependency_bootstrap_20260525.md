# Linux dependency bootstrap cleanup

## Plan
- [x] Survey android/get_deps scripts and current dependency flow
- [x] Make fresh Linux bootstrap easier without locking scripts to Ubuntu only
- [x] Add a very short Ubuntu install readme
- [x] Run syntax or smoke checks that do not require downloading every tool

## Notes
- `get_all.sh` now includes host prerequisite, PowerShell, shellcheck, and shfmt steps
- `resolve_dep_base.sh` now creates `dependency_base.txt` with the platform default if it is missing
- `get_powershell.sh` is the new Linux-aware entry point, while `get_powershell_ubuntu.sh` remains as a wrapper
- Validated with `bash -n`, helper `--help` runs, Linux prereq `--dry-run`, and a GitHub asset lookup for the pinned PowerShell release
- Follow-up run found `set_vars.sh` was not safe under `set -u`; patch it before rerunning `get_all.sh`
- Full `get_all.sh --no-prompt` now completes on this fresh Linux VM and a second run skips installed dependencies
- Gradle wrapper startup and project `./gradlew help` both pass after making `android/gradlew` executable
- Standalone helpers no longer fail noninteractive runs just because the final keypress prompt has no TTY
