# Chunk0503 matcen CTest registration candidate, 2026-10-16

Diagnosis and future implementation only, serially under plan_general_diagnosis_resume_20261009.md

Proposed GQF0322/GQR0308, P3/high, test-gap/regression-registration. IDs provisional until canonical publication. Score36 MEDIUM, H/M/B/C/R12/0/4/10/10: focused host test discovery, no inherited reduction, deterministic source/CTest oracle. Do not add this score to other owners or replace existing higher56 reference

## Concrete problem

android/tests/CMakeLists.txt206-207 creates test_matcen_mode and links the real shared matcen_mode.c. The complete DXX_HOST_TEST_TARGETS list498-549 and conditional registration550-566 omit the target, and no direct add_test names it. Tracked exact-name search found only its executable/include declarations. Default native CTest therefore has no entry that executes this standalone66-line release-active policy oracle. This is a source registration omission; no configure/build/CTest or seeded failure was executed during diagnosis

The test evaluates expectations without NDEBUG-sensitive assert and covers default/one-round/paused behavior, rejected invalid mode, count save/restore and level-reset preserving mode while clearing counts. Retain those assertions. Maintained actual matcen_stations fixture exercises mixed station shutdown, set/restore and packet/file-backed paths; gameplay-options fixture exercises shared packet serialization with captured transport. These useful fixtures do not prove all standalone assertions redundant. Do not replace integration coverage with the unit fixture or remove the unit fixture merely because other tests use the same policy

## Existing-owner census

Archived BR0024FIXED is repaired SOW extraction coverage. GQF0068/GQR0055 owns audio enumeration; GQF0158/GQR0145 owns bounded-extractor Python aggregate acceptance; GQF0165/GQR0152 owns the WAV parser regression; GQF0300/GQR0286 owns the black-output integration observer. These specific scopes do not register this target. Completed GQF0253/GQR0239 actual station fix and GQF0257/GQR0243 shared packet extraction remain FIXED/DONE, with historical validation preserved. No generic discovery framework or reopened historical issue

## Minimal implementation boundary

- Add test_matcen_mode to the existing DXX_HOST_TEST_TARGETS list beside related portable policy targets, using the existing registration loop, working directory and game-specific build ownership
- Retain the existing executable, real shared source, all assertions and other targets. No new PowerShell wrapper, parser, duplicated policy or inherited d1/d2 source change
- Verify actual configured D1 and D2 CTest inventories contain exactly one selected entry, build that target in the supported host configuration and run the exact selected test
- Prove a deliberate policy expectation failure reaches a nonzero CTest result in optimized/NDEBUG configuration using an isolated temporary copy, then restore and require the ordinary target to pass. Do not change production semantics to satisfy the test
- Retain actual station/packet/save integration registration and source identities. Run scoped CMake quality and diff checks during the separately authorized implementation tranche

No code/test/helper edits or execution, configure/build/formatter/device/network/probe, canonical admission/status/rating mutation, staging or commit in this plan. All assigned paths absent1996original, inherited saving0. Chunk0503 analysis/publication and broader diagnosis goal remain incomplete
