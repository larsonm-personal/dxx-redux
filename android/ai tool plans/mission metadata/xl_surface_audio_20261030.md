# XL ambient surface audio

Worktree: `C:/local/dxx-redux/temp/xl-batch2-20261027`.
Branch: `xl_more_66_surface_audio`, above PR #215.
The authorized block continues through October 30 at 04:57:30 UTC.

1. Extract the actual pinned `SideIsSoundSource` and `SetSoundSources` consumers.
   Compare visibility, overlay/base selection, connected-side suppression and
   per-segment volume distribution, without substituting copied source logic.
2. Add an XL-owned surface initializer, retaining the existing Redux initializer
   as the runtime alternate. Preserve permanent/deferred ownership, logical
   translation, forcefield scaling and side-center geometry.
3. Exercise the real initializer on loaded geometry with captured PCM, restart,
   saved ownership and stock isolation. Share native/Android controls and add
   source/native comparisons to existing audio test owners.
4. Run affected builds and tests, update evidence, publish a stacked draft PR,
   and inspect CI. No new CI process or relaxed timeout is planned.

Source: arbruijn/d2x-xl `2576a98b5de310e3ac7c9777df2677d1627b8bae`,
`audio/objectaudio.cpp` and relevant wall/segment declarations. XL normalizes
surface gain to F1_0 / (2 * emitting sides) within each segment. Its overlay
identity is separate from orientation, unlike Redux's packed texture word.
External sound objects, static explosion ambience, wall-change lifecycle and
classic replacement installation remain separate requirements, with admission
unchanged until those requirements are implemented and qualified.

## Initial CI publication

The surface initializer and shared native/Android controls are implemented.
The Windows native build and pinned-source suite pass, including 775 surface
selector/setup controls. Both 23/40 and 27/40 loaded-level runs pass; each
compares 16 surface cases and 35 owners against the actual source initializer.
The existing audio scope and loaded-geometry source comparisons also pass.
Scoped formatting passes.

This is a draft checkpoint for CI, not completed platform qualification.
The new Android surface controls and Linux build/runtime still need execution.
A dedicated exact-PCM stock surface comparison remains to be added; existing
stock combat and afterburner isolation checks pass. Admission is unchanged.
