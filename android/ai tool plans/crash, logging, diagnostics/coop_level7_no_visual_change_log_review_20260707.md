# Coop level 7 no visual change log review

## Goal
- Review the latest coop level 7 log after the secondary texture unit clear fix
- Explain why the bug remains visible in coop but not single player
- Identify the next likely cause or narrow fix

## Plan
- [done] Re-read project instructions and extract key records from the new log
- [done] Compare post-fix draw state, texture reads, and coop-specific setup against the last log
- [done] Re-check code paths that differ between coop and single player rendering or level setup
- [done] Record findings, patch if a narrow safe fix is indicated, and validate

## Findings
- The secondary texture-unit clear fix is active: tracked old-texmerge single draws show the merged texture bound on unit 0 and clear unit 1/2 state.
- The tracked merged bitmap still verifies cleanly against CPU and GL readbacks, including draw-time readback, so the latest logs do not support stale GL unit state or corrupt legacy merge data as the remaining cause.
- The tracked visible face is still routed through `auto_old_texmerge` for `coop_plain_transparent_overlay`, which is a broad multiplayer-only rendering fork. Single player does not take this path.
- The next narrow fix is to remove that broad coop-only old-merge route so coop uses the same Android alt-texmerge path as single player. The explicit legacy-texmerge experiment remains available for diagnostics.

## Patch
- Removed the Android multiplayer-only `render_android_should_force_multiplayer_oldmerge_tmap2` helper from D1 and D2.
- Removed the `coop_plain_transparent_overlay` branch that automatically forced plain transparent multiplayer overlays to the old CPU texmerge path.
- Kept the explicit `MERGED_WALL_EXPERIMENT_FORCE_LEGACY_TEXMERGE` path and the existing logging-target fallback path.

## Validation
- `.\android\run-code-quality.ps1 -Fix -Paths @('d1\main\render.c','d2\main\render.c')`
- `$env:JAVA_HOME='c:\local\jdk-21'; $env:Path="$env:JAVA_HOME\bin;$env:Path"; .\gradlew.bat :app:externalNativeBuildDebug`
- Direct full-file clang-format dry run is not a useful signal for these legacy render files; it reports broad preexisting style violations unrelated to this deletion-only patch.
