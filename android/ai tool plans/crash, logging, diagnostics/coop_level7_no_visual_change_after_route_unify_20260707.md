# Coop level 7 no visual change after route unify

## Goal
- Review build 17302 coop logs after removing the broad coop-only old-texmerge route
- Confirm whether the tracked wall now follows the single-player-style render path
- Identify the next likely coop-only cause or add a targeted fix/diagnostic

## Plan
- [done] Re-read project instructions and extract route/hash/state records from the new log
- [done] Compare build 17302 against the previous old-texmerge logs
- [done] Inspect render and multiplayer setup paths for the next remaining coop-only difference
- [done] Patch a narrow fix or add a sharper diagnostic, then validate

## Findings
- Build 17302 did remove the broad coop-only old-texmerge route: the tracked face now logs `route=merge_cached merge_impl=gpu_cached_single`.
- The tracked face still reads back as the same merged RGBA hash as before, `0xa4d6be4d`, with base `rock198` and overlay `ceil035` readbacks matching source.
- The visual result did not change, so the broad route split and stale secondary-unit theories are both falsified for this visible symptom.
- The tap probe still reports `status=no_crosshair_face`; it logs one selected fallback (`seg=169 side=0 face=0`), but the crosshair center is not inside that face's tracked box.
- `segment_sig` stays stable through multiplayer prep. The `texture_sig` churn includes bitmap paging/upload flags, so it is not by itself proof of level texture remapping.

## Patch
- Added forced `mwall_tap_probe kind=face_candidate source=selected_all` lines for up to six selected tracked faces when the crosshair misses.
- Kept the existing detailed fallback readback bundle on the single selected fallback, so log volume stays bounded.

## Validation
- `.\android\run-code-quality.ps1 -Fix -Paths @('android\app\src\main\cpp\shared\merged_wall_debug.c','android\ai tool plans\crash, logging, diagnostics\coop_level7_no_visual_change_after_route_unify_20260707.md')`
- `$env:JAVA_HOME='c:\local\jdk-21'; $env:Path="$env:JAVA_HOME\bin;$env:Path"; .\gradlew.bat :app:externalNativeBuildDebug`
