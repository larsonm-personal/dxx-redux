# GQR-0268: canonical decoding for loose and disc descriptors

Diagnosis and implementation plan only. Proposed GQF-0282 is current-generation REGROWTH linked to archived BR-0098; its earlier archive/stream/extracted/detail repair remains FIXED. No product or test edit, build, probe or execution is authorized in this tranche.

Problem: FileSetContentCatalog.parseDescriptor at current215 and SetupDiscImport.prepareDiscContent at294 call file.readText() before the shared parser. Invalid UTF-8 Windows-1252 bytes are replaced before MissionDescriptorPolicy.decode can apply the supported fallback. A descriptor with an accented title/author/reference is therefore inventoried differently from the same bytes read through MissionZip.parseMissionDescriptor(path, bytes); corrupted names can be persisted in owned entry metadata and dependency/retention decisions use a different decoded identity. Catalog is absent at GQ2 base and present with this call at frozen207; disc staging has the bypass at frozen304. Current source call graph is sufficient diagnosis; no fresh Kotlin/runtime reproduction claimed.

- [x] Source control/data flow, branch attribution and historical root deduplication
- [ ] Implement both byte-reader callsites through the existing byte-based parser/decoder
- [ ] Extend actual catalog and prepareDiscContent controls for identical UTF-8/Windows-1252 metadata and dependency identities
- [ ] Validate focused JVM contracts, scoped quality and Android Kotlin compilation

Allowed product paths: android/app/src/main/java/com/dxxredux/app/FileSetContentCatalog.kt and SetupDiscImport.kt. Allowed tests: FileSetContentCatalogTest.kt and the maintained disc-content staging fixture; discover its actual owner before editing. Reuse MissionZip.parseMissionDescriptor(path, bytes) or MissionDescriptorPolicy.decode; do not add another encoding heuristic, migration, native parser or format table. Preserve existing canonical containment, component ownership, exact movie companions, isolated staging, runtime/companion policy and checked deletion.

Acceptance: identical descriptor bytes yield the same title, author, level and referenced-asset identity through loose catalog, staged disc and existing archive/stream/extracted paths. Include accented title/author/reference, malformed UTF-8 requiring fallback, ordinary ASCII and valid multibyte UTF-8; assert no replacement-character publication or false dependency split. Drive actual prepareDiscContent on an isolated staging directory and verify retained referenced bytes plus unchanged supported runtime files and base/player separation. Keep file read/storage failure semantics truthful. Coordinate GQR-0102/0212 and BR-0472/0506 source-generation, bounds and publication plans without merging roots.

Priority: 44 MEDIUM (H/M/B/C/R=23/0/4/10/7). Material metadata/ownership correctness, zero original-file merge payoff, focused workflow, high source confidence, clear local repair with actual staging/platform validation. No inherited savings, canonical finding/status mutation or fresh test acceptance yet.
