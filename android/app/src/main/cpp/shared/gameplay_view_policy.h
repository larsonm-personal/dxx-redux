/* Fixed ship-forward gameplay view, independent of the displayed camera */
#ifndef GAMEPLAY_VIEW_POLICY_H
#define GAMEPLAY_VIEW_POLICY_H

/* DOS Descent cockpit reference, chosen explicitly for gameplay visibility
 *
 * The physical monitor was 4:3, but its 320x200 pixels were not square. The
 * original cockpit's transparent bounding rectangle is (0,0)..(319,132), so
 * the 3D canvas is 320x133, not a 4:3 viewport. Preserve the integer operations:
 *   screen aspect = floor(65536 * 320 * 3 / (200 * 4)) = 78643
 *   X scale       = floor(78643 * 133 / 320)           = 32685
 *   Y scale       = 65536; Z scale = original DOS zoom 0x9000 = 36864
 * Clipping uses |X| <= Z, |Y| <= Z after those scales. Hence this reference is
 * about 58.715507 degrees vertical and 96.877210 degrees horizontal. The
 * angles are explanatory only; production uses the exact fixed-point values
 *
 * Original sources, pinned to preserved releases:
 * https://github.com/videogamepreservation/descent/tree/f6fb1c30a06725b403dfa7db4b251ecea1c3efc1
 *   MAIN/RENDER.C:354 (DOS zoom; Mac used 0xB000)
 *   2D/GR.C:594-598 (pixel correction), 3D/SETUP.ASM:139-170 (canvas scale)
 *   3D/MATRIX.ASM:110-142 (zoom), MAIN/GAME.C:705-736 (cockpit canvas)
 *   2D/IBITBLT.C:348-369 (transparent bitmap bounds)
 * https://github.com/videogamepreservation/descent2/tree/75ba13c44259f1e40c0df70961c78abba898eafe
 *   SOURCE/MAIN/RENDER.C:76-80 and SOURCE/MAIN/GAME.C:510-592
 * Measured original D1 shareware 1.4 and D2 demo 1.0 cockpit assets agree on
 * the 320x133 rectangle; rear-view cockpit bounds deliberately do not apply
 *
 * This policy governs gameplay candidates and fire-warning projection. Visual FoV settings,
 * actual cockpit geometry, rear/HUD cameras and automap discovery keep their
 * separate renderer behavior. Fire warnings retain the original strict
 * |scaled X| < 4, |scaled Y| < 4 and scaled Z > 0 tests. At this reference their
 * world-space half-widths are about 8.020 horizontal and 4 vertical; this is
 * a forward box, not an angular-frustum or line-of-sight eligibility test
 */
#define GAMEPLAY_VIEW_WIDTH  320
#define GAMEPLAY_VIEW_HEIGHT 133

/* Alternative references: replace the active height above, keeping width,
 * screen aspect and zoom unchanged. These are intentionally not active
 * #define GAMEPLAY_VIEW_HEIGHT 200  // Fullscreen: V 58.715507, H 73.740914
 * #define GAMEPLAY_VIEW_HEIGHT 147  // Status bar: V 58.715507, H 91.158593
 */
#define GAMEPLAY_VIEW_SCREEN_ASPECT ((65536 * 320 * 3) / (200 * 4))
#define GAMEPLAY_VIEW_ZOOM          0x9000
#define GAMEPLAY_VIEW_X_SCALE       ((GAMEPLAY_VIEW_SCREEN_ASPECT * GAMEPLAY_VIEW_HEIGHT) / GAMEPLAY_VIEW_WIDTH)
#define GAMEPLAY_VIEW_Y_SCALE       65536

/* Preserve each current engine build's normal traversal budget. The software
 * backend has always used 20 here; do not silently widen it to the OGL limit
 * OGL headed and headless builds share the same MAX_RENDER_SEGS budget
 * Historical DOS detail presets are context, not a request to restore them
 * Keep temporary endlevel Render_depth changes out of gameplay collection
 * Native/default D1 and imported-D1 object sorting uses ClassicDepth=0
 */
#ifdef OGL
#define GAMEPLAY_VIEW_RENDER_DEPTH MAX_RENDER_SEGS
#else
#define GAMEPLAY_VIEW_RENDER_DEPTH 20
#endif
#define GAMEPLAY_VIEW_CLASSIC_DEPTH 0

#endif
