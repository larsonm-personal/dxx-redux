# Co-op score transition flash

Investigate the brief 3D or automap-like frame after the co-op scores screen
using source inspection without collecting logs

## Plan

1. Trace score dismissal, underlying window draws, loading, and co-op briefings in both engines
2. Correct a demonstrated transition rendering defect with a narrowly scoped change
3. Run scoped code quality and relevant native builds or focused checks
4. Record evidence and distinguish source-confirmed defects from an unverified visual match

## Findings and correction

- Both `multi_endlevel_score` implementations temporarily show `Game_wind` so
  the host can continue processing game actions underneath the score screen
- Both SDL event loops draw visible windows back to front, then call `gr_flip`
  even when the score window closes during its draw callback
- Both score handlers previously closed on timeout before `kmatrix_redraw`,
  exposing that frame's underlying draw rather than painting the score background
- Android's queued screen-advance request also returned before drawing when
  consumed during `EVENT_WINDOW_DRAW`, creating an additional uncovered frame
- An active automap draws wireframes in its own window, consistent with the
  reported appearance, but its presence during the reported flash is unverified

Moved the existing score/background/status draw before the expiry check in
both engines, and allowed queued Android advance requests to finish draw
events. D2's terminal score screen also draws before closing. The shared
draw-order defect affects desktop too, so the draw ordering fix is portable;
the queued-request change stays inside its existing Android guard

This keeps the score screen painted until loading or briefing replaces it,
without changing networking, simulation visibility, assets, or the global event loop

## Validation

The scratch harness at `temp/coop-score-transition-20261011/check.py` compiles
the actual production `kmatrix_handler` functions against window/render stubs
with MSVC. It seeds a simulated underlying frame and checks that every draw
paints once, including before closing the window

- D1: ordinary frame, timeout, and queued Android continue
- D2: those cases plus terminal timeout and terminal queued continue
- Before: both ordinary-frame controls pass; all six dismissal cases fail
- After: all eight cases pass

No device logs were collected and no new diagnostics were added. These checks
prove the skipped-draw defect and correction, not the exact visual match on
the user's device. Visual confirmation remains outstanding

Scoped code quality and `git diff --check` passed. Both changed translation
units compile in the Android arm64 Debug CMake build. Full linking is blocked
by unrelated C/C++ linkage errors in `game_introspect.cpp`: D1 references
`vm_vec_dist_quick` with C++ linkage, and D2 references that function and
`vm_vec_mag_quick`, while the math libraries export C symbols. No APK was
built or installed for this fix
