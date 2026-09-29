# Briefing aspect correction

- [x] Trace D1, D2 and imported D1 briefing backgrounds and robot projection
- [ ] Fit the complete briefing layout to its authored 4:3 display using the existing pixel aspect correction
- [ ] Keep sprites and D2 robot movies positioned relative to the briefing canvas
- [ ] Run scoped formatting, both native builds and relevant briefing regression checks

The launcher already writes the physical display ratio to AspectX/AspectY. The
renderer derives sc_aspect from that ratio and the rendering resolution. The
3D model-picture renderer uses sc_aspect and its subcanvas dimensions in
g3_start_frame_projection, so robot proportions need no separate correction.
320x200 artwork has legacy non-square pixels and belongs on a 4:3 display,
just like the 640x480 replacements.
