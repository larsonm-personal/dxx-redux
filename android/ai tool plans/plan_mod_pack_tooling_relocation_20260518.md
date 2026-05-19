# Mod pack tooling relocation plan

## Goal
- Move mod-pack conversion tooling under `game_data/mods/` so source inputs, generated outputs, scripts, and docs are grouped by pack family
- Keep the reusable Xfing conversion scripts and important docs available for commit while ignoring bulky source/generated data
- Move D2X-XL texture pack tooling out of `game_data/mods/` root into `game_data/mods/d2x-xl/`

## Tasks
- [x] Inspect current references to old Xfing and D2X-XL locations
- [x] Update Xfing script defaults to use `game_data/mods/xfing/`
- [x] Move D2X-XL texture pack scripts, docs, and generated files into `game_data/mods/d2x-xl/`
- [x] Update references and documentation for both locations
- [x] Adjust `.gitignore` rules so scripts and important docs are trackable, while large packs and generated DXAs stay ignored
- [x] Run reference checks and script validation

## Results
- Xfing scripts now live in `game_data/mods/xfing/`, with defaults rooted at that directory and baseline game data resolved from the repo root
- D2X-XL conversion scripts, README, source archives, and generated DXAs now live in `game_data/mods/d2x-xl/`
- `game_data/mods/` now has a top-level README and `.gitignore`, plus per-pack `.gitignore` files
- Git ignore checks show Xfing and D2X-XL scripts/docs are trackable, while source archives and generated DXAs remain ignored
- Existing plan/docs path references were mechanically updated from the old locations

## Validation
- Direct PowerShell parser check passed for all relocated scripts
- Direct PSScriptAnalyzer check passed for relocated scripts; the repo `run-code-quality.ps1` helper was also run, but it currently scopes PowerShell files to `android/`
- `game_data/mods/xfing/convert-xfing-minimal-dxa.ps1 -Game both` regenerated both plain texture DXAs
- `game_data/mods/xfing/verify-xfing-minimal-dxa.ps1` verified both generated DXAs
- D2X-XL path smoke check found moved scripts, source archives, and `tools/etc2tool/build/Release/etc2tool.exe`