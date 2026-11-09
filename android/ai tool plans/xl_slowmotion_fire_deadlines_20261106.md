# XL primary and flare deadlines

Continue #395 in an isolated draft. Pickup admission remains disabled.

1. Compare pinned controlweapon.cpp primary and flare timing to the native call sites.
2. Scale the interval from the last primary shot using the world clock, preserving
   the reference strict two-second sanity boundary. Scale flare scheduling with
   its distinct integer conversion order and unchanged two-second check.
3. Keep classic/network paths unchanged. Adapt reference fixed-point intervals to
   Redux's 64-bit epoch without converting absolute timestamps to float. Reject
   undefined interval conversions/overflow. Exercise actual firing entrypoints,
   source comparisons, boundaries, large epochs, isolation and malformed receipts.
4. Validate native and classic regressions, source oracle, public owner and quality;
   publish a separate draft on #395 only after validation.

## State scope

Primary last/next and flare next timestamps already participate in native relative
save state. This stage introduces no new state or save schema. Active slow-motion
clock persistence remains unimplemented; existing reset behavior is unchanged.
Missile last-shot state and Omega's per-simulation-frame eligibility must be added
with their save/reset semantics in a subsequent coordinated change. Neither is
claimed by this patch. Door/object/wall effect timing, controls, audio, authentic
pickup rendering and complete persistence remain outstanding.

## Validation and limits

Pinned revision: 2576a98b5de310e3ac7c9777df2677d1627b8bae,
`weapons/controlweapon.cpp` primary/flare decision fragments. Exact CRLF source
SHA-256: 1caa0d8a33f755260693a6083ac973593bf0533f2063815de72e66504d75d4b6.
The comparison uses a zero relative epoch, then requires identical native outcomes
at epoch 2^40. This intentionally avoids inheriting 32-bit absolute-float timestamp
precision loss; it is not a claim of overflow-domain parity with upstream.

- 1,024 mandatory native/source primary and flare cases across world profiles
  1, 1.025 and 3 (including bullet-time console speed 1), multiplayer, low energy,
  signed last-shot offsets, strict boundaries and two epochs passed.
- Eight malformed/duplicate/omitted/result-corrupted transcript controls rejected.
- Existing source comparisons and new deadline cases passed ASan/UBSan; this is
  the source oracle, not a sanitizer build of the full native engine.
- Native water actors, cloaks, projectiles and lava regressions passed. The water
  owner verifies disk preservation of the three existing firing timestamp offsets,
  dead-player blocking, invalid interval rejection, RNG isolation and stock reset.
- All 55 public XL contracts and 33 private-assets policy tests passed.
- Scoped mixed-language quality and automation catalog validation passed.
  Catalog execution remains blocked by the existing sandbox child-supervisor
  `/proc/self/task/.../children` limitation (child exit 125, wrapper exit 1).
- Derived native fixtures use unchanged Neptune HOG SHA-256
  4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303.
  No new original campaign readiness or rendered pass is claimed.

For the next state slice: reference `COmegaData::COmegaData` zero-initializes
`nLastFireFrame`; `weapons/omega.cpp:202` blocks current and previous simulation
frames, and line 245 writes the marker on an accepted local shot. This is not the
native 50 ms `d_tick_count`. Guided missile release in `fireweapon.cpp:898` updates
next fire time without updating last-shot time; the ordinary firing path sets it
at line 917. Persisting these requires an explicitly validated XL state extension
without changing classic save layout. Wall/object effects use different decrements
in `main/effects.cpp`; they must not receive a uniform time-scale substitution.
