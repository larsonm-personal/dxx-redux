# D2X SP gitignore plan

## Goal
- Make the important converter sources under `game_data/mods/xfing/d2x_sp/` visible to git
- Keep generated archives, staging directories, and proprietary source payloads ignored

## Tasks
- [x] Update top-level gitignore rules for `d2x_sp`
- [x] Add a scoped `.gitignore` inside `game_data/mods/xfing/d2x_sp/`
- [x] Validate which files become visible to git

## Result
- Git now exposes the `d2x_sp` converter scripts and markdown notes
- Generated `.dxa` files, the `tmp/` staging area, and the `uud2sp/` source payload directory remain ignored
- The parent `game_data/mods/xfing/.gitignore` needed an explicit `d2x_sp/` allowlist for the nested rules to take effect