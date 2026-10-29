# Authored fog and visibility

Continue the authorized twelve-hour run ending 2026-10-29 04:54:16 UTC.
Branch xl_more_42_fog starts from 23830d44e0931596375b0ae001aa69d4bb8c9370,
PR #187. Keep the user's root cmake checkout unchanged. Monitor the open stack,
publish reviewable increments, and do not merge. Use controlled events and poses,
not autonomous level navigation.

## Scope and dependencies

Canonical 25/27 layouts and corrected generator initialization are implemented
and locally verified on the parent branch. Pumo remains blocked independently by
events, effects, resources and environment behavior. Eleven of its fourteen
levels select authored fog. Removing that blocker requires the visibility effect,
not just reading the table or coloring a boundary.

1. Trace the pinned XL source from segment properties and fog settings through
   visible boundary selection, near/far accumulation, depth snapshots and final
   composition. Record source quirks and option-dependent behavior explicitly.
   Ignore the obsolete ogl/fog.cpp sample; the live implementation is in
   render/fastrender.cpp and effects/postprocessing.cpp.
2. Add owned fog data and source comparisons for type priority, byte conversion,
   boundary selection and composition, including zero distance, overlapping
   types, disconnected volumes, camera-inside and occlusion cases. Compare the
   complete setup path rather than an isolated formula alone.
3. Implement a separate XL rendering path with actual scene depth and the native
   projection. Keep stock rendering inactive and preserve GL state, viewport,
   transparency, auxiliary views, MSAA and context lifecycle. Require verified
   GPU capabilities instead of silently substituting a different effect.
4. Exercise real GPU output with known geometry and source-derived expectations,
   then the normal Android game renderer with controlled views, mission changes,
   save/restore, lifecycle and stock isolation. Add a reusable registered runner.
   Keep headless mathematical/geometry evidence distinct from rendered evidence.
5. Open admission only for the proven behavior. Refresh original requirements,
   update the evidence/roadmap, run scoped quality and affected platform checks,
   publish the increment and monitor CI. Unrelated Pumo blockers remain closed.

The reference uses a 1-unit near plane, while Redux currently uses 0.1. Select
the reference projection only for the active volumetric XL profile and use it
consistently in scene rendering and depth decoding. Stock retains its existing
projection. Float-buffer precision and blend support require actual capability
checks and GPU controls.

## Initial validation checkpoint

The extracted-source fog comparison, modern layout and inventory contracts pass
on MSVC and strict Linux GCC/UBSan. The actual desktop OpenGL backend passes the
offscreen interval/occlusion/composition controls, including both depth formats,
multisampling and zero-distance settings. The normal Windows and Android engines
build. Parent PR #187 is green across all seven workflows.

The emulator's legacy SwiftShader backend does not transfer scene depth. Direct
texture clear/sampling works; depth blit and copy do not. A measured capability
check now rejects this driver before admission, with a scene-depth explanation.
All 17 driver-rejection and subsequent stock-start controls pass. The host NVIDIA
backend passes both depth formats and the full 180-case GPU suite.

Normal game views now pass numeric pixel comparisons, including 2x/4x MSAA and
disk/rewind restore. A closed equal-type internal door deliberately produces no
fog in the reference algorithm; that view is an explicit negative control beside
a fogged exterior face. The test runner uses the existing resume API after restore
and treats MSAA-off as a safe setting without a confirmation challenge. The
persistent graphics queue retains MSAA through launcher resume; the temporary
native setter intentionally does not. All 208 controls now pass, including real
context loss, campaign transitions, saved ownership and stock isolation. All 28
protected files, including graphics safety state, are restored exactly. The
source report, public 14-test configuration and strict native/GPU checks pass.
No original Pumo level is newly qualified by this local work.

The affected canonical-layout regression also passes all 87 steps. Final
admission checks include the actual surface format, full-size allocations and
depth transfer before the level is committed. The updated native build, desktop
GPU control and all 208 Android controls pass; the final Android run takes
227915 ms. All 17 final driver-rejection controls also pass after restoring the
emulator's original backend. Both runs restore all 28 protected files exactly.
Publish and monitor CI. The newly requested [private CI work](xl_private_ci_20261028.md)
follows this increment and reuses the existing trusted asset access.
