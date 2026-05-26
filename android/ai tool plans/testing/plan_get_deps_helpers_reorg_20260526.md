# get_deps helper reorg 2026-05-26

## Scope
- Keep only top-level dependency entry scripts in `android/get_deps`
- Move implementation scripts to `android/get_deps/helpers`
- Update relative script paths after the move

## Assumptions
- Top-level entry scripts are `check-updates.ps1` and `get_all.sh`
- `README-ubuntu.md` and `tool_versions.conf` stay in `android/get_deps` as docs/config

## Tasks
- [x] Inventory current dependency scripts and references
- [x] Move helper scripts into `helpers`
- [x] Update relative paths in entry scripts, config, and callers
- [x] Validate script syntax and unresolved references
