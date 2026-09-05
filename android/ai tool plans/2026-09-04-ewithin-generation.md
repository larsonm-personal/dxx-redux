# Enemy Within combo archive generation

- [x] Trace missing nested archive extraction in host level metadata and guidebot simulation staging
- [x] Expose shared Kotlin archive variant selection to both host generators
- [x] Add regression coverage for preferred and ambiguous combo archives
- [x] Generate Enemy Within level metadata and guidebot simulations and verify outputs
- [x] Run scoped formatting and relevant checks

Both host runners now select the nested Rebirth ZIP through `selectPreferredMissionVariant` and `missionVariantForArchiveFilename` in the shared Kotlin policy before extracting payloads

Validation:
- CLI build and shared Kotlin policy tests passed
- `test_mission_archive_variants.ps1` exercises both actual extraction functions, ambiguous/unsupported variants, DOS fallback, and direct descriptor archives
- Level metadata generated for one mission with 32 levels
- All 32 guidebot runs produced results: 21 ok, 11 simulation timeouts, no infrastructure failures
- Scoped code quality passed

Only script/Kotlin code changed; generation used existing host native executables with `-NoBuild`
