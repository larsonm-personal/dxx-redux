# Compact matcen level metadata

- Keep native capture evidence and gameplay unchanged
- Persist only factory definitions, effective bindings and matcen trigger links
- Deduplicate effective records/model metadata and preserve invalid/unbound states
- Omit only complete loaded levels with no relevant records or triggers
- Format native and Android output as one readable semantic row per line
- Mechanically project existing historical data; prove retained fields agree without executing or rerecording gameplay
- Test comparisons, negative controls, output round trips and native/JVM build paths
- Review code and data diffs separately; publish a draft PR without merging

## Verification

- Independent reconstruction matched all 207 bindings in 81 original blocks; 15 empty blocks omitted
- Eleven historical captures shrink from 4,544 to 317 lines; all seven stored files shrink from 8,226 to 2,002 matcen lines
- Original capture hashes remain unchanged; compact projection hashes are recorded separately
- All 53 Python and 14 Kotlin tests pass; eleven frozen birth traces retain their original pass/review outcomes
- D1 and D2 metadata exporter syntax checks pass with existing native build dependencies
- Native synthetic checks preserve rejected/incomplete, invalid-segment, physical-out-of-bounds and logical-unbound states
- Native/Kotlin formatting agrees on a representative document; seven migrated files are byte-idempotent through Kotlin formatting
- Scoped quality wrapper could not load dependency tooling from the sparse checkout; direct pinned clang-format and Ruff passed
- Full hosted build and integration checks remain pending at publication
