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
