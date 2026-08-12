# Finish review sweeps and fix format findings

## Plan

- [x] Re-read repository and adversarial-review instructions and inventory the remaining queue
- [x] Complete R1-SWEEP-006 through R1-SWEEP-014 sequentially with sol-5.6-medium
- [x] Validate the completed sweep tranche, ledger integrity, and absence of ACTIVE rows
- [x] Rank up to 40 open non-network findings by file-format and import correctness relevance
- [x] Fix ranked findings sequentially with focused regression coverage and proportionate builds (34 complete)
- [x] Move each completely fixed finding to the done ledger and record exact validation evidence (34 archived)
- [x] Run final ledger, quality, test, build, and diff validation
- [x] Finish BR-0096 by validating descriptor-referenced mission assets before admission
- [x] Add focused mission-package parsing tests and run proportionate validation
- [x] Archive BR-0096 and re-run ledger and diff integrity checks
- [x] Migrate BR-0091 mission and custom music-name sidecars to one bounded versioned schema
- [x] Add producer-to-native collision, boundary, Unicode, and malformed-input tests
- [x] Validate, archive BR-0091, and re-run ledger integrity checks
- [x] Fix BR-0094 by representing unavailable archive compressed sizes explicitly
- [x] Add ZIP, 7z, extracted-archive, and details-rendering regression coverage
- [x] Validate, archive BR-0094, and re-run ledger integrity checks
- [x] Fix BR-0097 by canonicalizing multi-mission set ordering and primary identity
- [x] Add reversed ZIP, 7z, and extracted-record regression coverage
- [x] Validate, archive BR-0097, and re-run ledger integrity checks
- [x] Fix BR-0098 with one bounded legacy-compatible descriptor decoder
- [x] Add UTF-8, Windows-1252, malformed, and backend-parity regression coverage
- [x] Validate, archive BR-0098, and re-run ledger integrity checks
- [x] Fix BR-0095 with deterministic repository/configured RAR fixture resolution
- [x] Add explicit fixture-present execution and format-independent failure coverage
- [x] Validate, archive BR-0095, and re-run ledger integrity checks
- [x] Audit duplicated native test stubs against authoritative engine headers
- [x] Remove unnecessary stubs or document the smallest unavoidable test seam
- [x] Rebuild affected native tests and run diff validation
- [ ] Fix BR-0438 by validating a complete combined configuration before publication
- [x] Fix BR-0428 by rejecting non-finite touch-layout numbers before persistence
- [x] Fix BR-0299 by rejecting input-demo levels outside the loaded mission
- [x] Fix BR-0307 by validating polygon counts before fixed-array writes
- [x] Fix BR-0635 by validating complete WAV chunk spans before advancement
- [x] Fix BR-0452 by validating the complete BinHex envelope before publication
- [x] Fix BR-0483 without rewriting quoted JSON5 payloads
- [ ] Fix BR-0493 by validating launcher patches against the native schema
- [x] Fix BR-0354 by validating weapon runtime scalars before save publication
- [x] Fix BR-0379 by enforcing the accepted D1 checkpoint version

## Current status

Eight of the nine requested remaining parser and format fixes are complete and archived: BR-0428, BR-0299, BR-0307, BR-0635, BR-0452, BR-0483, BR-0354, and BR-0379. Focused JVM, Python, PowerShell, and native fixture tests pass, as do the full Android JVM suite, paired Windows D1/D2 builds, and the Android debug build. BR-0493 now rejects the native-incompatible verbs, missing values, malformed paths, unsupported sections and fields, static index ceilings, row shapes, scalar types, and simple ranges, but remains open until launcher validation shares the native consumer's live table counts and complete semantic ranges. BR-0438's semantic preflight remains complete, while cross-file and SharedPreferences publication still needs one staged transaction before archival. Network and deferred architectural findings remain out of scope.

The native rewind test no longer carries duplicate `vecmat.h`, `pstypes.h`, `physfs.h`, or `byteswap.h` stubs. Its focused core-only compile excludes unexercised engine serialization helpers, while CMake supplies the pinned PhysFS 3.2.0 public header and normal D1/D2 builds continue to use their authoritative engine headers. The focused MSVC build, test, scoped quality checks, and diff validation passed.

Continued remediation completed and archived BR-0095 by passing the repository root into JVM tests, supporting an explicit RAR fixture override, failing when an override is missing, and executing the local Reetus import with zero skips. Committed archive and extraction-store fixtures continue to cover format-independent failure behavior. Forty ranked format findings are now fixed and archived.

Continued remediation completed and archived BR-0098 by routing every mission-descriptor source through one bounded strict-UTF-8 decoder with a Windows-1252 fallback. Truncated text views now discard only an incomplete final UTF-8 code point rather than reinterpreting the valid prefix. Thirty-nine ranked format findings are now fixed and archived.

Continued remediation completed and archived BR-0097 by canonicalizing valid mission sets by case-insensitive descriptor path with an exact-path tie break before selecting the package identity. Reversed ZIP, 7z, and extracted-record projections now produce the same primary title, game, and mission ordering. Thirty-eight ranked format findings are now fixed and archived.

Continued remediation completed and archived BR-0094 by replacing fabricated compressed byte counts with an explicit unknown state. ZIP retains defined compressed sizes, while 7z and extracted RAR entries report `Unknown`; extraction limits continue to enforce actual materialized bytes. Thirty-seven ranked format findings were fixed and archived at this point.

Continued remediation completed and archived BR-0091 with one bounded versioned record contract shared by mission and custom-audio producers and the native consumer. Thirty-six ranked format findings were fixed and archived at this point.

Continued remediation completed and archived BR-0096 by validating descriptor-referenced assets before mission-package admission. Thirty-five ranked format findings are now fixed and archived.

All nine remaining review sweeps are complete. Forty ranked format findings are fixed and archived, including every item in the rescanned parser-first queue. Their focused regression tests, proportionate native or JVM builds, Android debug builds for maintained ABIs, scoped quality checks, and diff validation passed. BR-0082 remains open because complete body admission needs new scope-aware metadata integrity or a side-effect-free staged engine reader. BR-0096 now validates same-stem HOG/DXA structure and requires validated archive catalogs plus directory-local loose files to cover every declared level. BR-0095 now executes a repository-root or explicitly configured RAR fixture and fails closed for a missing override. BR-0098 now applies one strict UTF-8 and Windows-1252 fallback policy to every mission-descriptor consumer and bounded text view. BR-0097 now canonicalizes multi-mission identity and ordering across archive backends. BR-0091 now uses one bounded versioned sidecar schema with exact-path priority, ambiguity-aware aliases, and strict complete native decoding. BR-0103 remains open because a complete fix requires immutable generations, a process-wide transaction owner, fsynced candidates, an atomic manifest-pointer switch, and restart recovery rather than a partial lock-only patch. BR-0219 now enforces shared checkpoint, encoded-payload, expansion-ratio, and whole-demo limits before large replay allocations and applies the checkpoint ceiling to recording. BR-0218 now ignores recorded storage names and restores through collision-checked, replay-owned temporary files. BR-0208 now publishes rewind slots only after complete serialization, preserves full history on capture failure, resets smaller rewrites to their exact size, and emits mandatory blank thumbnails without allocation. BR-0320 now bounds native mounted texture indexing and rejects unsafe DXA structure before enablement. BR-0389 now validates classic-demo wall counts and complete record spans before decoding. BR-0281 now bounds and exactly reads custom PCM, MIDI, and converted HMP inputs and preflights decoded PCM and expansion budgets. BR-0237 now catalogs every MIDI and HMP entry in one validated pass and reports incomplete HOG scans explicitly. BR-0249 now preserves U8 and signed-16 SDL callback formats in OpenSL and rejects unsupported audio specs. BR-0531 now bounds ETC2 source dimensions, uses checked layout arithmetic throughout mip construction, and transactionally publishes complete KTX output. The rescanned parser-first queue is complete.

## Remediation queue

This is a priority queue, not a promise to broaden a fix when live-code investigation shows shared ownership or a larger design dependency. Complete fixes are archived individually; findings that need broad redesign remain open with the blocker recorded.

1. BR-0372, DXA robot weapon indices
2. BR-0217, replay weapon-order permutations
3. BR-0220, replay weapon-selector domains
4. BR-0221, replay frame-time invariants
5. BR-0291, replay difficulty domain
6. BR-0383, restored thief inventory indices
7. BR-0390, restored scheduler ticks
8. BR-0261, pilot weapon-order permutations
9. BR-0267, pilot cockpit mode
10. BR-0321, replacement image decode size
11. BR-0333, level control-center links
12. BR-0014, excess CUE FILE directives
13. BR-0183, CUE MSF field ranges
14. BR-0184, CUE data-track sector modes
15. BR-0093, ZIP preamble validation
16. BR-0045, legacy save-header validation
17. BR-0082, metadata-backed save-body validation
18. BR-0096, mission descriptor-to-asset admission
19. BR-0039, fingerprint database entry admission
20. BR-0040, fingerprint duration matching
21. BR-0069, PKG scan and extraction catalog parity
22. BR-0072, deterministic SOW scan manifests
23. BR-0091, music-name sidecar bounds
24. BR-0092, optional mission music inspection
25. BR-0102, extraction-record source identity
26. BR-0103, extraction-tree transactional publication
27. BR-0104, engine-compatible mission staging paths
28. BR-0185, path-qualified song references
29. BR-0237, complete HOG MIDI/HMP enumeration
30. BR-0238, unique mission MIDI classification
31. BR-0235, save metadata append failures
32. BR-0211, cooperative progress inventory publication
33. BR-0212, cooperative save metadata discovery
34. BR-0208, rewind snapshot publication
35. BR-0218, embedded checkpoint temporary ownership
36. BR-0219, embedded checkpoint expansion bounds
37. BR-0207, rewind mission identity
38. BR-0222, replay artifact-set publication
39. BR-0233, verified headless output publication
40. BR-0320, bounded texture-index construction
