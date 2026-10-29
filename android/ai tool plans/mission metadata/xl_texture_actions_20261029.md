# XL texture-change actions

Implement authored type-19 actions as an owned XL behavior, using the completed
texture-resource and event foundations. Redux retains its existing alternate.
Pumo has fifteen such actions, but removing this requirement does not qualify
any original level while its other blockers remain.

## Work

1. Execute pinned `CTrigger::DoChangeTexture` and `CSide::SetTextures` with
   independent side storage. Establish packed base/overlay values, signed
   overlay retention, 14-bit overlay assignment, unchanged orientation,
   multiple/duplicate/empty targets, and audio-route invalidation. Audit the
   source message and scheduling consumers as well.
2. Validate target geometry and all future texture dependencies before admission.
   Include effect/destruction assets reachable through a newly selected material.
   Invalid IDs and missing resources must reject without changing the active
   level. Keep object owner restrictions and all unrelated gates.
3. Dispatch wall, master, delayed/autoplay and supported object actions through
   the same XL texture implementation. Preserve overlay orientation even when
   clearing its texture; fix consumers that confuse orientation-only bits with
   an actual overlay. Invalidate cached sound routes after the operation.
4. Exercise actual material transparency/collision and renderer output, then
   ordinary saves, rewind, transitions and Redux controls. Use controlled
   engine events and poses, never autonomous navigation. Test the case where
   an overlay is cleared and then restored with its previous orientation.
5. Reuse existing test setup where it supplies meaningful coverage; avoid
   repeated fixture generation or unrelated CI work. Run relevant native,
   source and serial Android controls, required catalogs and scoped formatting.
   Publish only the completed, reviewed increment and monitor CI without merging.

The source changes side texture IDs without rewriting UVs, orientation or
lighting records. A negative high word leaves the overlay unchanged; a
nonnegative high word is assigned to a 14-bit field. The low word is always
assigned as the base texture and is not an unchanged-value sentinel. Actual
resource bounds still apply before the engine may use those IDs.

The bounded increment is complete. Source controls, both native layouts,
43/46 public contracts, 104 private contracts and all 67 Android steps pass.
The [evidence](../../XL_compat/evidence/texture-actions.md) records original
requirement changes, platform fixture parity and remaining campaign limits.
