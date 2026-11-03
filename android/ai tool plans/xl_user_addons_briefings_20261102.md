# User-supplied XL addons, briefing sound and loading preference

The user stopped the timed playtest goal and rejected bundling proprietary addon
files. The unfinished bundling code has been removed; no APK containing that
payload was built. Keep the already published bundles as historical baselines.

1. Inspect the supplied local D2X-XL distribution catalog without bulk extraction.
   Group optional d2x-xl.hog and d2x.ham under XL addon files in the D2 tracker.
   Support normal raw-file import and selective archive import. Retain existing
   mission-specific dependency checks rather than blocking every XL mission.
2. Use metadata-first 7z selection with collision checks before extraction and
   decode only selected entries (solid blocks may still require preceding data).
   Keep general import behavior; an explicit addon import scope selects no other
   distribution files. Do not ship proprietary resources in APK assets or Git.
3. Verify original Pumo auto-pagination and stop looping printing audio at every
   completed page, including overflow pages. Preserve text and authored paging.
4. Verify the progress setting. No current show-progress-bars preference was
   found in source; add one shared by all loading overlays if absent, enabled
   for fresh settings and both Original Descent and Defaults presets.
5. Run focused JVM/import and native/Android integration checks on the owned
   disposable emulator, preserve its profile, and deliver an APK with no addons.
   Phone access stays read-only; normal user UI imports supply required assets.

## Results (November 2)

- General Import Files also recognizes addon-only distributions by catalog
  contents, without a distribution-filename special case. Mixed retail imports
  retain their broader selection. Explicit addon import always limits selection.
- The supplied 1.5 GB distribution contains d2x-xl.hog but no d2x.ham. Direct
  catalog/selected-entry extraction through the app's Commons Compress path took
  about 0.45 seconds locally; streaming providers retain staged-copy fallback.
- Optional grouped tracker, raw/ZIP/7z publication and loading visibility passed
  instrumentation on owned DxxPhoneFog20261102. All 27 focused JVM tests passed.
- Original Pumo uses automatic overflow pagination. Also fixed physical-height
  clipping in both engines by including the full glyph height in the page check.
  Exact normal APK 022a192/version 29330 passed 18/18 briefing steps: first page
  overflow=true, fits_height=true, printing_sound=false; sound restarts and stops
  on the next page. The captured first page was visually inspected.
- Universal Android and Windows D2 builds, scoped quality and automation catalogs
  passed. All six APK native IDs match; no HOG/HAM/HXM/archive addon assets exist
  in the APK. Physical Samsung acceptance of this new candidate remains pending.
- Delivery and continuation record: temp/pumo-user-addon-playtest-20261102/
  final_handoff.md. The old timed goal remains paused at the user's request.
