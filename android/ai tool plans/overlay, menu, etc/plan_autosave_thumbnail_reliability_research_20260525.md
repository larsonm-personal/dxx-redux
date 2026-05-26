# Autosave Thumbnail Reliability Research 2026-05-25

## Goal
- [x] Research why autosave thumbnails are sometimes missing
- [x] Identify low-overhead options that avoid constant CPU/GPU load
- [x] Prefer fixes that can rescue an already-rendered frame on exit

## Investigation
- [x] Review existing save metadata and thumbnail writer paths
- [x] Review launcher/reader paths that decide whether a thumbnail is available
- [x] Trace autosave and quit flow ordering relative to rendering and GL teardown
- [x] Compare D1 and D2 behavior and note where hooks must be duplicated

## Output
- [x] Summarize likely failure modes
- [x] Propose implementation options with cost/risk
- [x] Recommend a small first change and validation path

## Findings
- Launcher thumbnails come only from the Android metadata trailer. The writer has fields for RGB6 thumbnail data, but the shared metadata write path does not currently populate them.
- Direct Android autosaves still call the save helper with blank thumbnails enabled, so even the classic save body can be all zero for minimize/exit autosaves.
- The Compose decoder treats all-zero RGB6 data as no thumbnail, which is good defensive behavior but exposes blank thumbnail writes.
- Lifecycle autosave is queued on pause, then native pause stops Android surface blits before the game thread consumes the save request. Any save-time framebuffer readback can race surface/egl lifetime.
- The OGL path already resolves the MSAA FBO before swap/readback, so a low-load "last thumbnail snapshot" can piggyback on that point without a constant per-frame readback.

## Recommendation
- First fix the trailer plumbing: make the save thumbnail writer keep the last RGB6 thumbnail bytes from the current save and pass them into `android_save_meta_write_physfs()`. Then stop forcing blank thumbnails for direct Android autosaves only if save-time readback is known safe on the target lifecycle path.
- Better follow-up: add a dirty/throttled Android thumbnail snapshot captured from the already resolved framebuffer at `gr_flip()` before `eglSwapBuffers`, refreshed only after gameplay frames or when an autosave/exit request is pending. Autosave can then write from the cached snapshot instead of rendering and reading after pause.
- Validation should inspect `ResumeSaveBridge.findNewest()` for `has_thumbnail=true` and verify decoded thumbnail is non-null for D1 and D2 auto_minimize plus auto_exit saves.

## Implementation 2026-05-25
- [x] Added a transient RGB6 thumbnail cache in `android_save_meta`
- [x] D1 and D2 save writers now populate the cache when they successfully write a save thumbnail
- [x] Android metadata trailers now attach the cached thumbnail for the same save
- [x] Direct Android autosaves now request normal thumbnail generation instead of a forced blank thumbnail
- [ ] Validate on-device for D1/D2 auto_minimize and auto_exit
