# XL audio route invalidation

Worktree: `C:/local/dxx-redux/temp/xl-batch2-20261027`.
Branch: `xl_more_67_audio_routes`, above PR #216.
The authorized block continues through October 30 at 04:57:30 UTC.

1. Execute pinned `CAudio::Update` with the actual source distance/router code.
   Establish stale-route behavior and explicit invalidation while the listener
   remains in the same segment. Inventory source wall/texture update sites.
2. Add an XL-owned route invalidation entry point that preserves voices,
   playheads, deferred creation and static visibility. Wire the corresponding
   texture, door, cloak and exploding-wall lifecycle events; preserve Redux.
3. Exercise actual wall/texture changes with a stationary listener, source
   route comparisons and real output, including blocked/open transitions,
   retained ownership and save/restore. Extend existing test owners.
4. Run affected native/Android checks, document exact coverage and remaining
   limits, publish a stacked draft and inspect CI. No timeout relaxation.

Reference: arbruijn/d2x-xl `2576a98b5de310e3ac7c9777df2677d1627b8bae`,
`include/audio.h`, `audio/objectaudio.cpp`, `main/wall.cpp` and `main/trigger.cpp`.
The source clears start/destination router identities on explicit updates;
ordinary queries otherwise reuse their listener-segment route cache.

## Verified result

The regression first fails on the unfixed engine with an inaudible owner after
opening a texture route. The corrected Windows and Linux fixtures pass both
23/40 and 27/40 with ten actual PCM outputs, intermediate route queries,
retained owner identity, earned runtime restoration and unchanged deferred state.
The source adapter executes the actual inline invalidator and distance/router
functions, including stale closed/open caches and unchanged listener identity.

Android passes 101/101 steps in 392.735 seconds with 1,467,392 checked samples,
111 output cases and all 26 protected files restored unchanged. The complete
private owner passes all 106 required tests, 16 original-format outcomes and
three original modern source comparisons in 421.187 seconds. Windows, Linux and
Android builds, both catalogs, scoped formatting and diff checks pass.
See [route evidence](../../XL_compat/evidence/audio-routes.md) for exact receipts,
hashes and limitations. No admission expansion or timeout increase is included.

Next implement external WAV linked-owner creation and saved resource validation,
then sound-effect object activation and lifetime handling. The existing resource
resolver already qualifies the authored files, but external linked owners are
deliberately rejected by the restore decoder and effect objects remain gated.
