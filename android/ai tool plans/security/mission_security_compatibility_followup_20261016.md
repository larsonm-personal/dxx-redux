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
- Verify live custom-definition loading for representative mission archives; update
  checked-in regression JSON only after each compatibility rule passes its real-file controls

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

## Remaining compatibility work after a0b4de51 rerun

- Treat the shipped D2 demo HAM as authoritative legacy input. Permit malformed
  fields in unused robot slots when no runtime reference can reach them, while
  retaining strict validation for every reachable robot, model, joint, bitmap,
  weapon, and payload range
- Accept Omicron's `pyromax.HXM` legacy zero suffix through a bounded structural
  rule derived from the actual file and loader behavior, not a mission-name or
  hash exception
- Extend `data_warnings` beyond lighting so every ignored or normalized legacy
  construct is visible in native metadata, the Android browser, and projected
  regression JSON. Include inert matcens, rejected companion data, and accepted
  legacy normalization where the engine changes or ignores serialized values
- Add exact real-file regression controls for the public demo HAM and Omicron
  HXM, plus damaged variants proving reachable invalid references and unbounded
  trailing data still reject
- Revisit the six pre-security admission failures only after the real serialized
  structures and historical runtime behavior are captured. Prefer bounded
  reconstruction or inert records over canonical-form rejection, truncation, or
  mission-specific exceptions

## Remaining compatibility completion results

- The pinned public `d2demo.ham` loads. Its 46 `model_num == -1` robot records are
  treated as inert slots; populated robot records remain strictly validated and
  runtime creation, paging, demo, and drop paths guard unusable definitions. Demo
  level 1 loads with 225 segments, 38 robots, and an empty `data_warnings` array
- Omicron level 7 accepts the structurally valid 564-byte zero HXM suffix and
  restores its custom robots, weapons, models, and textures. Nonzero trailing
  data and malformed indices still reject
- Legacy trigger counts are repaired without reading outside the ten serialized
  link slots. Negative counts become empty, oversized counts are bounded, and
  valid segment/side targets are compacted in original order while impossible
  targets are ignored. Native D1 metadata now publishes the same warning field
  as D2
- The wall table supports 255 entries, indices 0 through 254, while serialized
  byte value 255 remains the no-wall sentinel. Plutonia level 13 therefore keeps
  all 255 authored wall records instead of truncating or rejecting one
- Targeted native regeneration passes Descend Again, Orion, KCXF2, and Plutonia:
  all six formerly blocked levels load and the focused routes are `ok`. Their
  checked regression JSON records bounded counts and ignored invalid targets;
  ordinary levels carry empty warning arrays
- Both D1 and D2 upstream compatibility integration suites pass, as do the
  shared level-section repair test, real public demo HAM validation, and the
  focused four-archive regeneration

## Full-corpus follow-up results

- A non-writing pass over every available archive found one additional
  supported-format admission regression outside the earlier 84-archive set:
  PPG Hall retained dynamic-light source ranges after its editor filled the
  serialized 10,000-target table. The loader now preserves every available
  target, truncates only the missing suffix, and reports `Bounded 8 dynamic-light
ranges; ignored 231 missing targets`. The original level loads with 896
  segments and its route confirms
- The same pass completed 135 of 138 archive jobs directly. The remaining three
  RAR jobs were tool-discovery failures and all passed when rerun with the same
  native workers and an explicitly supplied repository-local extractor
- The public D2 demo still loads level 1 after final formatting and rebuild with
  225 segments, 38 robots, and an empty `data_warnings` array
- Mission descriptors with multiple validated secret origins retain the original
  engine's first-origin behavior and now report the ignored extra values. KKR's
  checked metadata records `Used first secret origin; ignored 1 extra origin`;
  malformed and out-of-range origins remain rejected
- Final D1 and D2 production builds pass. The D1 and D2 upstream compatibility
  suites, shared level-section tests, descriptor tests, D2 HAM tests, and paired
  real mission-loader secret-origin fixtures all pass
- The reported odd-width IFF sanitizer issue is covered by the shared bounded
  decoder introduced with the security work. AddressSanitizer passes raw and
  RLE odd-width PBM rows, a nine-pixel planar ILBM row, and an odd-width ANIM
  delta whose encoded pad byte is clipped rather than written past the visible
  image. The older decoder's padded-row overwrite is no longer on a public read
  path
- The larger corpus also contains explicit D2X-XL level versions 11 through 23.
  These are not ordinary D2 files with a bumped header: their serialized segment
  owners/groups, widened wall indices, extended segment records, colored-light
  tables, equipment generators, and version-dependent trigger semantics differ.
  Do not bypass the version check and misparse them. Supporting those missions
  is a separate format implementation, with ignored unsupported XL features
  reported through `data_warnings`
- A final non-writing audit completed all 138 archive jobs: 696 missions and
  2,639 advertised levels. All 2,549 present classic D1/D2-format levels load.
  The remaining 90 entries are 59 explicit D2X-XL-format levels and 31 level
  files absent from their archives; there are no remaining classic-format
  admission failures in the available corpus
- First Strike and Counterstrike each load all 30 levels with empty warning
  arrays. Comparing successful metadata with the previous full pass while
  excluding the newly added warning field found only the intended PPG Hall
  compatibility change; three other apparent differences were previously
  unavailable RAR extractions
- Fresh D1 and D2 AddressSanitizer builds both pass the bounded IFF/ABM decoder
  suite, including the odd-width compatibility cases
- The final upstream compatibility gate exposed a separate legacy IFF writer
  overread: PBM rows were encoded at the even on-disk width directly from a
  tightly packed odd-width bitmap, and the RLE encoder could also read one byte
  past a run ending at the input boundary. D1 and D2 now synthesize the required
  zero pad byte, bound both RLE lookaheads, and round-trip odd widths 3, 65, and
  127 under AddressSanitizer. Both complete upstream compatibility suites pass
