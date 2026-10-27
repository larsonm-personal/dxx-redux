# XL effect payload inspection

Implement on xl_more_11, stacked on equipment PR #153. This chunk improves
inspection and explicit rejection; it does not port effect gameplay or admit a
mission merely because its object table can now be read. No merges.

Reference: arbruijn/d2x-xl 2576a98b5de310e3ac7c9777df2677d1627b8bae,
objects/objectio.cpp and include/object.h.

## Scope

- Read particle, lightning, sound and empty thruster render payloads within the
  existing 15/33, 17/36 and 23/40 inspection profiles
- Preserve serialized identities/fields and expose reference-reader defaults and
  identity normalization separately. Retain all 40 sound filename bytes
- Read waypoint controllers and the known no-payload static movement encoding,
  retaining explicit runtime gates. The pinned reader warns on MT_STATIC despite
  its enum definition; this does not establish static-actor gameplay support
- Keep particle/lightning/ambient sound behavior, waypoints, static actors and
  unsupported effect object types blocked. Unknown layouts remain fail-closed
- Populate truthful per-source metadata counts and useful rejection reasons

## Acceptance

1. Compare consumed bytes, fields/defaults and reader identities with extracted,
   hash-pinned CObject::Read/FixEffectRenderType functions
2. Exercise all supported inspection profiles, identity mismatches, empty/long
   sound names, truncations and unknown layouts in asset-free contracts
3. Verify production rejection/recovery without replacing live stock/XL state;
   parser success never bypasses capability or resource checks
4. Reinspect hash-matched original candidates and refine feature/blocker evidence
   without promoting missions. Keep report generation canonical and repeatable
5. Run focused regressions, formatting and appropriate metadata integration;
   publish a separate PR and wait for green CI

Cosmetic deferral does not excuse missing sound cues, lightning interactions,
waypoint behavior or script activation. Those requirements remain visible until
their production behavior and lifecycle are implemented and verified.

## Implemented evidence

75 synthetic profiles and 1,980 truncations pass. The pinned reader matches all
profile fields/identities and 7,132 object records across 13 completely decoded
original levels; skipped original members remain unverified. Production native
rejection/recovery and the normal Android JNI/dialog check pass. Five of seven
spawn-bot and seven of eleven equipment candidates now decode completely, with
zero runtime promotions. See [the detailed evidence](../../XL_compat/evidence/effect-layouts.md).

The reference contract, asset-free payload contract and real-loader gate are
required by the private receipt. Public CI also discovers the asset-free test.
Publish as a separate PR based on xl_more_10 and require green latest-head CI.
