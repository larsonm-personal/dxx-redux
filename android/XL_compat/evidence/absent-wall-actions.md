# XL wall-type actions with absent targets

Sphere 1.6's original sp-02 object trigger 2 is type 9 (open wall), with targets
3419:3, 3420:3, 3888:1 and 2887:1. A missing wall is not an instruction to create
one or abandon the remaining links.

Reference: arbruijn/d2x-xl revision 2576a98b5de310e3ac7c9777df2677d1627b8bae,
the relevant consumer is `main/trigger.cpp`,
`CTrigger::DoChangeWalls` (SHA-256 of file:
92c0bef977a49c0cd6610ce49682df18820a933e70986be5276250bb3a0edece).
Its open/close/illusory-wall dispatcher checks `pSeg->Wall(nSide)` and continues
when absent, before changing wall state. Other linked walls are still processed.

The native dispatcher now preserves that behavior only for an active XL level.
Classic dispatch is unchanged. Admission permits sentinel -1 for action types
9, 10 and 11, in addition to the existing open-door no-op. It still rejects
malformed indices and coordinates; unaudited missing-wall actions remain blocked.

Controls cover absent-only links, an absent link preceding a valid forcefield
pair, all three wall-type actions, malformed indices and coordinates, and other
unsupported action types. The native controls use the existing derived Neptune
fixture; original input bytes remain unchanged.

This is not a Sphere readiness pass. Original sp-02 lightning owners 428 and 429
retain their separate unsupported cone-angle/style profile. No selected readiness
case or success criterion changes. Dinter version 21 and the OOF SPCL/RANI cases
are outside this change.

## Validation

- All 19 standalone XL CTests passed
- New absent-wall admission control fails against the pre-fix inventory library
- Native trigger flag controls passed against the unchanged Neptune HOG
- Exact original Sphere level 2 still exits at level_load with both lightning
  blockers; its missing-wall blocker is removed, without changing original bytes
- No readiness cohort or criterion changes
