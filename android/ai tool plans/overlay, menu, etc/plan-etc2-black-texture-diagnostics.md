# ETC2 black texture diagnostics

## Problem
Both emulator and phone show all-black 3D textures (walls, ceilings, robots)
when d2 128 hi-res ETC2 texture pack (.dxa) is applied.
HUD/cockpit renders correctly. ETC2 uploads all succeed (err=0x0).

## Root cause finding
The "all-black" observation on the emulator was a **stale introspection value**.
The `framebuffer_sample` in introspect.json captured data from a menu/loading
frame (where the framebuffer IS black), not from active 3D gameplay.

Live introspection during gameplay shows colored pixels (88,96,96,255).
Multiple diagnostic layers confirmed the rendering pipeline works:
1. ETC2 self-test: GPU decodes compressed data to colored pixels (92,84,59,255)
2. Data integrity: all compressed payloads have non-zero data (etc2_zero=0)
3. Render bind log: correct textures bound with correct handles
4. Per-draw readback: 3D scene renders correctly after 20 draws
5. End-of-frame readback: varying colors across frames (14-244 range)
6. gr_flip readback: consistently colored (88,96,96,255) for 270+ frames

Phone testing still needed -- phone GPU may have a different ETC2 issue.
The self-test readback line will be the definitive phone check:
`ETC2 self-test: <name> readback=(<r>,<g>,<b>,<a>) OK|BLACK`

## Diagnostics kept permanently in d2/arch/ogl/ogl.c (and d1)
- ETC2 data integrity: hex dump of first 4 bytes, nz64 count, ALL-ZERO warning
- FBO self-test: render+readback of first ETC2 texture, with GL error drain
- Render bind log: first 5 3D texture bindings per level (name, handle, filter)
- Cache summary: etc2_zero count in texture stats
- Framebuffer sample: center pixel readback in gr_flip for introspection

## Temporary diagnostics removed
- ogl_end_frame readback block
- gr_flip_enter pre-palfx readback (s_preflip_log counter)
- gr_flip_post_palfx verbose logging (s_post_palfx_log counter)
- g3_draw_tmap per-draw diagnostic with readback after draw #20
- r_etc2_3d_readback_done variable

## Status
- [x] Phase 1: Data integrity logging
- [x] Phase 2: Render verification logging
- [x] Phase 3: FBO self-test (was initially skipped, then implemented)
- [x] Phase 4: Build and test on emulator -- ETC2 renders correctly
- [x] Diagnostic cleanup -- temp code removed, permanent diagnostics retained
- [ ] Phone testing -- user needs to test with current build
