# Briefing text scale

- [x] Trace native D1, D2 and imported D1 sizing and wrapping
- [x] Scale briefing glyphs, tabs and line spacing with the fitted canvas
- [x] Preserve the existing 640x480 text proportions and restore global font scale after each draw
- [x] Correct imported D1 overflow to advance by one line
- [x] Extend real briefing rendering checks for text geometry, narrow render canvases and overflow
- [x] Run scoped quality checks, both host builds, briefing comparisons and Android native builds

The 4:3 canvas correction narrowed text boxes without changing the screen-based
font scale. A 640x480 render buffer displayed at 16:9 uses a 480x480 briefing
canvas, requiring horizontal font correction as well. Use the existing 640x480
presentation as the reference: low-resolution fonts render at 2x, high-resolution
fonts at 1x. Fractional canvas scaling keeps text proportions stable across sizes
and non-square render pixels. Imported D1 also retained the legacy overflow bug
that uses the text box's top offset as its line advance

Validation:

- All 84 pre-existing 640x480 reference frames remained pixel-identical
- All 144 native D1 frames matched imported D1 with and without D2 assets
- Pixel measurements verified glyph width and line spacing at 640x480, full HD,
  widescreen, portrait and non-square pixel layouts, including 640x480 buffers
  displayed at 16:9 and 20:9
- Ordinary D2 high-resolution font geometry passed at 640x480, full HD and the
  narrow phone-buffer layout
- Drawing restores the previous global font scales in all rendered cases
- Windows D1 and D2 builds and Android arm64 Debug native builds passed
- Scoped formatting and lint passed; existing weapon return-path and D2
  printing-channel assignment warnings remain
- No physical phone run was performed

Artifacts: `temp/briefing-text-comparison.log`, `temp/d1-briefing-comparison/`,
`temp/briefing-text-host-build.log`, `temp/briefing-text-android-build.log`,
`temp/briefing-text-quality.log`
