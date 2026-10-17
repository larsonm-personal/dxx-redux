# Mission security compatibility follow-up

## Scope

- Preserve bounded legacy HXM and extra-HAM trailers without admitting arbitrary trailing data
- Preserve the original engine behavior for negative serialized delta-light counts by normalizing them to empty ranges before graph validation
- Normalize unused offsets on zero-count robot joint lists before runtime pointer formation while retaining all nonempty-list bounds checks
- Accept legacy levels with unusable delta-light records while preventing every
  invalid source or target reference from reaching a segment-array access
- Report compact per-level compatibility warnings in native metadata, the Android
  metadata browser, and projected regression JSON; ordinary levels report an
  empty warning list
- Verify live custom-definition loading for representative mission archives; do not update checked-in regression baselines

## Validation

- Extend focused native validation tests for trailer shape, length, and bounds
- Add focused coverage for signed-count normalization and retained range rejection
- Add validator and live-loader coverage for zero-count joint offsets and nonempty negative/out-of-range controls
- Rebuild D2 and run the relevant native tests
- Run non-writing native metadata checks against the affected mission archives

## Deferred families

- Matcen compatibility reconstructs the original runtime-selected mapping from the
  generated ordinal, the loaded record's station link, and the station/segment
  backlinks. The redundant serialized `segnum` is not authoritative because the
  original runtime did not use it. Do not compact or renumber around placeholders;
  leave ordinals without disk records inert and verify later centers retain their
  legacy identities
- Explicit D2X-XL descriptor support remains separate from legacy descriptor compatibility
- Invalid delta-light source groups are made inert because no valid runtime side
  can activate them. Invalid targets are skipped individually so valid targets in
  the same source group retain their original lighting behavior. Preserve the raw
  records for save/editor round-tripping and guard the runtime access as defense
  in depth

## Delta-light compatibility validation

- Cover orphan sources, mixed valid/invalid targets, malformed source ranges,
  diagnostic reset between loads, and safe runtime application
- Emit the same `data_warnings` array from host and JNI native metadata paths
- Preserve `data_warnings`, including an empty array, in regression projection
- Parse and display warnings in level summary/detail UI without treating a safely
  repaired level as failed
- Run focused native and Kotlin tests, Windows D2 build, and non-writing metadata
  checks for DD2LVLS1, Entropy v1.0, EAF2, descent.fan D2 conversion, Phenomia,
  and Prophecy

## Delta-light completion results

- D1/D2 Windows builds and the D2 upstream compatibility integration suite pass
- Android metadata parsing and host regression projection tests pass
- Targeted host regeneration passes all six archives and all 80 levels
- Seven levels carry compact warnings; the other 73 carry empty warning arrays
- Checked-in JSON was regenerated only for the six targeted archives

## Matcen compatibility validation

- Cover stale disk backlinks, exact ordinal-to-station resolution, invalid station
  links, and out-of-range placeholder ordinals in the native D1 and D2 fixture
- Keep valid public matcen production byte-for-byte deterministic
- Run non-writing metadata generation for Orion, ORION-D2, Galmond2, and Vignettes
  and confirm all previously blocked levels load without changing checked-in data
