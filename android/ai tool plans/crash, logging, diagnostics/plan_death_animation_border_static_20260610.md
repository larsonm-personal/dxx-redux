## Goal
Find why the death animation shows static in the top and bottom borders.

## Plan
- [x] Trace the player death render path.
- [x] Check OpenGL frame clearing, viewport, and canvas sizing around the game view.
- [x] Identify the most likely cause and propose the smallest fix.

## Notes
- Initial request is diagnostic only, so avoid gameplay/source edits unless the cause is clear and the user wants a fix.
- Death selects `CM_LETTERBOX`, which narrows `Screen_3d_window` to the middle 3/4 of the screen.
- Letterbox top/bottom are cleared during cockpit setup, but not redrawn every frame.
- Android OGL does not clear the color buffer after swap, so swapped buffers can expose stale or uninitialized content in those bands.
