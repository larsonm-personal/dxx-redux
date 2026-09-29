# Briefing aspect correction

- [x] Trace D1, D2 and imported D1 briefing backgrounds and robot projection
- [x] Fit the complete briefing layout to its authored 4:3 display using the existing pixel aspect correction
- [x] Keep sprites and D2 robot movies positioned relative to the briefing canvas
- [x] Run scoped formatting, both native builds and relevant briefing regression checks

The launcher already writes the physical display ratio to AspectX/AspectY. The
renderer derives sc_aspect from that ratio and the rendering resolution. The
3D model-picture renderer uses sc_aspect and its subcanvas dimensions in
g3_start_frame_projection, so robot proportions need no separate correction.
320x200 artwork has legacy non-square pixels and belongs on a 4:3 display,
just like the 640x480 replacements.

Validation:
- Scoped code quality checks passed
- Windows D1 and D2 CMake builds passed; existing weapon.c return-path warnings remain
- Real briefing renderer comparison passed: 102 frames match between native D1 and imported D1
- Coverage includes 640x480, 1280x720, 720x1280 and non-square render pixels,
  with canvas bounds and black-margin assertions; wide and tall PNGs visually reviewed
- The briefing harness now requests 32-bit video, matching normal startup, to avoid
  SDL silently falling back to 640x480 when testing other resolutions
- Android arm64 Debug native builds passed for both engines; existing D2 printing_channel spacing warnings remain
