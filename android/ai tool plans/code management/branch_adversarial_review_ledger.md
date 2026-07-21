# Adversarial Branch Review Ledger

This is the only canonical file for review progress, findings, dispositions, and closure

## Frozen campaign snapshot

- Generated: 2026-07-19 07:25:34 UTC
- Campaign ID: `R1`
- Target ref at generation: `upstream/main` -> `6e76c5d8dafd02dc32a1c6312ce9440dc95b1aca`
- Review base: `fb555eec75e1ed12c8348805ab335afb4c721b06`
- Review head: `c01d8fe4686c63d931b1e543a6305bbafaa944a9`
- Diff command: `git diff fb555eec75e1ed12c8348805ab335afb4c721b06 c01d8fe4686c63d931b1e543a6305bbafaa944a9`
- Changed paths: 2638
- Diff lines: +672291/-4047
- Generated review chunks: 599
- Process: `android/ai tool plans/code management/branch_adversarial_review_process.md`

Review the frozen commits even if the live branch moves. `R1-CLOSE-001` must account for every later commit before the campaign can finish

## Status legend

- `[ ] TODO`: not reviewed
- `[-] ACTIVE`: claimed by the current call
- `[x] DONE`: reviewed and recorded, including explicit no-finding results
- `[x] SKIP`: disposition recorded with a concrete reason and substitute validation
- `[!] BLOCKED`: cannot be completed without named evidence or authority

Only one call may edit this file at a time. Replace the state in place and put finding IDs or `none` in the Result column

## Inventory summary

| Kind | Paths | Added | Deleted |
|---|---:|---:|---:|
| artifact | 22 | 56411 | 0 |
| authored-config | 129 | 7839 | 2 |
| authored-source | 741 | 213468 | 3889 |
| build-script | 179 | 30973 | 152 |
| dependency-lock | 1 | 2910 | 0 |
| documentation | 26 | 2182 | 4 |
| generated-fixture | 120 | 218262 | 0 |
| historical-plan | 1122 | 88521 | 0 |
| other-data | 21 | 244 | 0 |
| test-source | 277 | 51481 | 0 |

## Review queue

| ID | State | Phase | Risk | Kind | Path | Assigned scope | Result |
|---|---|---|---|---|---|---|---|
| R1-PREFLIGHT-001 | [x] DONE | preflight | critical | pr-scope | `complete diff` | PR composition, provenance, generated artifacts, secrets, licenses, and split strategy | BR-0001, BR-0002, BR-0003, INV-0001 |
| R1-PREFLIGHT-002 | [x] DONE | preflight | high | architecture | `complete diff` | Subsystem map, intended behavior, ownership boundaries, and highest-risk data flows | BR-0004, BR-0005 |
| R1-PREFLIGHT-003 | [x] DONE | preflight | high | change-history | `complete diff` | Commit clusters, superseded approaches, partial migrations, and abandoned compatibility paths | BR-0006 |
| R1-CHUNK-0001 | [x] DONE | source | critical | authored-config | `2 related paths` | 45 review lines under android | BR-0007 |
| R1-CHUNK-0002 | [x] DONE | source | critical | authored-config | `14 related paths` | 566 review lines under game_data/CD images | BR-0008, BR-0009, BR-0010 |
| R1-CHUNK-0003 | [x] DONE | source | critical | authored-config | `7 related paths` | 485 review lines under game_data/CD images | none |
| R1-CHUNK-0004 | [x] DONE | source | critical | authored-config | `8 related paths` | 516 review lines under game_data/CD images | BR-0011 |
| R1-CHUNK-0005 | [x] DONE | source | critical | authored-config | `8 related paths` | 575 review lines under game_data/CD images | BR-0011, BR-0012 |
| R1-CHUNK-0006 | [x] DONE | source | critical | authored-config | `9 related paths` | 241 review lines under game_data/music | none |
| R1-CHUNK-0007 | [x] DONE | source | critical | authored-source | `2 related paths` | 260 review lines under android/app/src/main/cpp | BR-0013, BR-0014 |
| R1-CHUNK-0008 | [x] DONE | source | critical | authored-source | `2 related paths` | 570 review lines under android/app/src/main/cpp | BR-0015, BR-0016 |
| R1-CHUNK-0009 | [x] DONE | source | critical | authored-source | `2 related paths` | 470 review lines under android/app/src/main/cpp | BR-0017, BR-0018 |
| R1-CHUNK-0010 | [x] DONE | source | critical | authored-source | `2 related paths` | 586 review lines under android/app/src/main/cpp | BR-0018, BR-0019, BR-0020, BR-0021 |
| R1-CHUNK-0011 | [x] DONE | source | critical | authored-source | `2 related paths` | 242 review lines under android/app/src/main/cpp | BR-0018, BR-0020, BR-0021, BR-0022, BR-0023, BR-0024, BR-0025 |
| R1-CHUNK-0012 | [x] DONE | source | critical | authored-source | `2 related paths` | 202 review lines under android/app/src/main/cpp | BR-0026, BR-0027 |
| R1-CHUNK-0013 | [x] DONE | source | critical | authored-source | `2 related paths` | 334 review lines under android/app/src/main/cpp | BR-0016, BR-0028, BR-0029, BR-0030 |
| R1-CHUNK-0014 | [x] DONE | source | critical | authored-source | `2 related paths` | 378 review lines under android/app/src/main/cpp | BR-0018, BR-0031, BR-0032, BR-0033 |
| R1-CHUNK-0015 | [x] DONE | source | critical | authored-source | `2 related paths` | 550 review lines under android/app/src/main/cpp | BR-0018, BR-0020, BR-0021, BR-0034, BR-0035, BR-0036, BR-0037 |
| R1-CHUNK-0016 | [x] DONE | source | critical | authored-source | `3 related paths` | 450 review lines under android/app/src/main/cpp | BR-0038, BR-0039, BR-0040, BR-0041 |
| R1-CHUNK-0017 | [x] DONE | source | critical | authored-source | `3 related paths` | 440 review lines under android/app/src/main/cpp | BR-0020, BR-0024, BR-0042, BR-0043 |
| R1-CHUNK-0018 | [x] DONE | source | critical | authored-source | `4 related paths` | 523 review lines under android/app/src/main/cpp | BR-0044, BR-0045 |
| R1-CHUNK-0019 | [x] DONE | source | critical | authored-source | `android/app/src/main/cpp/extract/extract_cd.c` | L1-L600 | BR-0013, BR-0014, BR-0020, BR-0046, BR-0047 |
| R1-CHUNK-0020 | [ ] TODO | source | critical | authored-source | `android/app/src/main/cpp/extract/fingerprint_cd.c` | L1-L442 | - |
| R1-CHUNK-0021 | [ ] TODO | source | critical | authored-source | `android/app/src/main/cpp/extract/hfs_reader.c` | L1-L600 | - |
| R1-CHUNK-0022 | [ ] TODO | source | critical | authored-source | `android/app/src/main/cpp/extract/inno_reader.c` | L1-L600 | - |
| R1-CHUNK-0023 | [ ] TODO | source | critical | authored-source | `android/app/src/main/cpp/extract/inno_reader.c` | L601-L1200 | - |
| R1-CHUNK-0024 | [ ] TODO | source | critical | authored-source | `android/app/src/main/cpp/extract/inno_reader.c` | L1201-L1800 | - |
| R1-CHUNK-0025 | [ ] TODO | source | critical | authored-source | `android/app/src/main/cpp/extract/iso9660_reader.c` | L1-L556 | - |
| R1-CHUNK-0026 | [ ] TODO | source | critical | authored-source | `android/app/src/main/cpp/extract/iso9660_reader.h` | L1-L116 | - |
| R1-CHUNK-0027 | [ ] TODO | source | critical | authored-source | `android/app/src/main/cpp/extract/jni_disc_import.c` | L1-L547 | - |
| R1-CHUNK-0028 | [ ] TODO | source | critical | authored-source | `android/app/src/main/cpp/extract/mac_hfs_extract.h` | L1-L24 | - |
| R1-CHUNK-0029 | [ ] TODO | source | critical | authored-source | `android/app/src/main/cpp/extract/pkg_reader.c` | L1-L539 | - |
| R1-CHUNK-0030 | [ ] TODO | source | critical | authored-source | `android/app/src/main/cpp/extract/pkg_reader.h` | L1-L77 | - |
| R1-CHUNK-0031 | [ ] TODO | source | critical | authored-source | `android/app/src/main/cpp/extract/sow_extract.c` | L1-L600 | - |
| R1-CHUNK-0032 | [ ] TODO | source | critical | authored-source | `android/app/src/main/cpp/extract/sti2_extract.c` | L1-L600 | - |
| R1-CHUNK-0033 | [ ] TODO | source | critical | authored-source | `android/app/src/main/cpp/extract/sti2_extract.c` | L601-L1200 | - |
| R1-CHUNK-0034 | [ ] TODO | source | critical | authored-source | `android/app/src/main/cpp/extract/sti2_extract.c` | L1201-L1800 | - |
| R1-CHUNK-0035 | [ ] TODO | source | critical | authored-source | `android/app/src/main/cpp/jni_fingerprint.c` | L1-L273 | - |
| R1-CHUNK-0036 | [ ] TODO | source | critical | authored-source | `android/app/src/main/cpp/jni_level_metadata.cpp` | L1-L600 | - |
| R1-CHUNK-0037 | [ ] TODO | source | critical | authored-source | `android/app/src/main/cpp/jni_level_metadata.cpp` | L601-L1153 | - |
| R1-CHUNK-0038 | [ ] TODO | source | critical | authored-source | `android/app/src/main/cpp/jni_main.c` | L1-L600 | - |
| R1-CHUNK-0039 | [ ] TODO | source | critical | authored-source | `android/app/src/main/cpp/jni_main.c` | L601-L1200 | - |
| R1-CHUNK-0040 | [ ] TODO | source | critical | authored-source | `android/app/src/main/cpp/jni_music_control.c` | L1-L415 | - |
| R1-CHUNK-0041 | [ ] TODO | source | critical | authored-source | `android/app/src/main/cpp/jni_resume_save.cpp` | L1-L600 | - |
| R1-CHUNK-0042 | [ ] TODO | source | critical | authored-source | `android/app/src/main/cpp/shared/physfs_archiver_saf.c` | L1-L600 | - |
| R1-CHUNK-0043 | [ ] TODO | source | critical | authored-source | `android/app/src/main/cpp/shared/physfs_archiver_saf.c` | L601-L653 | - |
| R1-CHUNK-0044 | [ ] TODO | source | critical | authored-source | `2 related paths` | 538 review lines under android/app/src/main/java | - |
| R1-CHUNK-0045 | [ ] TODO | source | critical | authored-source | `2 related paths` | 522 review lines under android/app/src/main/java | - |
| R1-CHUNK-0046 | [ ] TODO | source | critical | authored-source | `2 related paths` | 256 review lines under android/app/src/main/java | - |
| R1-CHUNK-0047 | [ ] TODO | source | critical | authored-source | `android/app/src/main/java/com/dxxredux/app/MissionZip.kt` | L1-L360 | - |
| R1-CHUNK-0048 | [ ] TODO | source | critical | authored-source | `android/app/src/main/java/com/dxxredux/app/MissionZipAudioFingerprintCache.kt` | L1-L286 | - |
| R1-CHUNK-0049 | [ ] TODO | source | critical | authored-source | `android/app/src/main/java/com/dxxredux/app/MissionZipExtractionStore.kt` | L1-L524 | - |
| R1-CHUNK-0050 | [ ] TODO | source | critical | authored-source | `2 related paths` | 270 review lines under server/src | - |
| R1-CHUNK-0051 | [ ] TODO | source | critical | authored-source | `2 related paths` | 562 review lines under server/src | - |
| R1-CHUNK-0052 | [ ] TODO | source | critical | authored-source | `2 related paths` | 595 review lines under server/src | - |
| R1-CHUNK-0053 | [ ] TODO | source | critical | authored-source | `3 related paths` | 555 review lines under server/src | - |
| R1-CHUNK-0054 | [ ] TODO | source | critical | authored-source | `3 related paths` | 445 review lines under server/src | - |
| R1-CHUNK-0055 | [ ] TODO | source | critical | authored-source | `4 related paths` | 498 review lines under server/src | - |
| R1-CHUNK-0056 | [ ] TODO | source | critical | authored-source | `server/src/db.rs` | L1-L532 | - |
| R1-CHUNK-0057 | [ ] TODO | source | critical | authored-source | `server/src/ws_handler.rs` | L1-L600 | - |
| R1-CHUNK-0058 | [ ] TODO | source | critical | authored-source | `server/src/ws_handler.rs` | L601-L1200 | - |
| R1-CHUNK-0059 | [ ] TODO | source | critical | authored-source | `server/src/ws_handler.rs` | L1201-L1800 | - |
| R1-CHUNK-0060 | [ ] TODO | source | critical | authored-source | `server/src/ws_handler.rs` | L1801-L2400 | - |
| R1-CHUNK-0061 | [ ] TODO | source | critical | authored-source | `server/src/ws_handler.rs` | L2401-L2951 | - |
| R1-CHUNK-0062 | [ ] TODO | source | critical | build-script | `2 related paths` | 320 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0063 | [ ] TODO | source | critical | build-script | `android/get_deps/helpers/get_7zip.ps1` | L1-L88 | - |
| R1-CHUNK-0064 | [ ] TODO | source | critical | build-script | `2 related paths` | 183 review lines under android/helpers | - |
| R1-CHUNK-0065 | [ ] TODO | source | critical | build-script | `2 related paths` | 304 review lines under android/helpers | - |
| R1-CHUNK-0066 | [ ] TODO | source | critical | build-script | `android/helpers/run_mission_zip_batch.ps1` | L1-L600 | - |
| R1-CHUNK-0067 | [ ] TODO | source | critical | build-script | `2 related paths` | 405 review lines under game_data | - |
| R1-CHUNK-0068 | [ ] TODO | source | critical | build-script | `game_data/extract_dos_demos.ps1` | L1-L277 | - |
| R1-CHUNK-0069 | [ ] TODO | source | critical | build-script | `game_data/extract_mac_cd.ps1` | L1-L450 | - |
| R1-CHUNK-0070 | [ ] TODO | source | critical | build-script | `game_data/extract_mac_demos.ps1` | L1-L198 | - |
| R1-CHUNK-0071 | [ ] TODO | source | critical | build-script | `game_data/fingerprint_mission_zip_music.ps1` | L1-L600 | - |
| R1-CHUNK-0072 | [ ] TODO | source | critical | build-script | `game_data/fingerprint_mission_zip_music.ps1` | L601-L780 | - |
| R1-CHUNK-0073 | [ ] TODO | source | critical | test-source | `2 related paths` | 515 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0074 | [ ] TODO | source | critical | test-source | `2 related paths` | 73 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0075 | [ ] TODO | source | critical | test-source | `4 related paths` | 596 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0076 | [ ] TODO | source | critical | test-source | `5 related paths` | 90 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0077 | [ ] TODO | source | critical | test-source | `android/app/src/main/cpp/extract/test_cue_iso.c` | L1-L600 | - |
| R1-CHUNK-0078 | [ ] TODO | source | critical | test-source | `android/app/src/main/cpp/extract/test_cue_iso.c` | L601-L1200 | - |
| R1-CHUNK-0079 | [ ] TODO | source | critical | test-source | `android/app/src/main/cpp/extract/test_cue_iso.c` | L1201-L1800 | - |
| R1-CHUNK-0080 | [ ] TODO | source | critical | test-source | `android/app/src/main/cpp/extract/test_cue_iso.c` | L1801-L2033 | - |
| R1-CHUNK-0081 | [ ] TODO | source | critical | test-source | `android/app/src/main/cpp/extract/test_hfs.c` | L1-L600 | - |
| R1-CHUNK-0082 | [ ] TODO | source | critical | test-source | `android/app/src/main/cpp/extract/test_sti2.c` | L1-L596 | - |
| R1-CHUNK-0083 | [ ] TODO | source | critical | test-source | `android/app/src/main/cpp/extract/test_stuffit_corpus.cpp` | L1-L600 | - |
| R1-CHUNK-0084 | [ ] TODO | source | critical | test-source | `3 related paths` | 375 review lines under android/app/src/test/java | - |
| R1-CHUNK-0085 | [ ] TODO | source | critical | test-source | `7 related paths` | 509 review lines under android/app/src/test/java | - |
| R1-CHUNK-0086 | [ ] TODO | source | critical | test-source | `android/app/src/test/java/com/dxxredux/app/MissionZipMusicTest.kt` | L1-L234 | - |
| R1-CHUNK-0087 | [ ] TODO | source | critical | test-source | `android/app/src/test/java/com/dxxredux/app/MissionZipTest.kt` | L1-L471 | - |
| R1-CHUNK-0088 | [ ] TODO | source | critical | test-source | `android/app/src/test/java/com/dxxredux/app/ModManagerMissionZipTest.kt` | L1-L600 | - |
| R1-CHUNK-0089 | [ ] TODO | source | critical | test-source | `android/app/src/test/java/com/dxxredux/app/ModManagerMissionZipTest.kt` | L601-L930 | - |
| R1-CHUNK-0090 | [ ] TODO | source | critical | test-source | `4 related paths` | 149 review lines under android/game_scripts | - |
| R1-CHUNK-0091 | [ ] TODO | source | critical | test-source | `2 related paths` | 321 review lines under android/tests | - |
| R1-CHUNK-0092 | [ ] TODO | source | critical | test-source | `2 related paths` | 133 review lines under android/tests | - |
| R1-CHUNK-0093 | [ ] TODO | source | critical | test-source | `2 related paths` | 306 review lines under android/tests | - |
| R1-CHUNK-0094 | [ ] TODO | source | critical | test-source | `android/tests/extract_regression_spec_helpers.ps1` | L1-L326 | - |
| R1-CHUNK-0095 | [ ] TODO | source | critical | test-source | `android/tests/test_extract.ps1` | L1-L600 | - |
| R1-CHUNK-0096 | [ ] TODO | source | critical | test-source | `android/tests/test_extract.ps1` | L601-L1200 | - |
| R1-CHUNK-0097 | [ ] TODO | source | critical | test-source | `android/tests/test_saf_archiver.ps1` | L1-L536 | - |
| R1-CHUNK-0098 | [ ] TODO | source | high | authored-config | `d1/CMakePresets.json` | diff hunks 1-1, new L20-L20 | - |
| R1-CHUNK-0099 | [ ] TODO | source | high | authored-config | `d2/CMakePresets.json` | diff hunks 1-1, new L20-L20 | - |
| R1-CHUNK-0100 | [ ] TODO | source | high | authored-config | `3 related paths` | 171 review lines under server | - |
| R1-CHUNK-0101 | [ ] TODO | source | high | authored-source | `10 related paths` | 522 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0102 | [ ] TODO | source | high | authored-source | `10 related paths` | 716 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0103 | [ ] TODO | source | high | authored-source | `2 related paths` | 539 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0104 | [ ] TODO | source | high | authored-source | `2 related paths` | 90 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0105 | [ ] TODO | source | high | authored-source | `2 related paths` | 137 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0106 | [ ] TODO | source | high | authored-source | `2 related paths` | 302 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0107 | [ ] TODO | source | high | authored-source | `2 related paths` | 531 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0108 | [ ] TODO | source | high | authored-source | `2 related paths` | 123 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0109 | [ ] TODO | source | high | authored-source | `2 related paths` | 597 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0110 | [ ] TODO | source | high | authored-source | `2 related paths` | 494 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0111 | [ ] TODO | source | high | authored-source | `2 related paths` | 76 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0112 | [ ] TODO | source | high | authored-source | `2 related paths` | 714 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0113 | [ ] TODO | source | high | authored-source | `2 related paths` | 172 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0114 | [ ] TODO | source | high | authored-source | `2 related paths` | 101 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0115 | [ ] TODO | source | high | authored-source | `2 related paths` | 702 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0116 | [ ] TODO | source | high | authored-source | `3 related paths` | 740 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0117 | [ ] TODO | source | high | authored-source | `3 related paths` | 215 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0118 | [ ] TODO | source | high | authored-source | `3 related paths` | 519 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0119 | [ ] TODO | source | high | authored-source | `3 related paths` | 469 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0120 | [ ] TODO | source | high | authored-source | `3 related paths` | 425 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0121 | [ ] TODO | source | high | authored-source | `3 related paths` | 397 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0122 | [ ] TODO | source | high | authored-source | `3 related paths` | 424 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0123 | [ ] TODO | source | high | authored-source | `3 related paths` | 564 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0124 | [ ] TODO | source | high | authored-source | `3 related paths` | 332 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0125 | [ ] TODO | source | high | authored-source | `3 related paths` | 369 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0126 | [ ] TODO | source | high | authored-source | `4 related paths` | 599 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0127 | [ ] TODO | source | high | authored-source | `4 related paths` | 633 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0128 | [ ] TODO | source | high | authored-source | `4 related paths` | 685 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0129 | [ ] TODO | source | high | authored-source | `4 related paths` | 748 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0130 | [ ] TODO | source | high | authored-source | `4 related paths` | 745 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0131 | [ ] TODO | source | high | authored-source | `4 related paths` | 635 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0132 | [ ] TODO | source | high | authored-source | `4 related paths` | 478 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0133 | [ ] TODO | source | high | authored-source | `4 related paths` | 745 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0134 | [ ] TODO | source | high | authored-source | `4 related paths` | 416 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0135 | [ ] TODO | source | high | authored-source | `4 related paths` | 595 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0136 | [ ] TODO | source | high | authored-source | `4 related paths` | 622 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0137 | [ ] TODO | source | high | authored-source | `5 related paths` | 716 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0138 | [ ] TODO | source | high | authored-source | `5 related paths` | 438 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0139 | [ ] TODO | source | high | authored-source | `5 related paths` | 490 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0140 | [ ] TODO | source | high | authored-source | `5 related paths` | 736 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0141 | [ ] TODO | source | high | authored-source | `5 related paths` | 421 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0142 | [ ] TODO | source | high | authored-source | `5 related paths` | 682 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0143 | [ ] TODO | source | high | authored-source | `5 related paths` | 708 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0144 | [ ] TODO | source | high | authored-source | `5 related paths` | 569 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0145 | [ ] TODO | source | high | authored-source | `5 related paths` | 620 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0146 | [ ] TODO | source | high | authored-source | `5 related paths` | 676 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0147 | [ ] TODO | source | high | authored-source | `6 related paths` | 612 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0148 | [ ] TODO | source | high | authored-source | `7 related paths` | 431 review lines under android/app/src/main/cpp | - |
| R1-CHUNK-0149 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/android_autoselect.cpp` | L1-L433 | - |
| R1-CHUNK-0150 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/android_gamepad_config.cpp` | L1-L431 | - |
| R1-CHUNK-0151 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/android_input.c` | L1-L750 | - |
| R1-CHUNK-0152 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/android_input.c` | L751-L1500 | - |
| R1-CHUNK-0153 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/android_input.c` | L1501-L1929 | - |
| R1-CHUNK-0154 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/android_pilot_prefs.cpp` | L1-L519 | - |
| R1-CHUNK-0155 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/headless/headless_metadata_dump_main.cpp` | L1-L750 | - |
| R1-CHUNK-0156 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/android_menu_scale.c` | L1-L600 | - |
| R1-CHUNK-0157 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/android_profile.c` | L1-L738 | - |
| R1-CHUNK-0158 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/automap_metadata_overlay.c` | L1-L750 | - |
| R1-CHUNK-0159 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/cd_preview.c` | L1-L721 | - |
| R1-CHUNK-0160 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/coop_indicator_lines_math.h` | L1-L70 | - |
| R1-CHUNK-0161 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/coop_indicator_lines.c` | L1-L606 | - |
| R1-CHUNK-0162 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/coop/coop_save.c` | L1-L750 | - |
| R1-CHUNK-0163 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/digi_tsf_music.c` | L1-L750 | - |
| R1-CHUNK-0164 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/game_automate.cpp` | L1-L750 | - |
| R1-CHUNK-0165 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/game_automate.cpp` | L751-L1500 | - |
| R1-CHUNK-0166 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/game_automate.cpp` | L1501-L2250 | - |
| R1-CHUNK-0167 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/game_automate.cpp` | L2251-L3000 | - |
| R1-CHUNK-0168 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/game_introspect.cpp` | L1-L750 | - |
| R1-CHUNK-0169 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/game_introspect.cpp` | L751-L1500 | - |
| R1-CHUNK-0170 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/game_introspect.cpp` | L1501-L2155 | - |
| R1-CHUNK-0171 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/game_introspect.h` | L1-L84 | - |
| R1-CHUNK-0172 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/gles3_shim.c` | L1-L750 | - |
| R1-CHUNK-0173 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/input_demo_controls.cpp` | L1-L750 | - |
| R1-CHUNK-0174 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/input_demo_debug_logging.cpp` | L1-L602 | - |
| R1-CHUNK-0175 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/input_demo_fixture.cpp` | L1-L750 | - |
| R1-CHUNK-0176 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/input_demo_hooks_shared.c` | L1-L750 | - |
| R1-CHUNK-0177 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/input_demo_hooks_shared.c` | L751-L1500 | - |
| R1-CHUNK-0178 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/input_demo_hooks_shared.c` | L1501-L2243 | - |
| R1-CHUNK-0179 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/input_demo_hooks_shared.h` | L1-L57 | - |
| R1-CHUNK-0180 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/input_demo_newdemo_shared.c` | L1-L704 | - |
| R1-CHUNK-0181 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/input_demo_newdemo_shared.h` | L1-L9 | - |
| R1-CHUNK-0182 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/input_demo_recorder.cpp` | L1-L750 | - |
| R1-CHUNK-0183 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/input_demo_replay.cpp` | L1-L750 | - |
| R1-CHUNK-0184 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/input_demo_result.cpp` | L1-L659 | - |
| R1-CHUNK-0185 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/input_demo_rng_trace.h` | L1-L54 | - |
| R1-CHUNK-0186 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/input_demo_start_shared.c` | L1-L700 | - |
| R1-CHUNK-0187 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/input_demo_start_shared.h` | L1-L64 | - |
| R1-CHUNK-0188 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/input_demo_state_trace.cpp` | L1-L750 | - |
| R1-CHUNK-0189 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/level_metadata_scan.c` | L1-L537 | - |
| R1-CHUNK-0190 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/level_metadata_scan.h` | L1-L263 | - |
| R1-CHUNK-0191 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/merged_wall_debug.c` | L1-L750 | - |
| R1-CHUNK-0192 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/merged_wall_debug.c` | L751-L1500 | - |
| R1-CHUNK-0193 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/merged_wall_debug.c` | L1501-L2250 | - |
| R1-CHUNK-0194 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/merged_wall_debug.c` | L2251-L3000 | - |
| R1-CHUNK-0195 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/merged_wall_debug.c` | L3001-L3750 | - |
| R1-CHUNK-0196 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/merged_wall_debug.c` | L3751-L4500 | - |
| R1-CHUNK-0197 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/merged_wall_debug.c` | L4501-L5250 | - |
| R1-CHUNK-0198 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/merged_wall_debug.c` | L5251-L6000 | - |
| R1-CHUNK-0199 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/merged_wall_debug.c` | L6001-L6750 | - |
| R1-CHUNK-0200 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/merged_wall_debug.c` | L6751-L7500 | - |
| R1-CHUNK-0201 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/merged_wall_debug.c` | L7501-L8172 | - |
| R1-CHUNK-0202 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/midi_preview.c` | L1-L695 | - |
| R1-CHUNK-0203 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/midi_preview.h` | L1-L61 | - |
| R1-CHUNK-0204 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/multi_save_transfer.c` | L1-L750 | - |
| R1-CHUNK-0205 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/pngfile_stb.c` | L1-L475 | - |
| R1-CHUNK-0206 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/rbaudio_bin.c` | L1-L750 | - |
| R1-CHUNK-0207 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/rbaudio_bin.c` | L751-L1500 | - |
| R1-CHUNK-0208 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/route_planner.cpp` | L1-L750 | - |
| R1-CHUNK-0209 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/route_planner.cpp` | L751-L1500 | - |
| R1-CHUNK-0210 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/route_planner.cpp` | L1501-L2250 | - |
| R1-CHUNK-0211 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/route_planner.cpp` | L2251-L3000 | - |
| R1-CHUNK-0212 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/route_snapshot.cpp` | L1-L616 | - |
| R1-CHUNK-0213 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/route_snapshot.h` | L1-L207 | - |
| R1-CHUNK-0214 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/secret_area_game_adapter.c` | L1-L750 | - |
| R1-CHUNK-0215 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/secret_area_game_adapter.c` | L751-L1500 | - |
| R1-CHUNK-0216 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/secret_area_game_adapter.c` | L1501-L2250 | - |
| R1-CHUNK-0217 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/secret_area_scan.c` | L1-L750 | - |
| R1-CHUNK-0218 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/state_android_shared.c` | L1-L750 | - |
| R1-CHUNK-0219 | [ ] TODO | source | high | authored-source | `2 related paths` | 432 review lines under android/app/src/main/java | - |
| R1-CHUNK-0220 | [ ] TODO | source | high | authored-source | `3 related paths` | 494 review lines under android/app/src/main/java | - |
| R1-CHUNK-0221 | [ ] TODO | source | high | authored-source | `android/app/src/main/java/com/dxxredux/app/multiplayer/NetworkProtocol.kt` | L1-L666 | - |
| R1-CHUNK-0222 | [ ] TODO | source | high | authored-source | `android/app/src/main/java/com/dxxredux/app/StorageFailureDialog.kt` | L1-L29 | - |
| R1-CHUNK-0223 | [ ] TODO | source | high | authored-source | `android/app/src/main/res/xml/network_security_config.xml` | L1-L10 | - |
| R1-CHUNK-0224 | [ ] TODO | source | high | authored-source | `6 related paths` | 201 review lines under d1/2d | - |
| R1-CHUNK-0225 | [ ] TODO | source | high | authored-source | `d1/3d/interp.c` | diff hunks 1-7, new L21-L418 | - |
| R1-CHUNK-0226 | [ ] TODO | source | high | authored-source | `4 related paths` | 101 review lines under d1/arch | - |
| R1-CHUNK-0227 | [ ] TODO | source | high | authored-source | `4 related paths` | 747 review lines under d1/arch | - |
| R1-CHUNK-0228 | [ ] TODO | source | high | authored-source | `9 related paths` | 409 review lines under d1/arch | - |
| R1-CHUNK-0229 | [ ] TODO | source | high | authored-source | `d1/arch/ogl/ogl.c` | diff hunks 1-64, new L13-L1610 | - |
| R1-CHUNK-0230 | [ ] TODO | source | high | authored-source | `d1/arch/ogl/ogl.c` | diff hunks 65-103, new L1612-L2951 | - |
| R1-CHUNK-0231 | [ ] TODO | source | high | authored-source | `16 related paths` | 155 review lines under d1/include | - |
| R1-CHUNK-0232 | [ ] TODO | source | high | authored-source | `2 related paths` | 16 review lines under d1/include | - |
| R1-CHUNK-0233 | [ ] TODO | source | high | authored-source | `10 related paths` | 666 review lines under d1/main | - |
| R1-CHUNK-0234 | [ ] TODO | source | high | authored-source | `16 related paths` | 608 review lines under d1/main | - |
| R1-CHUNK-0235 | [ ] TODO | source | high | authored-source | `3 related paths` | 44 review lines under d1/main | - |
| R1-CHUNK-0236 | [ ] TODO | source | high | authored-source | `3 related paths` | 584 review lines under d1/main | - |
| R1-CHUNK-0237 | [ ] TODO | source | high | authored-source | `3 related paths` | 92 review lines under d1/main | - |
| R1-CHUNK-0238 | [ ] TODO | source | high | authored-source | `3 related paths` | 527 review lines under d1/main | - |
| R1-CHUNK-0239 | [ ] TODO | source | high | authored-source | `4 related paths` | 300 review lines under d1/main | - |
| R1-CHUNK-0240 | [ ] TODO | source | high | authored-source | `4 related paths` | 718 review lines under d1/main | - |
| R1-CHUNK-0241 | [ ] TODO | source | high | authored-source | `6 related paths` | 266 review lines under d1/main | - |
| R1-CHUNK-0242 | [ ] TODO | source | high | authored-source | `7 related paths` | 650 review lines under d1/main | - |
| R1-CHUNK-0243 | [ ] TODO | source | high | authored-source | `8 related paths` | 467 review lines under d1/main | - |
| R1-CHUNK-0244 | [ ] TODO | source | high | authored-source | `8 related paths` | 738 review lines under d1/main | - |
| R1-CHUNK-0245 | [ ] TODO | source | high | authored-source | `9 related paths` | 612 review lines under d1/main | - |
| R1-CHUNK-0246 | [ ] TODO | source | high | authored-source | `d1/main/input_demo_hooks.c` | L1-L750 | - |
| R1-CHUNK-0247 | [ ] TODO | source | high | authored-source | `d1/main/net_udp.c` | diff hunks 1-165, new L13-L5626 | - |
| R1-CHUNK-0248 | [ ] TODO | source | high | authored-source | `d1/main/net_udp.c` | diff hunks 166-245, new L5633-L7684 | - |
| R1-CHUNK-0249 | [ ] TODO | source | high | authored-source | `d1/main/newmenu.c` | diff hunks 1-81, new L50-L1978 | - |
| R1-CHUNK-0250 | [ ] TODO | source | high | authored-source | `d1/main/newmenu.c` | diff hunks 82-129, new L1980-L3361 | - |
| R1-CHUNK-0251 | [ ] TODO | source | high | authored-source | `d1/main/state.c` | diff hunks 12-12, new L141-L947 | - |
| R1-CHUNK-0252 | [ ] TODO | source | high | authored-source | `d1/maths/rand.c` | diff hunks 1-11, new L4-L211 | - |
| R1-CHUNK-0253 | [ ] TODO | source | high | authored-source | `4 related paths` | 64 review lines under d1/misc | - |
| R1-CHUNK-0254 | [ ] TODO | source | high | authored-source | `d1/texmap/texmapl.h` | diff hunks 1-2, new L1-L152 | - |
| R1-CHUNK-0255 | [ ] TODO | source | high | authored-source | `4 related paths` | 25 review lines under d1/xmodel | - |
| R1-CHUNK-0256 | [ ] TODO | source | high | authored-source | `6 related paths` | 204 review lines under d2/2d | - |
| R1-CHUNK-0257 | [ ] TODO | source | high | authored-source | `d2/3d/interp.c` | diff hunks 1-12, new L21-L682 | - |
| R1-CHUNK-0258 | [ ] TODO | source | high | authored-source | `12 related paths` | 623 review lines under d2/arch | - |
| R1-CHUNK-0259 | [ ] TODO | source | high | authored-source | `7 related paths` | 131 review lines under d2/arch | - |
| R1-CHUNK-0260 | [ ] TODO | source | high | authored-source | `d2/arch/ogl/ogl.c` | diff hunks 1-63, new L13-L1620 | - |
| R1-CHUNK-0261 | [ ] TODO | source | high | authored-source | `d2/arch/ogl/ogl.c` | diff hunks 64-101, new L1622-L2965 | - |
| R1-CHUNK-0262 | [ ] TODO | source | high | authored-source | `d2/arch/ogl/ogl.c` | diff hunks 102-125, new L2969-L3871 | - |
| R1-CHUNK-0263 | [ ] TODO | source | high | authored-source | `16 related paths` | 174 review lines under d2/include | - |
| R1-CHUNK-0264 | [ ] TODO | source | high | authored-source | `4 related paths` | 45 review lines under d2/include | - |
| R1-CHUNK-0265 | [ ] TODO | source | high | authored-source | `d2/libmve/mveplay.c` | diff hunks 1-16, new L237-L982 | - |
| R1-CHUNK-0266 | [ ] TODO | source | high | authored-source | `11 related paths` | 690 review lines under d2/main | - |
| R1-CHUNK-0267 | [ ] TODO | source | high | authored-source | `2 related paths` | 412 review lines under d2/main | - |
| R1-CHUNK-0268 | [ ] TODO | source | high | authored-source | `2 related paths` | 700 review lines under d2/main | - |
| R1-CHUNK-0269 | [ ] TODO | source | high | authored-source | `3 related paths` | 468 review lines under d2/main | - |
| R1-CHUNK-0270 | [ ] TODO | source | high | authored-source | `3 related paths` | 84 review lines under d2/main | - |
| R1-CHUNK-0271 | [ ] TODO | source | high | authored-source | `3 related paths` | 672 review lines under d2/main | - |
| R1-CHUNK-0272 | [ ] TODO | source | high | authored-source | `3 related paths` | 665 review lines under d2/main | - |
| R1-CHUNK-0273 | [ ] TODO | source | high | authored-source | `4 related paths` | 532 review lines under d2/main | - |
| R1-CHUNK-0274 | [ ] TODO | source | high | authored-source | `4 related paths` | 738 review lines under d2/main | - |
| R1-CHUNK-0275 | [ ] TODO | source | high | authored-source | `4 related paths` | 473 review lines under d2/main | - |
| R1-CHUNK-0276 | [ ] TODO | source | high | authored-source | `4 related paths` | 550 review lines under d2/main | - |
| R1-CHUNK-0277 | [ ] TODO | source | high | authored-source | `4 related paths` | 632 review lines under d2/main | - |
| R1-CHUNK-0278 | [ ] TODO | source | high | authored-source | `5 related paths` | 709 review lines under d2/main | - |
| R1-CHUNK-0279 | [ ] TODO | source | high | authored-source | `5 related paths` | 502 review lines under d2/main | - |
| R1-CHUNK-0280 | [ ] TODO | source | high | authored-source | `6 related paths` | 729 review lines under d2/main | - |
| R1-CHUNK-0281 | [ ] TODO | source | high | authored-source | `6 related paths` | 202 review lines under d2/main | - |
| R1-CHUNK-0282 | [ ] TODO | source | high | authored-source | `7 related paths` | 632 review lines under d2/main | - |
| R1-CHUNK-0283 | [ ] TODO | source | high | authored-source | `7 related paths` | 689 review lines under d2/main | - |
| R1-CHUNK-0284 | [ ] TODO | source | high | authored-source | `7 related paths` | 503 review lines under d2/main | - |
| R1-CHUNK-0285 | [ ] TODO | source | high | authored-source | `8 related paths` | 281 review lines under d2/main | - |
| R1-CHUNK-0286 | [ ] TODO | source | high | authored-source | `9 related paths` | 467 review lines under d2/main | - |
| R1-CHUNK-0287 | [ ] TODO | source | high | authored-source | `d2/main/ai.c` | diff hunks 1-91, new L53-L2348 | - |
| R1-CHUNK-0288 | [ ] TODO | source | high | authored-source | `d2/main/d1_custom.c` | L1-L750 | - |
| R1-CHUNK-0289 | [ ] TODO | source | high | authored-source | `d2/main/d1_in_d2.c` | L1-L750 | - |
| R1-CHUNK-0290 | [ ] TODO | source | high | authored-source | `d2/main/d1_in_d2.c` | L751-L1500 | - |
| R1-CHUNK-0291 | [ ] TODO | source | high | authored-source | `d2/main/d1_save_translate.c` | L1-L750 | - |
| R1-CHUNK-0292 | [ ] TODO | source | high | authored-source | `d2/main/dxa_metadata_patch.cpp` | L1-L750 | - |
| R1-CHUNK-0293 | [ ] TODO | source | high | authored-source | `d2/main/escort.c` | diff hunks 4-4, new L129-L1435 | - |
| R1-CHUNK-0294 | [ ] TODO | source | high | authored-source | `d2/main/escort.c` | diff hunks 5-40, new L1437-L2465 | - |
| R1-CHUNK-0295 | [ ] TODO | source | high | authored-source | `d2/main/escort.c` | diff hunks 41-105, new L2478-L3865 | - |
| R1-CHUNK-0296 | [ ] TODO | source | high | authored-source | `d2/main/escort.c` | diff hunks 106-151, new L3867-L4557 | - |
| R1-CHUNK-0297 | [ ] TODO | source | high | authored-source | `d2/main/input_demo_hooks.c` | L1-L750 | - |
| R1-CHUNK-0298 | [ ] TODO | source | high | authored-source | `d2/main/input_demo_hooks.c` | L751-L1500 | - |
| R1-CHUNK-0299 | [ ] TODO | source | high | authored-source | `d2/main/input_demo_hooks.c` | L1501-L2250 | - |
| R1-CHUNK-0300 | [ ] TODO | source | high | authored-source | `d2/main/input_demo_hooks.c` | L2251-L3000 | - |
| R1-CHUNK-0301 | [ ] TODO | source | high | authored-source | `d2/main/input_demo_hooks.c` | L3001-L3750 | - |
| R1-CHUNK-0302 | [ ] TODO | source | high | authored-source | `d2/main/input_demo_hooks.c` | L3751-L4500 | - |
| R1-CHUNK-0303 | [ ] TODO | source | high | authored-source | `d2/main/input_demo_hooks.c` | L4501-L5250 | - |
| R1-CHUNK-0304 | [ ] TODO | source | high | authored-source | `d2/main/input_demo_hooks.c` | L5251-L6000 | - |
| R1-CHUNK-0305 | [ ] TODO | source | high | authored-source | `d2/main/input_demo_hooks.c` | L6001-L6750 | - |
| R1-CHUNK-0306 | [ ] TODO | source | high | authored-source | `d2/main/laser.c` | diff hunks 1-89, new L46-L2835 | - |
| R1-CHUNK-0307 | [ ] TODO | source | high | authored-source | `d2/main/net_udp.c` | diff hunks 1-174, new L17-L5497 | - |
| R1-CHUNK-0308 | [ ] TODO | source | high | authored-source | `d2/main/net_udp.c` | diff hunks 175-276, new L5512-L7961 | - |
| R1-CHUNK-0309 | [ ] TODO | source | high | authored-source | `d2/main/net_udp.h` | diff hunks 1-5, new L1-L206 | - |
| R1-CHUNK-0310 | [ ] TODO | source | high | authored-source | `d2/main/newdemo.c` | diff hunks 1-60, new L28-L4325 | - |
| R1-CHUNK-0311 | [ ] TODO | source | high | authored-source | `d2/main/newdemo.h` | diff hunks 1-3, new L104-L140 | - |
| R1-CHUNK-0312 | [ ] TODO | source | high | authored-source | `d2/main/newmenu.c` | diff hunks 1-81, new L51-L1966 | - |
| R1-CHUNK-0313 | [ ] TODO | source | high | authored-source | `d2/main/newmenu.c` | diff hunks 82-129, new L1968-L3302 | - |
| R1-CHUNK-0314 | [ ] TODO | source | high | authored-source | `d2/main/state.c` | diff hunks 12-12, new L156-L1019 | - |
| R1-CHUNK-0315 | [ ] TODO | source | high | authored-source | `d2/main/state.c` | diff hunks 13-111, new L1027-L3207 | - |
| R1-CHUNK-0316 | [ ] TODO | source | high | authored-source | `d2/maths/rand.c` | diff hunks 1-14, new L8-L242 | - |
| R1-CHUNK-0317 | [ ] TODO | source | high | authored-source | `4 related paths` | 73 review lines under d2/misc | - |
| R1-CHUNK-0318 | [ ] TODO | source | high | authored-source | `d2/texmap/texmapl.h` | diff hunks 1-2, new L1-L93 | - |
| R1-CHUNK-0319 | [ ] TODO | source | high | authored-source | `4 related paths` | 25 review lines under d2/xmodel | - |
| R1-CHUNK-0320 | [ ] TODO | source | high | authored-source | `server/Cargo.toml` | L1-L38 | - |
| R1-CHUNK-0321 | [ ] TODO | source | high | build-script | `android/app/src/main/cpp/CMakeLists.txt` | L1-L750 | - |
| R1-CHUNK-0322 | [ ] TODO | source | high | build-script | `android/app/src/main/cpp/CMakeLists.txt` | L751-L793 | - |
| R1-CHUNK-0323 | [ ] TODO | source | high | build-script | `d1/CMakeLists.txt` | diff hunks 1-13, new L1-L151 | - |
| R1-CHUNK-0324 | [ ] TODO | source | high | build-script | `d1/2d/CMakeLists.txt` | diff hunks 1-3, new L1-L26 | - |
| R1-CHUNK-0325 | [ ] TODO | source | high | build-script | `d1/3d/CMakeLists.txt` | diff hunks 1-3, new L1-L22 | - |
| R1-CHUNK-0326 | [ ] TODO | source | high | build-script | `5 related paths` | 43 review lines under d1/arch | - |
| R1-CHUNK-0327 | [ ] TODO | source | high | build-script | `d1/editor/CMakeLists.txt` | diff hunks 1-2, new L1-L42 | - |
| R1-CHUNK-0328 | [ ] TODO | source | high | build-script | `d1/iff/CMakeLists.txt` | diff hunks 1-2, new L1-L9 | - |
| R1-CHUNK-0329 | [ ] TODO | source | high | build-script | `d1/main/CMakeLists.txt` | diff hunks 1-16, new L1-L333 | - |
| R1-CHUNK-0330 | [ ] TODO | source | high | build-script | `d1/maths/CMakeLists.txt` | diff hunks 1-4, new L1-L222 | - |
| R1-CHUNK-0331 | [ ] TODO | source | high | build-script | `d1/mem/CMakeLists.txt` | diff hunks 1-2, new L1-L9 | - |
| R1-CHUNK-0332 | [ ] TODO | source | high | build-script | `d1/misc/CMakeLists.txt` | diff hunks 1-3, new L1-L20 | - |
| R1-CHUNK-0333 | [ ] TODO | source | high | build-script | `d1/texmap/CMakeLists.txt` | diff hunks 1-6, new L1-L30 | - |
| R1-CHUNK-0334 | [ ] TODO | source | high | build-script | `d1/ui/CMakeLists.txt` | diff hunks 1-2, new L1-L28 | - |
| R1-CHUNK-0335 | [ ] TODO | source | high | build-script | `d1/xmodel/CMakeLists.txt` | diff hunks 1-2, new L1-L12 | - |
| R1-CHUNK-0336 | [ ] TODO | source | high | build-script | `d2/CMakeLists.txt` | diff hunks 1-13, new L6-L156 | - |
| R1-CHUNK-0337 | [ ] TODO | source | high | build-script | `d2/2d/CMakeLists.txt` | diff hunks 1-3, new L1-L26 | - |
| R1-CHUNK-0338 | [ ] TODO | source | high | build-script | `d2/3d/CMakeLists.txt` | diff hunks 1-3, new L1-L22 | - |
| R1-CHUNK-0339 | [ ] TODO | source | high | build-script | `5 related paths` | 39 review lines under d2/arch | - |
| R1-CHUNK-0340 | [ ] TODO | source | high | build-script | `d2/editor/CMakeLists.txt` | diff hunks 1-2, new L1-L42 | - |
| R1-CHUNK-0341 | [ ] TODO | source | high | build-script | `d2/iff/CMakeLists.txt` | diff hunks 1-2, new L1-L9 | - |
| R1-CHUNK-0342 | [ ] TODO | source | high | build-script | `d2/libmve/CMakeLists.txt` | diff hunks 1-5, new L1-L27 | - |
| R1-CHUNK-0343 | [ ] TODO | source | high | build-script | `d2/main/CMakeLists.txt` | diff hunks 1-19, new L1-L405 | - |
| R1-CHUNK-0344 | [ ] TODO | source | high | build-script | `d2/maths/CMakeLists.txt` | diff hunks 1-4, new L1-L239 | - |
| R1-CHUNK-0345 | [ ] TODO | source | high | build-script | `d2/mem/CMakeLists.txt` | diff hunks 1-2, new L1-L9 | - |
| R1-CHUNK-0346 | [ ] TODO | source | high | build-script | `d2/misc/CMakeLists.txt` | diff hunks 1-3, new L1-L21 | - |
| R1-CHUNK-0347 | [ ] TODO | source | high | build-script | `d2/texmap/CMakeLists.txt` | diff hunks 1-6, new L1-L30 | - |
| R1-CHUNK-0348 | [ ] TODO | source | high | build-script | `d2/ui/CMakeLists.txt` | diff hunks 1-2, new L1-L28 | - |
| R1-CHUNK-0349 | [ ] TODO | source | high | build-script | `d2/xmodel/CMakeLists.txt` | diff hunks 1-2, new L1-L12 | - |
| R1-CHUNK-0350 | [ ] TODO | source | high | build-script | `9 related paths` | 507 review lines under server | - |
| R1-CHUNK-0351 | [ ] TODO | source | high | documentation | `d1/INSTALL.txt` | diff hunks 1-1, new L93-L93 | - |
| R1-CHUNK-0352 | [ ] TODO | source | high | documentation | `d2/INSTALL.txt` | diff hunks 1-1, new L107-L107 | - |
| R1-CHUNK-0353 | [ ] TODO | source | high | documentation | `server/rust_version.txt` | L1-L7 | - |
| R1-CHUNK-0354 | [ ] TODO | source | high | test-source | `2 related paths` | 82 review lines under android/app/src/test/java | - |
| R1-CHUNK-0355 | [ ] TODO | source | high | test-source | `3 related paths` | 350 review lines under android/tests | - |
| R1-CHUNK-0356 | [ ] TODO | source | high | test-source | `server/tests/integration.rs` | L1-L750 | - |
| R1-CHUNK-0357 | [ ] TODO | source | high | test-source | `server/tests/integration.rs` | L751-L1500 | - |
| R1-CHUNK-0358 | [ ] TODO | source | high | test-source | `server/tests/integration.rs` | L1501-L2250 | - |
| R1-CHUNK-0359 | [ ] TODO | source | high | test-source | `server/tests/integration.rs` | L2251-L2901 | - |
| R1-CHUNK-0360 | [ ] TODO | source | high | test-source | `server/tests/nat_sim_tests.rs` | L1-L476 | - |
| R1-CHUNK-0361 | [ ] TODO | source | medium | authored-config | `3 related paths` | 124 review lines under .vscode | - |
| R1-CHUNK-0362 | [ ] TODO | source | medium | authored-config | `2 related paths` | 32 review lines under android | - |
| R1-CHUNK-0363 | [ ] TODO | source | medium | authored-config | `2 related paths` | 768 review lines under android/app/src/main/assets | - |
| R1-CHUNK-0364 | [ ] TODO | source | medium | authored-config | `6 related paths` | 604 review lines under android/app/src/main/assets | - |
| R1-CHUNK-0365 | [ ] TODO | source | medium | authored-config | `android/app/src/main/assets/known_discs.json5` | L1-L900 | - |
| R1-CHUNK-0366 | [ ] TODO | source | medium | authored-config | `android/get_deps/tool_versions.conf` | L1-L169 | - |
| R1-CHUNK-0367 | [ ] TODO | source | medium | authored-config | `16 related paths` | 479 review lines under game_data/CD images | - |
| R1-CHUNK-0368 | [ ] TODO | source | medium | authored-config | `9 related paths` | 600 review lines under game_data/CD images | - |
| R1-CHUNK-0369 | [ ] TODO | source | medium | authored-config | `9 related paths` | 804 review lines under game_data/CD images | - |
| R1-CHUNK-0370 | [ ] TODO | source | medium | authored-config | `game_data/demo installers/mac_stuffit_oracles.json` | L1-L85 | - |
| R1-CHUNK-0371 | [ ] TODO | source | medium | authored-config | `4 related paths` | 114 review lines under game_data/gog installers | - |
| R1-CHUNK-0372 | [ ] TODO | source | medium | authored-config | `16 related paths` | 430 review lines under game_data/music | - |
| R1-CHUNK-0373 | [ ] TODO | source | medium | authored-config | `7 related paths` | 129 review lines under game_data/music | - |
| R1-CHUNK-0374 | [ ] TODO | source | medium | authored-source | `android/gradle.properties` | L1-L7 | - |
| R1-CHUNK-0375 | [ ] TODO | source | medium | authored-source | `android/app/src/main/AndroidManifest.xml` | L1-L108 | - |
| R1-CHUNK-0376 | [ ] TODO | source | medium | authored-source | `2 related paths` | 885 review lines under android/app/src/main/java | - |
| R1-CHUNK-0377 | [ ] TODO | source | medium | authored-source | `2 related paths` | 775 review lines under android/app/src/main/java | - |
| R1-CHUNK-0378 | [ ] TODO | source | medium | authored-source | `2 related paths` | 293 review lines under android/app/src/main/java | - |
| R1-CHUNK-0379 | [ ] TODO | source | medium | authored-source | `2 related paths` | 756 review lines under android/app/src/main/java | - |
| R1-CHUNK-0380 | [ ] TODO | source | medium | authored-source | `2 related paths` | 742 review lines under android/app/src/main/java | - |
| R1-CHUNK-0381 | [ ] TODO | source | medium | authored-source | `2 related paths` | 544 review lines under android/app/src/main/java | - |
| R1-CHUNK-0382 | [ ] TODO | source | medium | authored-source | `2 related paths` | 861 review lines under android/app/src/main/java | - |
| R1-CHUNK-0383 | [ ] TODO | source | medium | authored-source | `2 related paths` | 814 review lines under android/app/src/main/java | - |
| R1-CHUNK-0384 | [ ] TODO | source | medium | authored-source | `2 related paths` | 698 review lines under android/app/src/main/java | - |
| R1-CHUNK-0385 | [ ] TODO | source | medium | authored-source | `2 related paths` | 385 review lines under android/app/src/main/java | - |
| R1-CHUNK-0386 | [ ] TODO | source | medium | authored-source | `2 related paths` | 779 review lines under android/app/src/main/java | - |
| R1-CHUNK-0387 | [ ] TODO | source | medium | authored-source | `2 related paths` | 848 review lines under android/app/src/main/java | - |
| R1-CHUNK-0388 | [ ] TODO | source | medium | authored-source | `2 related paths` | 31 review lines under android/app/src/main/java | - |
| R1-CHUNK-0389 | [ ] TODO | source | medium | authored-source | `3 related paths` | 623 review lines under android/app/src/main/java | - |
| R1-CHUNK-0390 | [ ] TODO | source | medium | authored-source | `3 related paths` | 872 review lines under android/app/src/main/java | - |
| R1-CHUNK-0391 | [ ] TODO | source | medium | authored-source | `3 related paths` | 864 review lines under android/app/src/main/java | - |
| R1-CHUNK-0392 | [ ] TODO | source | medium | authored-source | `3 related paths` | 883 review lines under android/app/src/main/java | - |
| R1-CHUNK-0393 | [ ] TODO | source | medium | authored-source | `3 related paths` | 847 review lines under android/app/src/main/java | - |
| R1-CHUNK-0394 | [ ] TODO | source | medium | authored-source | `4 related paths` | 731 review lines under android/app/src/main/java | - |
| R1-CHUNK-0395 | [ ] TODO | source | medium | authored-source | `4 related paths` | 668 review lines under android/app/src/main/java | - |
| R1-CHUNK-0396 | [ ] TODO | source | medium | authored-source | `4 related paths` | 543 review lines under android/app/src/main/java | - |
| R1-CHUNK-0397 | [ ] TODO | source | medium | authored-source | `4 related paths` | 722 review lines under android/app/src/main/java | - |
| R1-CHUNK-0398 | [ ] TODO | source | medium | authored-source | `4 related paths` | 817 review lines under android/app/src/main/java | - |
| R1-CHUNK-0399 | [ ] TODO | source | medium | authored-source | `4 related paths` | 400 review lines under android/app/src/main/java | - |
| R1-CHUNK-0400 | [ ] TODO | source | medium | authored-source | `4 related paths` | 862 review lines under android/app/src/main/java | - |
| R1-CHUNK-0401 | [ ] TODO | source | medium | authored-source | `4 related paths` | 837 review lines under android/app/src/main/java | - |
| R1-CHUNK-0402 | [ ] TODO | source | medium | authored-source | `5 related paths` | 239 review lines under android/app/src/main/java | - |
| R1-CHUNK-0403 | [ ] TODO | source | medium | authored-source | `5 related paths` | 854 review lines under android/app/src/main/java | - |
| R1-CHUNK-0404 | [ ] TODO | source | medium | authored-source | `5 related paths` | 802 review lines under android/app/src/main/java | - |
| R1-CHUNK-0405 | [ ] TODO | source | medium | authored-source | `5 related paths` | 843 review lines under android/app/src/main/java | - |
| R1-CHUNK-0406 | [ ] TODO | source | medium | authored-source | `5 related paths` | 891 review lines under android/app/src/main/java | - |
| R1-CHUNK-0407 | [ ] TODO | source | medium | authored-source | `5 related paths` | 599 review lines under android/app/src/main/java | - |
| R1-CHUNK-0408 | [ ] TODO | source | medium | authored-source | `5 related paths` | 749 review lines under android/app/src/main/java | - |
| R1-CHUNK-0409 | [ ] TODO | source | medium | authored-source | `5 related paths` | 679 review lines under android/app/src/main/java | - |
| R1-CHUNK-0410 | [ ] TODO | source | medium | authored-source | `7 related paths` | 828 review lines under android/app/src/main/java | - |
| R1-CHUNK-0411 | [ ] TODO | source | medium | authored-source | `7 related paths` | 681 review lines under android/app/src/main/java | - |
| R1-CHUNK-0412 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/AdvancedSettingsPage.kt` | L1-L900 | - |
| R1-CHUNK-0413 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/AdvancedSettingsPage.kt` | L901-L1800 | - |
| R1-CHUNK-0414 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/AutomapTouchPolicy.kt` | L1-L129 | - |
| R1-CHUNK-0415 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/AutoselectEditorPage.kt` | L1-L900 | - |
| R1-CHUNK-0416 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/ControllerConfigModel.kt` | L1-L320 | - |
| R1-CHUNK-0417 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/ControllerConfigPage.kt` | L1-L900 | - |
| R1-CHUNK-0418 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/ControllerConfigPage.kt` | L901-L1800 | - |
| R1-CHUNK-0419 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/ControllerConfigPage.kt` | L1801-L2700 | - |
| R1-CHUNK-0420 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/FileSetManager.kt` | L1-L521 | - |
| R1-CHUNK-0421 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/GameFileFormats.kt` | L1-L494 | - |
| R1-CHUNK-0422 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/LauncherScriptExecutor.kt` | L1-L900 | - |
| R1-CHUNK-0423 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/LauncherScriptExecutor.kt` | L901-L1187 | - |
| R1-CHUNK-0424 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/LevelMetadata.kt` | L1-L900 | - |
| R1-CHUNK-0425 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/LevelMetadata.kt` | L901-L1430 | - |
| R1-CHUNK-0426 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/lobby/LobbyService.kt` | L1-L900 | - |
| R1-CHUNK-0427 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/lobby/LobbyService.kt` | L901-L1482 | - |
| R1-CHUNK-0428 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/MainActivity.kt` | L1-L900 | - |
| R1-CHUNK-0429 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/MainActivity.kt` | L901-L1800 | - |
| R1-CHUNK-0430 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/MainActivity.kt` | L1801-L2700 | - |
| R1-CHUNK-0431 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/MainActivity.kt` | L2701-L3600 | - |
| R1-CHUNK-0432 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/ModManager.kt` | L1-L900 | - |
| R1-CHUNK-0433 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/ModManager.kt` | L901-L1800 | - |
| R1-CHUNK-0434 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/multiplayer/IceProgressPanel.kt` | L1-L253 | - |
| R1-CHUNK-0435 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/multiplayer/LanDiscoveryTab.kt` | L1-L900 | - |
| R1-CHUNK-0436 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/multiplayer/LocalhostProxy.kt` | L1-L559 | - |
| R1-CHUNK-0437 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/multiplayer/MatchmakingService.kt` | L1-L900 | - |
| R1-CHUNK-0438 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/multiplayer/MultiplayerScreen.kt` | L1-L900 | - |
| R1-CHUNK-0439 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/MusicControlPanel.kt` | L1-L900 | - |
| R1-CHUNK-0440 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/MusicPickerPage.kt` | L1-L900 | - |
| R1-CHUNK-0441 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/MusicPickerPage.kt` | L901-L1800 | - |
| R1-CHUNK-0442 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt` | L1-L900 | - |
| R1-CHUNK-0443 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt` | L901-L1800 | - |
| R1-CHUNK-0444 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt` | L1801-L2700 | - |
| R1-CHUNK-0445 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt` | L2701-L3600 | - |
| R1-CHUNK-0446 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt` | L3601-L4500 | - |
| R1-CHUNK-0447 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt` | L4501-L4976 | - |
| R1-CHUNK-0448 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/SetupAutomationApi.kt` | L1-L900 | - |
| R1-CHUNK-0449 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/SetupDialogs.kt` | L1-L900 | - |
| R1-CHUNK-0450 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/SetupDialogs.kt` | L901-L1799 | - |
| R1-CHUNK-0451 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/SetupDiscImport.kt` | L1-L649 | - |
| R1-CHUNK-0452 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/SetupFileImport.kt` | L1-L662 | - |
| R1-CHUNK-0453 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/SetupSaveExplorer.kt` | L1-L900 | - |
| R1-CHUNK-0454 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/SetupSaveExplorer.kt` | L901-L1196 | - |
| R1-CHUNK-0455 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/SetupSections.kt` | L1-L900 | - |
| R1-CHUNK-0456 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/SetupSections.kt` | L901-L1800 | - |
| R1-CHUNK-0457 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/SetupSections.kt` | L1801-L2700 | - |
| R1-CHUNK-0458 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/SetupSections.kt` | L2701-L3416 | - |
| R1-CHUNK-0459 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/TouchControl.kt` | L1-L886 | - |
| R1-CHUNK-0460 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/TouchEditorPage.kt` | L1-L900 | - |
| R1-CHUNK-0461 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/TouchEditorPage.kt` | L901-L1800 | - |
| R1-CHUNK-0462 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/TouchEditorPage.kt` | L1801-L2700 | - |
| R1-CHUNK-0463 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/TouchEditorPage.kt` | L2701-L3600 | - |
| R1-CHUNK-0464 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt` | L1-L900 | - |
| R1-CHUNK-0465 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt` | L901-L1800 | - |
| R1-CHUNK-0466 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt` | L1801-L2700 | - |
| R1-CHUNK-0467 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt` | L2701-L3600 | - |
| R1-CHUNK-0468 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt` | L3601-L4500 | - |
| R1-CHUNK-0469 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/VideoInfoOverlay.kt` | L1-L900 | - |
| R1-CHUNK-0470 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/WeaponWheelLabels.kt` | L1-L481 | - |
| R1-CHUNK-0471 | [ ] TODO | source | medium | authored-source | `android/app/src/main/res/xml/file_paths.xml` | L1-L9 | - |
| R1-CHUNK-0472 | [ ] TODO | source | medium | authored-source | `android/tools/etc2tool/etc2tool.cpp` | L1-L300 | - |
| R1-CHUNK-0473 | [ ] TODO | source | medium | build-script | `6 related paths` | 457 review lines under [root] | - |
| R1-CHUNK-0474 | [ ] TODO | source | medium | build-script | `run-windows-build.ps1` | L1-L493 | - |
| R1-CHUNK-0475 | [ ] TODO | source | medium | build-script | `2 related paths` | 214 review lines under android | - |
| R1-CHUNK-0476 | [ ] TODO | source | medium | build-script | `2 related paths` | 804 review lines under android | - |
| R1-CHUNK-0477 | [ ] TODO | source | medium | build-script | `2 related paths` | 514 review lines under android | - |
| R1-CHUNK-0478 | [ ] TODO | source | medium | build-script | `6 related paths` | 861 review lines under android | - |
| R1-CHUNK-0479 | [ ] TODO | source | medium | build-script | `android/PSScriptAnalyzerSettings.psd1` | L1-L63 | - |
| R1-CHUNK-0480 | [ ] TODO | source | medium | build-script | `android/run_all_tests.ps1` | L1-L900 | - |
| R1-CHUNK-0481 | [ ] TODO | source | medium | build-script | `android/run_all_tests.ps1` | L901-L1800 | - |
| R1-CHUNK-0482 | [ ] TODO | source | medium | build-script | `android/app/build.gradle` | L1-L411 | - |
| R1-CHUNK-0483 | [ ] TODO | source | medium | build-script | `2 related paths` | 210 review lines under android/docker | - |
| R1-CHUNK-0484 | [ ] TODO | source | medium | build-script | `11 related paths` | 796 review lines under android/get_deps | - |
| R1-CHUNK-0485 | [ ] TODO | source | medium | build-script | `3 related paths` | 349 review lines under android/get_deps | - |
| R1-CHUNK-0486 | [ ] TODO | source | medium | build-script | `9 related paths` | 783 review lines under android/get_deps | - |
| R1-CHUNK-0487 | [ ] TODO | source | medium | build-script | `9 related paths` | 847 review lines under android/get_deps | - |
| R1-CHUNK-0488 | [ ] TODO | source | medium | build-script | `android/get_deps/check-updates.ps1` | L1-L900 | - |
| R1-CHUNK-0489 | [ ] TODO | source | medium | build-script | `android/get_deps/check-updates.ps1` | L901-L1800 | - |
| R1-CHUNK-0490 | [ ] TODO | source | medium | build-script | `2 related paths` | 744 review lines under android/helpers | - |
| R1-CHUNK-0491 | [ ] TODO | source | medium | build-script | `3 related paths` | 622 review lines under android/helpers | - |
| R1-CHUNK-0492 | [ ] TODO | source | medium | build-script | `4 related paths` | 761 review lines under android/helpers | - |
| R1-CHUNK-0493 | [ ] TODO | source | medium | build-script | `5 related paths` | 725 review lines under android/helpers | - |
| R1-CHUNK-0494 | [ ] TODO | source | medium | build-script | `7 related paths` | 895 review lines under android/helpers | - |
| R1-CHUNK-0495 | [ ] TODO | source | medium | build-script | `7 related paths` | 857 review lines under android/helpers | - |
| R1-CHUNK-0496 | [ ] TODO | source | medium | build-script | `android/helpers/push_game_data.sh` | L1-L255 | - |
| R1-CHUNK-0497 | [ ] TODO | source | medium | build-script | `android/regression_demos/.gitignore` | L1-L4 | - |
| R1-CHUNK-0498 | [ ] TODO | source | medium | build-script | `3 related paths` | 873 review lines under android/tools | - |
| R1-CHUNK-0499 | [ ] TODO | source | medium | build-script | `5 related paths` | 290 review lines under cmake | - |
| R1-CHUNK-0500 | [ ] TODO | source | medium | build-script | `2 related paths` | 648 review lines under game_data | - |
| R1-CHUNK-0501 | [ ] TODO | source | medium | build-script | `2 related paths` | 670 review lines under game_data | - |
| R1-CHUNK-0502 | [ ] TODO | source | medium | build-script | `2 related paths` | 601 review lines under game_data | - |
| R1-CHUNK-0503 | [ ] TODO | source | medium | build-script | `2 related paths` | 447 review lines under game_data | - |
| R1-CHUNK-0504 | [ ] TODO | source | medium | build-script | `3 related paths` | 726 review lines under game_data | - |
| R1-CHUNK-0505 | [ ] TODO | source | medium | build-script | `2 related paths` | 846 review lines under game_data/mods | - |
| R1-CHUNK-0506 | [ ] TODO | source | medium | build-script | `2 related paths` | 673 review lines under game_data/mods | - |
| R1-CHUNK-0507 | [ ] TODO | source | medium | build-script | `3 related paths` | 872 review lines under game_data/mods | - |
| R1-CHUNK-0508 | [ ] TODO | source | medium | build-script | `4 related paths` | 741 review lines under game_data/mods | - |
| R1-CHUNK-0509 | [ ] TODO | source | medium | build-script | `4 related paths` | 382 review lines under game_data/mods | - |
| R1-CHUNK-0510 | [ ] TODO | source | medium | build-script | `game_data/mods/d2x-xl/convert_d2xxl_textures.ps1` | L1-L877 | - |
| R1-CHUNK-0511 | [ ] TODO | source | medium | build-script | `game_data/mods/xfing/xfing_minimal_dxa_lib.ps1` | L1-L900 | - |
| R1-CHUNK-0512 | [ ] TODO | source | medium | build-script | `game_data/mods/xfing/xfing_minimal_dxa_lib.ps1` | L901-L1126 | - |
| R1-CHUNK-0513 | [ ] TODO | source | low | documentation | `.github/copilot-instructions.md` | L1-L235 | - |
| R1-CHUNK-0514 | [ ] TODO | source | low | documentation | `2 related paths` | 5 review lines under [root] | - |
| R1-CHUNK-0515 | [ ] TODO | source | low | documentation | `AGENTS.md` | L1-L3 | - |
| R1-CHUNK-0516 | [ ] TODO | source | low | documentation | `d1_d2_ogl_diff.txt` | L1-L922 | - |
| R1-CHUNK-0517 | [ ] TODO | source | low | documentation | `3 related paths` | 263 review lines under android | - |
| R1-CHUNK-0518 | [ ] TODO | source | low | documentation | `android/app/src/main/assets/DISC_HASHING.md` | L1-L61 | - |
| R1-CHUNK-0519 | [ ] TODO | source | low | documentation | `android/get_deps/README-ubuntu.md` | L1-L25 | - |
| R1-CHUNK-0520 | [ ] TODO | source | low | documentation | `android/regression_demos/README.md` | L1-L6 | - |
| R1-CHUNK-0521 | [ ] TODO | source | low | documentation | `android/test_fixtures/SECRET_AREA_BASELINE.md` | L1-L9 | - |
| R1-CHUNK-0522 | [ ] TODO | source | low | documentation | `4 related paths` | 182 review lines under android/tests | - |
| R1-CHUNK-0523 | [ ] TODO | source | low | documentation | `2 related paths` | 264 review lines under game_data | - |
| R1-CHUNK-0524 | [ ] TODO | source | low | documentation | `game_data_to_copy_to_emulator/README.md` | L1-L13 | - |
| R1-CHUNK-0525 | [ ] TODO | source | low | documentation | `game_data/demo installers/README.md` | L1-L119 | - |
| R1-CHUNK-0526 | [ ] TODO | source | low | documentation | `3 related paths` | 66 review lines under game_data/mods | - |
| R1-CHUNK-0527 | [ ] TODO | source | low | test-source | `android/0_upload_to_test.ps1` | L1-L161 | - |
| R1-CHUNK-0528 | [ ] TODO | source | low | test-source | `11 related paths` | 1092 review lines under android/app/src/test/java | - |
| R1-CHUNK-0529 | [ ] TODO | source | low | test-source | `12 related paths` | 872 review lines under android/app/src/test/java | - |
| R1-CHUNK-0530 | [ ] TODO | source | low | test-source | `13 related paths` | 1032 review lines under android/app/src/test/java | - |
| R1-CHUNK-0531 | [ ] TODO | source | low | test-source | `5 related paths` | 1012 review lines under android/app/src/test/java | - |
| R1-CHUNK-0532 | [ ] TODO | source | low | test-source | `8 related paths` | 1047 review lines under android/app/src/test/java | - |
| R1-CHUNK-0533 | [ ] TODO | source | low | test-source | `8 related paths` | 959 review lines under android/app/src/test/java | - |
| R1-CHUNK-0534 | [ ] TODO | source | low | test-source | `9 related paths` | 1014 review lines under android/app/src/test/java | - |
| R1-CHUNK-0535 | [ ] TODO | source | low | test-source | `9 related paths` | 945 review lines under android/app/src/test/java | - |
| R1-CHUNK-0536 | [ ] TODO | source | low | test-source | `android/app/src/test/java/com/dxxredux/app/WeaponWheelLabelTest.kt` | L1-L228 | - |
| R1-CHUNK-0537 | [ ] TODO | source | low | test-source | `11 related paths` | 1046 review lines under android/game_scripts | - |
| R1-CHUNK-0538 | [ ] TODO | source | low | test-source | `13 related paths` | 1075 review lines under android/game_scripts | - |
| R1-CHUNK-0539 | [ ] TODO | source | low | test-source | `15 related paths` | 1079 review lines under android/game_scripts | - |
| R1-CHUNK-0540 | [ ] TODO | source | low | test-source | `15 related paths` | 1067 review lines under android/game_scripts | - |
| R1-CHUNK-0541 | [ ] TODO | source | low | test-source | `16 related paths` | 1094 review lines under android/game_scripts | - |
| R1-CHUNK-0542 | [ ] TODO | source | low | test-source | `6 related paths` | 447 review lines under android/game_scripts | - |
| R1-CHUNK-0543 | [ ] TODO | source | low | test-source | `2 related paths` | 504 review lines under android/helpers | - |
| R1-CHUNK-0544 | [ ] TODO | source | low | test-source | `2 related paths` | 587 review lines under android/helpers | - |
| R1-CHUNK-0545 | [ ] TODO | source | low | test-source | `android/helpers/test_helpers.ps1` | L1-L1100 | - |
| R1-CHUNK-0546 | [ ] TODO | source | low | test-source | `android/helpers/test_helpers.ps1` | L1101-L2200 | - |
| R1-CHUNK-0547 | [ ] TODO | source | low | test-source | `2 related paths` | 1032 review lines under android/tests | - |
| R1-CHUNK-0548 | [ ] TODO | source | low | test-source | `2 related paths` | 676 review lines under android/tests | - |
| R1-CHUNK-0549 | [ ] TODO | source | low | test-source | `2 related paths` | 103 review lines under android/tests | - |
| R1-CHUNK-0550 | [ ] TODO | source | low | test-source | `2 related paths` | 526 review lines under android/tests | - |
| R1-CHUNK-0551 | [ ] TODO | source | low | test-source | `3 related paths` | 537 review lines under android/tests | - |
| R1-CHUNK-0552 | [ ] TODO | source | low | test-source | `3 related paths` | 928 review lines under android/tests | - |
| R1-CHUNK-0553 | [ ] TODO | source | low | test-source | `3 related paths` | 652 review lines under android/tests | - |
| R1-CHUNK-0554 | [ ] TODO | source | low | test-source | `3 related paths` | 955 review lines under android/tests | - |
| R1-CHUNK-0555 | [ ] TODO | source | low | test-source | `4 related paths` | 918 review lines under android/tests | - |
| R1-CHUNK-0556 | [ ] TODO | source | low | test-source | `5 related paths` | 870 review lines under android/tests | - |
| R1-CHUNK-0557 | [ ] TODO | source | low | test-source | `5 related paths` | 790 review lines under android/tests | - |
| R1-CHUNK-0558 | [ ] TODO | source | low | test-source | `6 related paths` | 965 review lines under android/tests | - |
| R1-CHUNK-0559 | [ ] TODO | source | low | test-source | `6 related paths` | 996 review lines under android/tests | - |
| R1-CHUNK-0560 | [ ] TODO | source | low | test-source | `7 related paths` | 896 review lines under android/tests | - |
| R1-CHUNK-0561 | [ ] TODO | source | low | test-source | `7 related paths` | 884 review lines under android/tests | - |
| R1-CHUNK-0562 | [ ] TODO | source | low | test-source | `9 related paths` | 765 review lines under android/tests | - |
| R1-CHUNK-0563 | [ ] TODO | source | low | test-source | `android/tests/run_input_demo_regressions.ps1` | L1-L265 | - |
| R1-CHUNK-0564 | [ ] TODO | source | low | test-source | `android/tests/run_input_demo_replay.ps1` | L1-L1100 | - |
| R1-CHUNK-0565 | [ ] TODO | source | low | test-source | `android/tests/test_input_demo_fixture.cpp` | L1-L474 | - |
| R1-CHUNK-0566 | [ ] TODO | source | low | test-source | `android/tests/test_input_demo_recorder.cpp` | L1-L991 | - |
| R1-CHUNK-0567 | [ ] TODO | source | low | test-source | `android/tests/test_input_demo_replay.cpp` | L1-L995 | - |
| R1-CHUNK-0568 | [ ] TODO | source | low | test-source | `android/tests/test_lan.ps1` | L1-L1100 | - |
| R1-CHUNK-0569 | [ ] TODO | source | low | test-source | `android/tests/test_level_metadata_scan.c` | L1-L1100 | - |
| R1-CHUNK-0570 | [ ] TODO | source | low | test-source | `android/tests/test_route_snapshot.cpp` | L1-L1100 | - |
| R1-CHUNK-0571 | [ ] TODO | mechanical | mechanical | artifact | `22 paths` | batch 1 | - |
| R1-CHUNK-0572 | [ ] TODO | mechanical | mechanical | dependency-lock | `1 paths` | batch 1 | - |
| R1-CHUNK-0573 | [ ] TODO | mechanical | mechanical | generated-fixture | `20 paths` | batch 3 | - |
| R1-CHUNK-0574 | [ ] TODO | mechanical | mechanical | generated-fixture | `50 paths` | batch 1 | - |
| R1-CHUNK-0575 | [ ] TODO | mechanical | mechanical | generated-fixture | `50 paths` | batch 2 | - |
| R1-CHUNK-0576 | [ ] TODO | mechanical | mechanical | historical-plan | `22 paths` | batch 23 | - |
| R1-CHUNK-0577 | [ ] TODO | mechanical | mechanical | historical-plan | `50 paths` | batch 1 | - |
| R1-CHUNK-0578 | [ ] TODO | mechanical | mechanical | historical-plan | `50 paths` | batch 2 | - |
| R1-CHUNK-0579 | [ ] TODO | mechanical | mechanical | historical-plan | `50 paths` | batch 3 | - |
| R1-CHUNK-0580 | [ ] TODO | mechanical | mechanical | historical-plan | `50 paths` | batch 4 | - |
| R1-CHUNK-0581 | [ ] TODO | mechanical | mechanical | historical-plan | `50 paths` | batch 5 | - |
| R1-CHUNK-0582 | [ ] TODO | mechanical | mechanical | historical-plan | `50 paths` | batch 6 | - |
| R1-CHUNK-0583 | [ ] TODO | mechanical | mechanical | historical-plan | `50 paths` | batch 7 | - |
| R1-CHUNK-0584 | [ ] TODO | mechanical | mechanical | historical-plan | `50 paths` | batch 8 | - |
| R1-CHUNK-0585 | [ ] TODO | mechanical | mechanical | historical-plan | `50 paths` | batch 9 | - |
| R1-CHUNK-0586 | [ ] TODO | mechanical | mechanical | historical-plan | `50 paths` | batch 10 | - |
| R1-CHUNK-0587 | [ ] TODO | mechanical | mechanical | historical-plan | `50 paths` | batch 11 | - |
| R1-CHUNK-0588 | [ ] TODO | mechanical | mechanical | historical-plan | `50 paths` | batch 12 | - |
| R1-CHUNK-0589 | [ ] TODO | mechanical | mechanical | historical-plan | `50 paths` | batch 13 | - |
| R1-CHUNK-0590 | [ ] TODO | mechanical | mechanical | historical-plan | `50 paths` | batch 14 | - |
| R1-CHUNK-0591 | [ ] TODO | mechanical | mechanical | historical-plan | `50 paths` | batch 15 | - |
| R1-CHUNK-0592 | [ ] TODO | mechanical | mechanical | historical-plan | `50 paths` | batch 16 | - |
| R1-CHUNK-0593 | [ ] TODO | mechanical | mechanical | historical-plan | `50 paths` | batch 17 | - |
| R1-CHUNK-0594 | [ ] TODO | mechanical | mechanical | historical-plan | `50 paths` | batch 18 | - |
| R1-CHUNK-0595 | [ ] TODO | mechanical | mechanical | historical-plan | `50 paths` | batch 19 | - |
| R1-CHUNK-0596 | [ ] TODO | mechanical | mechanical | historical-plan | `50 paths` | batch 20 | - |
| R1-CHUNK-0597 | [ ] TODO | mechanical | mechanical | historical-plan | `50 paths` | batch 21 | - |
| R1-CHUNK-0598 | [ ] TODO | mechanical | mechanical | historical-plan | `50 paths` | batch 22 | - |
| R1-CHUNK-0599 | [ ] TODO | mechanical | mechanical | other-data | `21 paths` | batch 1 | - |
| R1-SWEEP-001 | [ ] TODO | sweep | high | d1-d2 | `d1/ and d2/` | Parity, minimal upstream edits, platform guards, and shared-new-code boundaries | - |
| R1-SWEEP-002 | [ ] TODO | sweep | critical | native-boundary | `Android JNI and native code` | Ownership, lifetimes, thread attachment, references, bounds, and exception paths | - |
| R1-SWEEP-003 | [ ] TODO | sweep | high | android-lifecycle | `Android Kotlin and Java` | Activity lifecycle, state restoration, cancellation, permissions, backgrounding, and touch-only operation | - |
| R1-SWEEP-004 | [ ] TODO | sweep | critical | files-data | `Import, archive, storage, config, and save paths` | Trust boundaries, traversal, size limits, transactions, schema ownership, and C source of truth | - |
| R1-SWEEP-005 | [ ] TODO | sweep | critical | server-network | `server/ and multiplayer clients` | Protocol validation, abuse cases, rate and size limits, timeouts, cleanup, deadlocks, and compatibility | - |
| R1-SWEEP-006 | [ ] TODO | sweep | high | concurrency-resources | `complete diff` | Threads, locks, cancellation, handles, memory, GL resources, and lifecycle cleanup | - |
| R1-SWEEP-007 | [ ] TODO | sweep | high | build-portability | `Build, packaging, dependency, and release files` | Pinned inputs, host preservation, ABI matrix, paths with spaces, exit codes, and reproducibility | - |
| R1-SWEEP-008 | [ ] TODO | sweep | high | tests | `Tests, automation, fixtures, and runners` | Meaningful assertions, false passes, cleanup, timeouts, determinism, and missing integration coverage | - |
| R1-SWEEP-009 | [ ] TODO | sweep | high | determinism | `Simulation, save, replay, and metadata code` | RNG ownership, floating point, serialization completeness, state restore, and demo transparency | - |
| R1-SWEEP-010 | [ ] TODO | sweep | medium | performance | `Hot paths and large-data workflows` | Per-frame work, allocations, blocking I/O, repeated parsing, caching, and unbounded growth | - |
| R1-SWEEP-011 | [ ] TODO | sweep | high | errors-logging | `Complete diff` | Fail-safe behavior, cleanup on error, actionable diagnostics, privacy, and Android debug-log routing | - |
| R1-SWEEP-012 | [ ] TODO | sweep | high | interfaces | `Cross-language and cross-process interfaces` | API, ABI, protocol, schema, duplicated constants, versioning, and compatibility | - |
| R1-SWEEP-013 | [ ] TODO | sweep | medium | maintainability | `Complete diff` | Confusing names, unnecessary layers, duplication, dead code, stale comments, and simpler alternatives | - |
| R1-SWEEP-014 | [ ] TODO | sweep | high | pr-hygiene | `Complete diff` | Unexpected artifacts, generated files, historical plans, private data, licenses, and reviewability | - |
| R1-CLOSE-001 | [ ] TODO | closure | critical | coverage | `Ledger and live branch` | Reconcile every path and chunk, validate findings, inspect head delta, and produce closure summary | - |

## Mechanical batch path lists

### R1-CHUNK-0001

- `android/auth_config.json5.template`: L1-L26
- `android/play-store-credentials.sample.json`: L1-L19

### R1-CHUNK-0002

- `game_data/CD images/d1 mac 2nd bin+cue/extract_regression.json5`: L1-L36
- `game_data/CD images/d1 mac 2nd bin+cue/track_fingerprints.json`: L1-L124
- `game_data/CD images/d1 mac 2nd bin+cue/track_hashes.json`: L1-L16
- `game_data/CD images/d2 mac/extract_regression.json5`: L1-L39
- `game_data/CD images/Descent - Anniversary Edition (Brazil) (Covermount)/extract_regression.json5`: L1-L36
- `game_data/CD images/Descent - Anniversary Edition (USA)/extract_regression.json5`: L1-L36
- `game_data/CD images/Descent - Destination Saturn (USA)/extract_regression.json5`: L1-L36
- `game_data/CD images/Descent - Levels of the World (USA)/extract_regression.json5`: L1-L33
- `game_data/CD images/Descent - Mac macplay/extract_regression.json5`: L1-L36
- `game_data/CD images/Descent - Test Flight (USA)/extract_regression.json5`: L1-L36
- `game_data/CD images/Descent (Europe) (Alt)/extract_regression.json5`: L1-L36
- `game_data/CD images/Descent (Europe)/extract_regression.json5`: L1-L36
- `game_data/CD images/Descent (USA)/extract_regression.json5`: L1-L36
- `game_data/CD images/Descent Anniversary (ISO)/extract_regression.json5`: L1-L30

### R1-CHUNK-0003

- `game_data/CD images/Descent II (USA) (Rerelease)/extract_regression.json5`: L1-L71
- `game_data/CD images/Descent II (USA) (v1.1)/extract_regression.json5`: L1-L71
- `game_data/CD images/Descent II (USA)/extract_regression.json5`: L1-L87
- `game_data/CD images/Descent II Infinite Abyss/extract_regression.json5`: L1-L71
- `game_data/CD images/Descent-II-Destination-Quartzon_Win_EN_ISO-Version/extract_regression.json5`: L1-L37
- `game_data/CD images/Descent-II-Destination-Quartzon_Win_EN_ISO-Version/track_fingerprints.json`: L1-L115
- `game_data/CD images/Dimensions for Descent (USA)/extract_regression.json5`: L1-L33

### R1-CHUNK-0004

- `game_data/CD images/Descent I and II - The Definitive Collection (Europe) (Disc 1)/extract_regression.json5`: L1-L36
- `game_data/CD images/Descent I and II - The Definitive Collection (Europe) (Disc 2)/extract_regression.json5`: L1-L71
- `game_data/CD images/Descent I and II - The Definitive Collection (Europe) (Disc 3)/extract_regression.json5`: L1-L66
- `game_data/CD images/Descent I and II - The Definitive Collection (USA) (Disc 1)/extract_regression.json5`: L1-L36
- `game_data/CD images/Descent I and II - The Definitive Collection (USA) (Disc 2)/extract_regression.json5`: L1-L71
- `game_data/CD images/Descent I and II - The Definitive Collection (USA) (Disc 3)/extract_regression.json5`: L1-L66
- `game_data/CD images/Descent II - Destination Quartzon (Europe)/extract_regression.json5`: L1-L85
- `game_data/CD images/Descent II - Destination Quartzon (USA) (Diamond OEM)/extract_regression.json5`: L1-L85

### R1-CHUNK-0005

- `game_data/CD images/Descent II - Destination Quartzon (USA) (Logitech OEM)/extract_regression.json5`: L1-L85
- `game_data/CD images/Descent II - Destination Quartzon (USA)/extract_regression.json5`: L1-L85
- `game_data/CD images/Descent II - Destination Quartzon 3D (Europe)/extract_regression.json5`: L1-L57
- `game_data/CD images/Descent II - The Vertigo Series (USA)/extract_regression.json5`: L1-L66
- `game_data/CD images/Descent II (Europe) (v1.1)/extract_regression.json5`: L1-L71
- `game_data/CD images/Descent II (Europe)/extract_regression.json5`: L1-L87
- `game_data/CD images/Descent II (USA) (3-Level Interactive Preview)/extract_regression.json5`: L1-L37
- `game_data/CD images/Descent II (USA) (Alt)/extract_regression.json5`: L1-L87

### R1-CHUNK-0006

- `game_data/music/Mission ZIP - castaway_redux/chromaprint_info.json5`: L1-L19
- `game_data/music/Mission ZIP - cererian_1.3/chromaprint_info.json5`: L1-L16
- `game_data/music/Mission ZIP - ewithin-versions/chromaprint_info.json5`: L1-L75
- `game_data/music/Mission ZIP - KCXF2RMv11/chromaprint_info.json5`: L1-L17
- `game_data/music/Mission ZIP - nefarious/chromaprint_info.json5`: L1-L10
- `game_data/music/Mission ZIP - Trine1/chromaprint_info.json5`: L1-L23
- `game_data/music/Mission ZIP - trine2/chromaprint_info.json5`: L1-L23
- `game_data/music/Mission ZIP - U3AAH/chromaprint_info.json5`: L1-L11
- `game_data/music/Mission ZIP - ulterior_v1.0.6b/chromaprint_info.json5`: L1-L47

### R1-CHUNK-0007

- `android/app/src/main/cpp/extract/cue_parser.c`: L1-L181
- `android/app/src/main/cpp/extract/cue_parser.h`: L1-L79

### R1-CHUNK-0008

- `android/app/src/main/cpp/jni_cd_preview.c`: L1-L162
- `android/app/src/main/cpp/jni_disc_import.c`: L1-L408

### R1-CHUNK-0009

- `android/app/src/main/cpp/extract/stuffit_extract.c`: L1-L452
- `android/app/src/main/cpp/extract/stuffit_extract.h`: L1-L18

### R1-CHUNK-0010

- `android/app/src/main/cpp/extract/jni_gog_import.c`: L1-L390
- `android/app/src/main/cpp/extract/mac_hfs_extract.c`: L1-L196

### R1-CHUNK-0011

- `android/app/src/main/cpp/extract/sow_extract.c`: L601-L768
- `android/app/src/main/cpp/extract/sow_extract.h`: L1-L74

### R1-CHUNK-0012

- `android/app/src/main/cpp/extract/hfs_reader.c`: L601-L726
- `android/app/src/main/cpp/extract/hfs_reader.h`: L1-L76

### R1-CHUNK-0013

- `android/app/src/main/cpp/jni_main.c`: L1201-L1410
- `android/app/src/main/cpp/jni_midi_preview.c`: L1-L124

### R1-CHUNK-0014

- `android/app/src/main/cpp/extract/sti2_extract.c`: L1801-L2117
- `android/app/src/main/cpp/extract/sti2_extract.h`: L1-L61

### R1-CHUNK-0015

- `android/app/src/main/cpp/extract/inno_reader.c`: L1801-L2202
- `android/app/src/main/cpp/extract/inno_reader.h`: L1-L148

### R1-CHUNK-0016

- `android/app/src/main/cpp/extract/fingerprint_match.c`: L1-L356
- `android/app/src/main/cpp/extract/game_file_extensions.c`: L1-L67
- `android/app/src/main/cpp/extract/game_file_extensions.h`: L1-L27

### R1-CHUNK-0017

- `android/app/src/main/cpp/extract/extract_cd.c`: L601-L647
- `android/app/src/main/cpp/extract/extract_gog.c`: L1-L205
- `android/app/src/main/cpp/extract/fingerprint_audio.c`: L1-L188

### R1-CHUNK-0018

- `android/app/src/main/cpp/jni_resume_save.cpp`: L601-L952
- `android/app/src/main/cpp/jni_saf.c`: L1-L92
- `android/app/src/main/cpp/shared/android_jni_overlay.c`: L1-L61
- `android/app/src/main/cpp/shared/android_jni_overlay.h`: L1-L18

### R1-CHUNK-0044

- `android/app/src/main/java/com/dxxredux/app/MissionZipMusicStageManager.kt`: L1-L436
- `android/app/src/main/java/com/dxxredux/app/multiplayer/PlayGamesAuth.kt`: L1-L102

### R1-CHUNK-0045

- `android/app/src/main/java/com/dxxredux/app/MissionZipMusic.kt`: L1-L417
- `android/app/src/main/java/com/dxxredux/app/MissionZipMusicNames.kt`: L1-L105

### R1-CHUNK-0046

- `android/app/src/main/java/com/dxxredux/app/ArchiveFiles.kt`: L1-L228
- `android/app/src/main/java/com/dxxredux/app/ArchiveInputStreams.kt`: L1-L28

### R1-CHUNK-0050

- `server/src/bin/nat_sim.rs`: L1-L50
- `server/src/config.rs`: L1-L220

### R1-CHUNK-0051

- `server/src/nat_sim.rs`: L1-L423
- `server/src/pow.rs`: L1-L139

### R1-CHUNK-0052

- `server/src/protocol.rs`: L1-L452
- `server/src/rate_limit.rs`: L1-L143

### R1-CHUNK-0053

- `server/src/friends.rs`: L1-L121
- `server/src/http_api.rs`: L1-L313
- `server/src/identity.rs`: L1-L121

### R1-CHUNK-0054

- `server/src/lib.rs`: L1-L47
- `server/src/lobby.rs`: L1-L231
- `server/src/main.rs`: L1-L167

### R1-CHUNK-0055

- `server/src/relay.rs`: L1-L167
- `server/src/stats.rs`: L1-L185
- `server/src/stun.rs`: L1-L107
- `server/src/tls.rs`: L1-L39

### R1-CHUNK-0062

- `android/app/src/main/cpp/extract/.gitignore`: L1-L1
- `android/app/src/main/cpp/extract/CMakeLists.txt`: L1-L319

### R1-CHUNK-0064

- `android/helpers/playstore-auth.ps1`: L1-L151
- `android/helpers/run_cue_iso_tests.sh`: L1-L32

### R1-CHUNK-0065

- `android/helpers/run_mission_zip_batch.ps1`: L601-L822
- `android/helpers/udp_relay.ps1`: L1-L82

### R1-CHUNK-0067

- `game_data/extract_all_cds.ps1`: L1-L189
- `game_data/extract_all_gog.ps1`: L1-L216

### R1-CHUNK-0073

- `android/app/src/main/cpp/extract/test_fingerprint.c`: L1-L415
- `android/app/src/main/cpp/extract/test_gog_fd.c`: L1-L100

### R1-CHUNK-0074

- `android/app/src/main/cpp/extract/test_hfs.c`: L601-L650
- `android/app/src/main/cpp/extract/test_sow_direct.c`: L1-L23

### R1-CHUNK-0075

- `android/app/src/main/cpp/extract/test_stuffit_corpus.cpp`: L601-L1045
- `android/app/src/main/cpp/extract/test_stuffit_demo_oracles.cmake`: L1-L107
- `android/app/src/main/cpp/extract/test_stuffit_direct.c`: L1-L25
- `android/app/src/main/cpp/extract/test/data/stuffit_manifests/testfile.stuffit45_dlx.mac9.sit.json`: L1-L19

### R1-CHUNK-0076

- `android/app/src/main/cpp/extract/test/data/stuffit_manifests/testfile.stuffit651_dlx.mac9.sit.json`: L1-L19
- `android/app/src/main/cpp/extract/test/data/stuffit_manifests/testfile.stuffit651_dlx.macx1.sit.json`: L1-L19
- `android/app/src/main/cpp/extract/test/data/stuffit_manifests/testfile.stuffit7_dlx.mac9.sit.json`: L1-L19
- `android/app/src/main/cpp/extract/test/data/stuffit_manifests/testfile.stuffit7_dlx.macx1.sit.json`: L1-L19
- `android/app/src/main/cpp/extract/test/data/stuffit_manifests/testfile.stuffit7.win.sit.json`: L1-L14

### R1-CHUNK-0084

- `android/app/src/test/java/com/dxxredux/app/MissionZipMusicNamesTest.kt`: L1-L122
- `android/app/src/test/java/com/dxxredux/app/MissionZipMusicProgressTest.kt`: L1-L33
- `android/app/src/test/java/com/dxxredux/app/MissionZipMusicStageManagerTest.kt`: L1-L220

### R1-CHUNK-0085

- `android/app/src/test/java/com/dxxredux/app/ArchiveInputStreamsTest.kt`: L1-L44
- `android/app/src/test/java/com/dxxredux/app/MissionArchiveProgressFormatTest.kt`: L1-L22
- `android/app/src/test/java/com/dxxredux/app/MissionZipAudioFingerprintCacheTest.kt`: L1-L208
- `android/app/src/test/java/com/dxxredux/app/MissionZipExtractionStoreTest.kt`: L1-L41
- `android/app/src/test/java/com/dxxredux/app/MissionZipMusicAnalysisSkipTest.kt`: L1-L62
- `android/app/src/test/java/com/dxxredux/app/MissionZipMusicDisplayTest.kt`: L1-L67
- `android/app/src/test/java/com/dxxredux/app/MissionZipMusicExtractedPreviewTest.kt`: L1-L65

### R1-CHUNK-0090

- `android/game_scripts/test_extract_regression_template.json5`: L1-L56
- `android/game_scripts/test_level_metadata_launcher_zip_reusable.json5`: L1-L37
- `android/game_scripts/test_mission_zip_batch_import_metadata_launch.json5`: L1-L38
- `android/game_scripts/test_mission_zip_batch_import_metadata.json5`: L1-L18

### R1-CHUNK-0091

- `android/tests/test_all_extracts.ps1`: L1-L283
- `android/tests/test_cue_iso.ps1`: L1-L38

### R1-CHUNK-0092

- `android/tests/test_validate_extract_regression_specs.ps1`: L1-L10
- `android/tests/validate_extract_regression_specs.ps1`: L1-L123

### R1-CHUNK-0093

- `android/tests/test_extract.ps1`: L1201-L1488
- `android/tests/test_mission_zip_batch.ps1`: L1-L18

### R1-CHUNK-0100

- `server/config.json5.default`: L1-L53
- `server/nginx-dxx-matchmaking.conf`: L1-L63
- `server/server_config.json5.template`: L1-L55

### R1-CHUNK-0101

- `android/app/src/main/cpp/shared/coop_indicator_lines.h`: L1-L19
- `android/app/src/main/cpp/shared/coop_start_positions.c`: L1-L79
- `android/app/src/main/cpp/shared/coop_start_positions.h`: L1-L9
- `android/app/src/main/cpp/shared/coop/coop_host_migration_policy.c`: L1-L37
- `android/app/src/main/cpp/shared/coop/coop_host_migration_policy.h`: L1-L28
- `android/app/src/main/cpp/shared/coop/coop_host_migration.c`: L1-L94
- `android/app/src/main/cpp/shared/coop/coop_host_migration.h`: L1-L7
- `android/app/src/main/cpp/shared/coop/coop_multi_status.c`: L1-L194
- `android/app/src/main/cpp/shared/coop/coop_multi_status.h`: L1-L37
- `android/app/src/main/cpp/shared/coop/coop_restore_remap.h`: L1-L18

### R1-CHUNK-0102

- `android/app/src/main/cpp/shared/gles3_shim.c`: L751-L771
- `android/app/src/main/cpp/shared/gles3_shim.h`: L1-L182
- `android/app/src/main/cpp/shared/hmp_android_shared.c`: L1-L132
- `android/app/src/main/cpp/shared/hmp_android_shared.h`: L1-L20
- `android/app/src/main/cpp/shared/homing_compat.h`: L1-L52
- `android/app/src/main/cpp/shared/hud_counts_shared.c`: L1-L172
- `android/app/src/main/cpp/shared/hud_counts_shared.h`: L1-L26
- `android/app/src/main/cpp/shared/hud_layout_shared.h`: L1-L20
- `android/app/src/main/cpp/shared/input_demo_codec.cpp`: L1-L67
- `android/app/src/main/cpp/shared/input_demo_codec.h`: L1-L24

### R1-CHUNK-0103

- `android/app/src/main/cpp/shared/android_graphics_options.c`: L1-L506
- `android/app/src/main/cpp/shared/android_graphics_options.h`: L1-L33

### R1-CHUNK-0104

- `android/app/src/main/cpp/shared/input_demo_fp_env.c`: L1-L73
- `android/app/src/main/cpp/shared/input_demo_fp_env.h`: L1-L17

### R1-CHUNK-0105

- `android/app/src/main/cpp/shared/android_log.h`: L1-L56
- `android/app/src/main/cpp/shared/android_menu_reorder.h`: L1-L81

### R1-CHUNK-0106

- `android/app/src/main/cpp/android_surface.c`: L1-L281
- `android/app/src/main/cpp/headless/d2_headless_runtime.c`: L1-L21

### R1-CHUNK-0107

- `android/app/src/main/cpp/shared/android_rewind.c`: L1-L482
- `android/app/src/main/cpp/shared/android_rewind.h`: L1-L49

### R1-CHUNK-0108

- `android/app/src/main/cpp/shared/input_demo_replay.cpp`: L751-L767
- `android/app/src/main/cpp/shared/input_demo_replay.h`: L1-L106

### R1-CHUNK-0109

- `android/app/src/main/cpp/shared/coop/coop_save.c`: L751-L1171
- `android/app/src/main/cpp/shared/coop/coop_save.h`: L1-L176

### R1-CHUNK-0110

- `android/app/src/main/cpp/shared/secret_area_scan.c`: L751-L1114
- `android/app/src/main/cpp/shared/secret_area_scan.h`: L1-L130

### R1-CHUNK-0111

- `android/app/src/main/cpp/shared/automap_metadata_overlay.c`: L751-L797
- `android/app/src/main/cpp/shared/automap_metadata_overlay.h`: L1-L29

### R1-CHUNK-0112

- `android/app/src/main/cpp/shared/input_demo_fixture.cpp`: L751-L1255
- `android/app/src/main/cpp/shared/input_demo_fixture.h`: L1-L209

### R1-CHUNK-0113

- `android/app/src/main/cpp/shared/input_demo_controls.cpp`: L751-L756
- `android/app/src/main/cpp/shared/input_demo_controls.h`: L1-L166

### R1-CHUNK-0114

- `android/app/src/main/cpp/shared/input_demo_recorder.cpp`: L751-L757
- `android/app/src/main/cpp/shared/input_demo_recorder.h`: L1-L94

### R1-CHUNK-0115

- `android/app/src/main/cpp/shared/game_automate.cpp`: L3001-L3596
- `android/app/src/main/cpp/shared/game_automate.h`: L1-L106

### R1-CHUNK-0116

- `android/app/src/main/cpp/shared/net/net_udp_android.c`: L1-L351
- `android/app/src/main/cpp/shared/net/net_udp_android.h`: L1-L87
- `android/app/src/main/cpp/shared/net/net_udp_p2p_proxy_shared.c`: L1-L302

### R1-CHUNK-0117

- `android/app/src/main/cpp/shared/fingerprint_gen.c`: L1-L144
- `android/app/src/main/cpp/shared/fingerprint_gen.h`: L1-L56
- `android/app/src/main/cpp/shared/font_control_shared.h`: L1-L15

### R1-CHUNK-0118

- `android/app/src/main/cpp/shared/android_texture_debug.c`: L1-L451
- `android/app/src/main/cpp/shared/android_texture_debug.h`: L1-L53
- `android/app/src/main/cpp/shared/android_visual_policy.h`: L1-L15

### R1-CHUNK-0119

- `android/app/src/main/cpp/shared/coop/coop_warp.c`: L1-L379
- `android/app/src/main/cpp/shared/coop/coop_warp.h`: L1-L70
- `android/app/src/main/cpp/shared/debug_log_categories.h`: L1-L20

### R1-CHUNK-0120

- `android/app/src/main/cpp/shared/route_edge.cpp`: L1-L300
- `android/app/src/main/cpp/shared/route_edge.h`: L1-L78
- `android/app/src/main/cpp/shared/route_planner_c.h`: L1-L47

### R1-CHUNK-0121

- `android/app/src/main/cpp/shared/input_demo_debug_logging.h`: L1-L182
- `android/app/src/main/cpp/shared/input_demo_direct_command_policy.c`: L1-L173
- `android/app/src/main/cpp/shared/input_demo_direct_command_policy.h`: L1-L42

### R1-CHUNK-0122

- `android/app/src/main/cpp/shared/debug_tex_overlay.h`: L1-L293
- `android/app/src/main/cpp/shared/deterministic_math.h`: L1-L13
- `android/app/src/main/cpp/shared/difficulty_runtime_shared.c`: L1-L118

### R1-CHUNK-0123

- `android/app/src/main/cpp/headless/headless_metadata_dump_main.cpp`: L751-L1052
- `android/app/src/main/cpp/headless/input_demo_headless_main.cpp`: L1-L246
- `android/app/src/main/cpp/native-lib.cpp`: L1-L16

### R1-CHUNK-0124

- `android/app/src/main/cpp/shared/secret_area_game_adapter.c`: L2251-L2483
- `android/app/src/main/cpp/shared/secret_area_item_names.c`: L1-L85
- `android/app/src/main/cpp/shared/secret_area_item_names.h`: L1-L14

### R1-CHUNK-0125

- `android/app/src/main/cpp/shared/route_planner.cpp`: L3001-L3012
- `android/app/src/main/cpp/shared/route_planner.h`: L1-L305
- `android/app/src/main/cpp/shared/route_snapshot_c.h`: L1-L52

### R1-CHUNK-0126

- `android/app/src/main/cpp/shared/android_save_meta.c`: L1-L309
- `android/app/src/main/cpp/shared/android_save_meta.h`: L1-L121
- `android/app/src/main/cpp/shared/android_save_set.c`: L1-L137
- `android/app/src/main/cpp/shared/android_save_set.h`: L1-L32

### R1-CHUNK-0127

- `android/app/src/main/cpp/shared/playsave_android_shared.c`: L1-L414
- `android/app/src/main/cpp/shared/playsave_android_shared.h`: L1-L31
- `android/app/src/main/cpp/shared/playsave_layout.c`: L1-L161
- `android/app/src/main/cpp/shared/playsave_layout.h`: L1-L27

### R1-CHUNK-0128

- `android/app/src/main/cpp/shared/merged_wall_debug.h`: L1-L270
- `android/app/src/main/cpp/shared/messagebox.c`: L1-L18
- `android/app/src/main/cpp/shared/midi_enumeration.c`: L1-L357
- `android/app/src/main/cpp/shared/midi_enumeration.h`: L1-L40

### R1-CHUNK-0129

- `android/app/src/main/cpp/shared/android_axis_mailbox.cpp`: L1-L332
- `android/app/src/main/cpp/shared/android_axis_mailbox.h`: L1-L91
- `android/app/src/main/cpp/shared/android_crash_handler.c`: L1-L284
- `android/app/src/main/cpp/shared/android_crash_handler.h`: L1-L41

### R1-CHUNK-0130

- `android/app/src/main/cpp/shared/classic_demo_json.c`: L1-L357
- `android/app/src/main/cpp/shared/classic_demo_json.h`: L1-L195
- `android/app/src/main/cpp/shared/console_ringbuf.cpp`: L1-L157
- `android/app/src/main/cpp/shared/console_ringbuf.h`: L1-L36

### R1-CHUNK-0131

- `android/app/src/main/cpp/shared/android_menu_scale.h`: L1-L83
- `android/app/src/main/cpp/shared/android_menu_touch_log.c`: L1-L55
- `android/app/src/main/cpp/shared/android_menu_touch_log.h`: L1-L56
- `android/app/src/main/cpp/shared/android_meta_actions.c`: L1-L441

### R1-CHUNK-0132

- `android/app/src/main/cpp/shared/songs_android_shared.c`: L1-L366
- `android/app/src/main/cpp/shared/songs_android_shared.h`: L1-L15
- `android/app/src/main/cpp/shared/startup_resume_shared.c`: L1-L89
- `android/app/src/main/cpp/shared/startup_resume_shared.h`: L1-L8

### R1-CHUNK-0133

- `android/app/src/main/cpp/shared/input_demo_result.h`: L1-L124
- `android/app/src/main/cpp/shared/input_demo_rng_mode.c`: L1-L176
- `android/app/src/main/cpp/shared/input_demo_rng_mode.h`: L1-L23
- `android/app/src/main/cpp/shared/input_demo_rng_trace.c`: L1-L422

### R1-CHUNK-0134

- `android/app/src/main/cpp/shared/playsave_text.c`: L1-L230
- `android/app/src/main/cpp/shared/playsave_text.h`: L1-L15
- `android/app/src/main/cpp/shared/playsave_transaction.c`: L1-L144
- `android/app/src/main/cpp/shared/playsave_transaction.h`: L1-L27

### R1-CHUNK-0135

- `android/app/src/main/cpp/shared/input_demo_state_trace.cpp`: L751-L791
- `android/app/src/main/cpp/shared/input_demo_state_trace.h`: L1-L422
- `android/app/src/main/cpp/shared/kconfig_android_shared.c`: L1-L91
- `android/app/src/main/cpp/shared/kconfig_android_shared.h`: L1-L41

### R1-CHUNK-0136

- `android/app/src/main/cpp/shared/digi_tsf_music.c`: L751-L907
- `android/app/src/main/cpp/shared/effect_runtime_shared.c`: L1-L91
- `android/app/src/main/cpp/shared/etc2_decode.c`: L1-L357
- `android/app/src/main/cpp/shared/etc2_decode.h`: L1-L17

### R1-CHUNK-0137

- `android/app/src/main/cpp/SDL_androidaudio.c`: L1-L453
- `android/app/src/main/cpp/SDL_androidaudio.h`: L1-L38
- `android/app/src/main/cpp/SDL_config_android.h`: L1-L87
- `android/app/src/main/cpp/shared/android_audio_diagnostics.c`: L1-L122
- `android/app/src/main/cpp/shared/android_audio_diagnostics.h`: L1-L16

### R1-CHUNK-0138

- `android/app/src/main/cpp/shared/android_dxxerror.h`: L1-L14
- `android/app/src/main/cpp/shared/android_egl_surface.c`: L1-L236
- `android/app/src/main/cpp/shared/android_egl_surface.h`: L1-L33
- `android/app/src/main/cpp/shared/android_font_scale.c`: L1-L148
- `android/app/src/main/cpp/shared/android_font_scale.h`: L1-L7

### R1-CHUNK-0139

- `android/app/src/main/cpp/shared/overlay_ringbuf.h`: L1-L48
- `android/app/src/main/cpp/shared/pcm_decoders.c`: L1-L276
- `android/app/src/main/cpp/shared/pcm_decoders.h`: L1-L48
- `android/app/src/main/cpp/shared/physfsx_android_shared.c`: L1-L110
- `android/app/src/main/cpp/shared/physfsx_android_shared.h`: L1-L8

### R1-CHUNK-0140

- `android/app/src/main/cpp/shared/android_level_preview.cpp`: L1-L365
- `android/app/src/main/cpp/shared/android_level_preview.h`: L1-L19
- `android/app/src/main/cpp/shared/android_loading_progress.c`: L1-L190
- `android/app/src/main/cpp/shared/android_loading_progress.h`: L1-L16
- `android/app/src/main/cpp/shared/android_log.c`: L1-L146

### R1-CHUNK-0141

- `android/app/src/main/cpp/shared/android_profile.h`: L1-L74
- `android/app/src/main/cpp/shared/android_resume_pilot.c`: L1-L157
- `android/app/src/main/cpp/shared/android_resume_pilot.h`: L1-L15
- `android/app/src/main/cpp/shared/android_rewind_policy.c`: L1-L106
- `android/app/src/main/cpp/shared/android_rewind_policy.h`: L1-L69

### R1-CHUNK-0142

- `android/app/src/main/cpp/shared/cd_preview.h`: L1-L64
- `android/app/src/main/cpp/shared/chromaprint_db.c`: L1-L369
- `android/app/src/main/cpp/shared/chromaprint_db.h`: L1-L58
- `android/app/src/main/cpp/shared/classic_demo_json_d2_snapshot.c`: L1-L171
- `android/app/src/main/cpp/shared/classic_demo_json_d2_snapshot.h`: L1-L20

### R1-CHUNK-0143

- `android/app/src/main/cpp/shared/ogl_texture_android.c`: L1-L357
- `android/app/src/main/cpp/shared/ogl_texture_android.h`: L1-L72
- `android/app/src/main/cpp/shared/ogl_viewport_android.c`: L1-L101
- `android/app/src/main/cpp/shared/ogl_viewport_android.h`: L1-L24
- `android/app/src/main/cpp/shared/overlay_ringbuf.cpp`: L1-L154

### R1-CHUNK-0144

- `android/app/src/main/cpp/shared/multi_save_transfer.c`: L751-L812
- `android/app/src/main/cpp/shared/net/auto_net.c`: L1-L207
- `android/app/src/main/cpp/shared/net/auto_net.h`: L1-L76
- `android/app/src/main/cpp/shared/net/net_udp_android_autonet_shared.c`: L1-L214
- `android/app/src/main/cpp/shared/net/net_udp_android_autonet_shared.h`: L1-L10

### R1-CHUNK-0145

- `android/app/src/main/cpp/shared/state_android_shared.c`: L751-L850
- `android/app/src/main/cpp/shared/state_android_shared.h`: L1-L47
- `android/app/src/main/cpp/shared/track_names.c`: L1-L405
- `android/app/src/main/cpp/shared/track_names.h`: L1-L57
- `android/app/src/main/cpp/shared/tsf_impl.c`: L1-L11

### R1-CHUNK-0146

- `android/app/src/main/cpp/shared/rbaudio_bin.c`: L1501-L1561
- `android/app/src/main/cpp/shared/rewind_file_compat.h`: L1-L32
- `android/app/src/main/cpp/shared/rewind_file.h`: L1-L359
- `android/app/src/main/cpp/shared/route_analysis_cache.c`: L1-L159
- `android/app/src/main/cpp/shared/route_analysis_cache.h`: L1-L65

### R1-CHUNK-0147

- `android/app/src/main/cpp/shared/android_meta_actions.h`: L1-L121
- `android/app/src/main/cpp/shared/android_music_control.h`: L1-L10
- `android/app/src/main/cpp/shared/android_newmenu_text_wrap.c`: L1-L149
- `android/app/src/main/cpp/shared/android_newmenu_text_wrap.h`: L1-L21
- `android/app/src/main/cpp/shared/android_pilot_listbox_hold.c`: L1-L266
- `android/app/src/main/cpp/shared/android_pilot_listbox_hold.h`: L1-L45

### R1-CHUNK-0148

- `android/app/src/main/cpp/shared/net/net_udp_p2p_proxy_shared.h`: L1-L16
- `android/app/src/main/cpp/shared/ogl_gpu_timer_android.c`: L1-L79
- `android/app/src/main/cpp/shared/ogl_gpu_timer_android.h`: L1-L22
- `android/app/src/main/cpp/shared/ogl_msaa_android.c`: L1-L208
- `android/app/src/main/cpp/shared/ogl_msaa_android.h`: L1-L40
- `android/app/src/main/cpp/shared/ogl_shader_runtime.c`: L1-L54
- `android/app/src/main/cpp/shared/ogl_shader_runtime.h`: L1-L12

### R1-CHUNK-0219

- `android/app/src/main/java/com/dxxredux/app/multiplayer/NetworkEventsOverlay.kt`: L1-L264
- `android/app/src/main/java/com/dxxredux/app/multiplayer/NetworkEventsPanel.kt`: L1-L168

### R1-CHUNK-0220

- `android/app/src/main/java/com/dxxredux/app/ImportStorageGuard.kt`: L1-L114
- `android/app/src/main/java/com/dxxredux/app/lobby/LobbyProtocol.kt`: L1-L328
- `android/app/src/main/java/com/dxxredux/app/multiplayer/NetworkConstants.kt`: L1-L52

### R1-CHUNK-0224

- `d1/2d/bitblt.c`: diff hunks 1-8, new L387-L412
- `d1/2d/bitmap.c`: diff hunks 1-4, new L34-L272
- `d1/2d/clip.h`: diff hunks 1-2, new L1-L146
- `d1/2d/font.c`: diff hunks 1-13, new L39-L1017
- `d1/2d/palette.c`: diff hunks 1-3, new L87-L122
- `d1/2d/rect.c`: diff hunks 1-3, new L31-L41

### R1-CHUNK-0226

- `d1/arch/include/joy.h`: diff hunks 1-5, new L20-L43
- `d1/arch/include/jukebox.h`: diff hunks 1-1, new L15-L18
- `d1/arch/include/window.h`: diff hunks 1-1, new L36-L41
- `d1/arch/ogl/gr.c`: diff hunks 1-22, new L58-L921

### R1-CHUNK-0227

- `d1/arch/ogl/ogl.c`: diff hunks 104-129, new L2955-L3763
- `d1/arch/ogl/oglprog.c`: diff hunks 1-24, new L3-L216
- `d1/arch/ogl/oglprog.h`: diff hunks 1-2, new L1-L18
- `d1/arch/sdl/digi_mixer.c`: diff hunks 1-18, new L19-L254

### R1-CHUNK-0228

- `d1/arch/sdl/digi.c`: diff hunks 1-1, new L118-L125
- `d1/arch/sdl/event.c`: diff hunks 1-11, new L17-L259
- `d1/arch/sdl/gr.c`: diff hunks 1-8, new L21-L311
- `d1/arch/sdl/joy.c`: diff hunks 1-26, new L5-L516
- `d1/arch/sdl/jukebox.c`: diff hunks 1-8, new L16-L317
- `d1/arch/sdl/mouse.c`: diff hunks 1-4, new L134-L181
- `d1/arch/sdl/timer.c`: diff hunks 1-2, new L47-L59
- `d1/arch/sdl/window.c`: diff hunks 1-3, new L96-L234
- `d1/arch/win32/include/resource.h`: diff hunks 1-2, new L1-L22

### R1-CHUNK-0231

- `d1/include/3d.h`: diff hunks 1-1, new L219-L225
- `d1/include/args.h`: diff hunks 1-2, new L30-L56
- `d1/include/console.h`: diff hunks 1-2, new L35-L37
- `d1/include/dxxerror.h`: diff hunks 1-2, new L40-L58
- `d1/include/editor/kdefs.h`: diff hunks 1-2, new L1-L332
- `d1/include/fix.h`: diff hunks 1-2, new L1-L7
- `d1/include/gr.h`: diff hunks 1-1, new L330-L330
- `d1/include/grdef.h`: diff hunks 1-2, new L1-L120
- `d1/include/ignorecase.h`: diff hunks 1-2, new L1-L80
- `d1/include/internal.h`: diff hunks 1-2, new L37-L44
- `d1/include/loadgl.h`: diff hunks 1-3, new L392-L403
- `d1/include/maths.h`: diff hunks 1-3, new L12-L56
- `d1/include/ogl_init.h`: diff hunks 1-10, new L14-L124
- `d1/include/physfsx.h`: diff hunks 1-5, new L179-L221
- `d1/include/pngfile.h`: diff hunks 1-1, new L22-L38
- `d1/include/pstypes.h`: diff hunks 1-1, new L24-L24

### R1-CHUNK-0232

- `d1/include/rbaudio.h`: diff hunks 1-1, new L62-L72
- `d1/include/xmodel.h`: diff hunks 1-2, new L1-L28

### R1-CHUNK-0233

- `d1/main/ai.c`: diff hunks 1-56, new L57-L3406
- `d1/main/ai.h`: diff hunks 1-3, new L78-L113
- `d1/main/aipath.c`: diff hunks 1-7, new L42-L1250
- `d1/main/automap.c`: diff hunks 1-35, new L63-L1319
- `d1/main/automap.h`: diff hunks 1-1, new L26-L58
- `d1/main/bm.c`: diff hunks 1-2, new L145-L163
- `d1/main/cntrlcen.c`: diff hunks 1-9, new L34-L377
- `d1/main/cntrlcen.h`: diff hunks 1-5, new L28-L99
- `d1/main/collide.c`: diff hunks 1-30, new L62-L1670
- `d1/main/collide.h`: diff hunks 1-1, new L45-L46

### R1-CHUNK-0234

- `d1/main/config.c`: diff hunks 1-19, new L36-L394
- `d1/main/config.h`: diff hunks 1-3, new L47-L59
- `d1/main/console.c`: diff hunks 1-10, new L23-L342
- `d1/main/coop_save.h`: L1-L7
- `d1/main/coop_warp.h`: L1-L7
- `d1/main/digi.h`: diff hunks 1-1, new L97-L97
- `d1/main/effects.c`: diff hunks 1-2, new L57-L99
- `d1/main/effects.h`: diff hunks 1-1, new L66-L74
- `d1/main/endlevel.c`: diff hunks 1-16, new L219-L937
- `d1/main/fireball.c`: diff hunks 1-26, new L53-L1464
- `d1/main/fuelcen.c`: diff hunks 1-3, new L37-L501
- `d1/main/fuelcen.h`: diff hunks 1-5, new L27-L162
- `d1/main/fvi.c`: diff hunks 1-5, new L35-L941
- `d1/main/fvi.h`: diff hunks 1-3, new L109-L143
- `d1/main/game.c`: diff hunks 1-44, new L32-L1596
- `d1/main/game.h`: diff hunks 1-4, new L35-L134

### R1-CHUNK-0235

- `d1/main/wall.h`: diff hunks 1-11, new L27-L279
- `d1/main/weapon.c`: diff hunks 1-3, new L48-L656
- `d1/main/weapon.h`: diff hunks 1-1, new L139-L141

### R1-CHUNK-0236

- `d1/main/playsave.c`: diff hunks 1-24, new L25-L2068
- `d1/main/playsave.h`: diff hunks 1-3, new L139-L207
- `d1/main/polyobj.c`: diff hunks 1-4, new L50-L536

### R1-CHUNK-0237

- `d1/main/net_udp.h`: diff hunks 1-5, new L1-L206
- `d1/main/newdemo.c`: diff hunks 1-13, new L28-L3421
- `d1/main/newdemo.h`: diff hunks 1-1, new L109-L110

### R1-CHUNK-0238

- `d1/main/multi.c`: diff hunks 1-55, new L62-L5703
- `d1/main/multi.h`: diff hunks 1-15, new L30-L731
- `d1/main/multibot.c`: diff hunks 1-3, new L711-L858

### R1-CHUNK-0239

- `d1/main/hud.c`: diff hunks 1-17, new L34-L175
- `d1/main/hudmsg.h`: diff hunks 1-3, new L16-L22
- `d1/main/inferno.c`: diff hunks 1-19, new L58-L543
- `d1/main/input_demo_control_info.h`: L1-L65

### R1-CHUNK-0240

- `d1/main/input_demo_hooks.c`: L751-L1292
- `d1/main/input_demo_hooks.h`: L1-L88
- `d1/main/input_demo_start.c`: L1-L71
- `d1/main/input_demo_start.h`: L1-L17

### R1-CHUNK-0241

- `d1/main/newmenu.h`: diff hunks 1-2, new L115-L155
- `d1/main/object.c`: diff hunks 1-37, new L41-L2386
- `d1/main/object.h`: diff hunks 1-5, new L105-L554
- `d1/main/physics.c`: diff hunks 1-13, new L17-L969
- `d1/main/piggy.c`: diff hunks 1-10, new L48-L1142
- `d1/main/piggy.h`: diff hunks 1-1, new L139-L139

### R1-CHUNK-0242

- `d1/main/gamecntl.c`: diff hunks 1-16, new L74-L1617
- `d1/main/gamefont.c`: diff hunks 1-1, new L118-L122
- `d1/main/gamerend.c`: diff hunks 1-12, new L59-L716
- `d1/main/gamesave.c`: diff hunks 1-16, new L24-L1432
- `d1/main/gameseg.c`: diff hunks 1-1, new L1808-L1808
- `d1/main/gameseq.c`: diff hunks 1-28, new L29-L1399
- `d1/main/gauges.c`: diff hunks 1-54, new L26-L4418

### R1-CHUNK-0243

- `d1/main/render.c`: diff hunks 1-38, new L57-L2180
- `d1/main/render.h`: diff hunks 1-1, new L46-L52
- `d1/main/scores.c`: diff hunks 1-2, new L47-L221
- `d1/main/screens.h`: diff hunks 1-1, new L121-L122
- `d1/main/secretarea.h`: L1-L42
- `d1/main/songs.c`: diff hunks 1-18, new L26-L493
- `d1/main/songs.h`: diff hunks 1-1, new L29-L29
- `d1/main/state.c`: diff hunks 1-11, new L37-L134

### R1-CHUNK-0244

- `d1/main/state.c`: diff hunks 13-87, new L1027-L2618
- `d1/main/state.h`: diff hunks 1-2, new L25-L46
- `d1/main/switch.c`: diff hunks 1-2, new L41-L151
- `d1/main/switch.h`: diff hunks 1-4, new L26-L98
- `d1/main/texmerge.c`: diff hunks 1-9, new L31-L214
- `d1/main/text.h`: diff hunks 1-5, new L442-L699
- `d1/main/titles.c`: diff hunks 1-18, new L55-L1264
- `d1/main/wall.c`: diff hunks 1-3, new L37-L101

### R1-CHUNK-0245

- `d1/main/kconfig.c`: diff hunks 1-34, new L56-L2084
- `d1/main/kconfig.h`: diff hunks 1-1, new L67-L73
- `d1/main/laser.c`: diff hunks 1-26, new L45-L1639
- `d1/main/laser.h`: diff hunks 1-3, new L71-L108
- `d1/main/menu.c`: diff hunks 1-38, new L59-L2274
- `d1/main/menu.h`: diff hunks 1-1, new L25-L25
- `d1/main/mglobal.c`: diff hunks 1-1, new L95-L97
- `d1/main/mission.c`: diff hunks 1-2, new L116-L337
- `d1/main/mission.h`: diff hunks 1-3, new L44-L72

### R1-CHUNK-0253

- `d1/misc/args.c`: diff hunks 1-8, new L146-L274
- `d1/misc/error.c`: diff hunks 1-3, new L23-L85
- `d1/misc/hmp.c`: diff hunks 1-2, new L16-L791
- `d1/misc/physfsx.c`: diff hunks 1-3, new L22-L284

### R1-CHUNK-0255

- `d1/xmodel/strfunc.h`: diff hunks 1-2, new L1-L93
- `d1/xmodel/tga.cpp`: diff hunks 1-1, new L742-L742
- `d1/xmodel/xmodel.cpp`: diff hunks 1-5, new L68-L218
- `d1/xmodel/xmodelnames.h`: diff hunks 1-2, new L1-L157

### R1-CHUNK-0256

- `d2/2d/bitblt.c`: diff hunks 1-9, new L30-L412
- `d2/2d/bitmap.c`: diff hunks 1-4, new L34-L276
- `d2/2d/clip.h`: diff hunks 1-2, new L1-L145
- `d2/2d/font.c`: diff hunks 1-13, new L39-L1017
- `d2/2d/palette.c`: diff hunks 1-3, new L40-L162
- `d2/2d/rect.c`: diff hunks 1-3, new L31-L41

### R1-CHUNK-0258

- `d2/arch/ogl/oglprog.c`: diff hunks 1-24, new L3-L216
- `d2/arch/ogl/oglprog.h`: diff hunks 1-2, new L1-L18
- `d2/arch/sdl/digi_mixer.c`: diff hunks 1-17, new L19-L251
- `d2/arch/sdl/digi.c`: diff hunks 1-1, new L121-L128
- `d2/arch/sdl/event.c`: diff hunks 1-11, new L17-L259
- `d2/arch/sdl/gr.c`: diff hunks 1-6, new L21-L312
- `d2/arch/sdl/joy.c`: diff hunks 1-28, new L5-L515
- `d2/arch/sdl/jukebox.c`: diff hunks 1-8, new L15-L315
- `d2/arch/sdl/mouse.c`: diff hunks 1-5, new L134-L198
- `d2/arch/sdl/timer.c`: diff hunks 1-2, new L47-L59
- `d2/arch/sdl/window.c`: diff hunks 1-3, new L96-L234
- `d2/arch/win32/include/resource.h`: diff hunks 1-2, new L1-L22

### R1-CHUNK-0259

- `d2/arch/cocoa/SDLMain.h`: diff hunks 1-2, new L1-L16
- `d2/arch/include/android_surface.h`: L1-L23
- `d2/arch/include/joy.h`: diff hunks 1-5, new L20-L43
- `d2/arch/include/jukebox.h`: diff hunks 1-1, new L15-L18
- `d2/arch/include/mouse.h`: diff hunks 1-1, new L44-L44
- `d2/arch/include/window.h`: diff hunks 1-1, new L36-L41
- `d2/arch/ogl/gr.c`: diff hunks 1-22, new L58-L928

### R1-CHUNK-0263

- `d2/include/3d.h`: diff hunks 1-1, new L193-L199
- `d2/include/args.h`: diff hunks 1-2, new L27-L54
- `d2/include/console.h`: diff hunks 1-2, new L35-L37
- `d2/include/dxxerror.h`: diff hunks 1-4, new L27-L60
- `d2/include/editor/kdefs.h`: diff hunks 1-2, new L1-L341
- `d2/include/fix.h`: diff hunks 1-2, new L1-L7
- `d2/include/gr.h`: diff hunks 1-1, new L317-L317
- `d2/include/grdef.h`: diff hunks 1-2, new L1-L63
- `d2/include/ignorecase.h`: diff hunks 1-2, new L1-L80
- `d2/include/internal.h`: diff hunks 1-2, new L37-L44
- `d2/include/interp.h`: diff hunks 1-1, new L20-L23
- `d2/include/loadgl.h`: diff hunks 1-8, new L462-L495
- `d2/include/maths.h`: diff hunks 1-3, new L12-L56
- `d2/include/ogl_init.h`: diff hunks 1-10, new L14-L124
- `d2/include/physfsx.h`: diff hunks 1-5, new L179-L221
- `d2/include/pngfile.h`: diff hunks 1-1, new L22-L38

### R1-CHUNK-0264

- `d2/include/pstypes.h`: diff hunks 1-1, new L24-L24
- `d2/include/rbaudio.h`: diff hunks 1-1, new L62-L72
- `d2/include/replay_debug_overlay.h`: L1-L27
- `d2/include/xmodel.h`: diff hunks 1-2, new L1-L28

### R1-CHUNK-0266

- `d2/main/state.c`: diff hunks 112-119, new L3210-L3426
- `d2/main/state.h`: diff hunks 1-2, new L24-L47
- `d2/main/switch.c`: diff hunks 1-15, new L47-L662
- `d2/main/switch.h`: diff hunks 1-6, new L26-L138
- `d2/main/texmerge.c`: diff hunks 1-9, new L30-L213
- `d2/main/text.h`: diff hunks 1-4, new L466-L746
- `d2/main/titles.c`: diff hunks 1-25, new L55-L1591
- `d2/main/wall.c`: diff hunks 1-14, new L35-L1404
- `d2/main/wall.h`: diff hunks 1-13, new L27-L312
- `d2/main/weapon.c`: diff hunks 1-13, new L69-L1529
- `d2/main/weapon.h`: diff hunks 1-1, new L182-L184

### R1-CHUNK-0267

- `d2/main/d1_in_d2.c`: L1501-L1849
- `d2/main/d1_in_d2.h`: L1-L63

### R1-CHUNK-0268

- `d2/main/input_demo_hooks.c`: L6751-L7118
- `d2/main/input_demo_hooks.h`: L1-L332

### R1-CHUNK-0269

- `d2/main/physics.c`: diff hunks 1-34, new L13-L1160
- `d2/main/piggy.c`: diff hunks 1-38, new L54-L2209
- `d2/main/piggy.h`: diff hunks 1-6, new L47-L131

### R1-CHUNK-0270

- `d2/main/controls.c`: diff hunks 1-10, new L40-L279
- `d2/main/coop_save.h`: L1-L7
- `d2/main/coop_warp.h`: L1-L7

### R1-CHUNK-0271

- `d2/main/ai.h`: diff hunks 1-5, new L106-L319
- `d2/main/ai2.c`: diff hunks 1-88, new L60-L2590
- `d2/main/aipath.c`: diff hunks 1-44, new L42-L1650

### R1-CHUNK-0272

- `d2/main/d1_save_translate.c`: L751-L1310
- `d2/main/d1_save_translate.h`: L1-L104
- `d2/main/digi.h`: diff hunks 1-1, new L99-L99

### R1-CHUNK-0273

- `d2/main/playsave.c`: diff hunks 1-25, new L54-L1815
- `d2/main/playsave.h`: diff hunks 1-3, new L132-L200
- `d2/main/polyobj.c`: diff hunks 1-6, new L43-L552
- `d2/main/powerup.c`: diff hunks 1-10, new L50-L395

### R1-CHUNK-0274

- `d2/main/multi.c`: diff hunks 1-67, new L67-L7542
- `d2/main/multi.h`: diff hunks 1-17, new L63-L797
- `d2/main/multibot.c`: diff hunks 1-15, new L48-L1342
- `d2/main/multibot.h`: diff hunks 1-1, new L55-L55

### R1-CHUNK-0275

- `d2/main/input_demo_start.c`: L1-L80
- `d2/main/input_demo_start.h`: L1-L21
- `d2/main/kconfig.c`: diff hunks 1-37, new L56-L2136
- `d2/main/kconfig.h`: diff hunks 1-1, new L71-L92

### R1-CHUNK-0276

- `d2/main/gamerend.c`: diff hunks 1-23, new L53-L1216
- `d2/main/gamesave.c`: diff hunks 1-20, new L24-L1635
- `d2/main/gameseg.c`: diff hunks 1-1, new L1890-L1890
- `d2/main/gameseq.c`: diff hunks 1-47, new L29-L1949

### R1-CHUNK-0277

- `d2/main/newmenu.c`: diff hunks 130-131, new L3306-L3350
- `d2/main/newmenu.h`: diff hunks 1-5, new L115-L181
- `d2/main/object.c`: diff hunks 1-69, new L70-L3057
- `d2/main/object.h`: diff hunks 1-7, new L108-L588

### R1-CHUNK-0278

- `d2/main/collide.c`: diff hunks 1-103, new L60-L3317
- `d2/main/collide.h`: diff hunks 1-1, new L46-L48
- `d2/main/config.c`: diff hunks 1-19, new L36-L417
- `d2/main/config.h`: diff hunks 1-3, new L48-L61
- `d2/main/console.c`: diff hunks 1-10, new L23-L343

### R1-CHUNK-0279

- `d2/main/automap.c`: diff hunks 1-78, new L63-L1842
- `d2/main/automap.h`: diff hunks 1-2, new L27-L63
- `d2/main/bm.c`: diff hunks 1-5, new L56-L243
- `d2/main/cntrlcen.c`: diff hunks 1-10, new L47-L449
- `d2/main/cntrlcen.h`: diff hunks 1-6, new L28-L118

### R1-CHUNK-0280

- `d2/main/gauges.c`: diff hunks 1-61, new L39-L5040
- `d2/main/hud.c`: diff hunks 1-18, new L34-L180
- `d2/main/hudmsg.h`: diff hunks 1-3, new L16-L22
- `d2/main/inferno.c`: diff hunks 1-36, new L40-L635
- `d2/main/input_demo_control_info.h`: L1-L73
- `d2/main/input_demo_energy_trace.h`: L1-L64

### R1-CHUNK-0281

- `d2/main/d1_custom.c`: L751-L831
- `d2/main/d1_custom.h`: L1-L26
- `d2/main/d1_in_d2_input_demo.c`: L1-L22
- `d2/main/d1_in_d2_input_demo.h`: L1-L15
- `d2/main/d1_in_d2_semantics.c`: L1-L40
- `d2/main/d1_in_d2_semantics.h`: L1-L18

### R1-CHUNK-0282

- `d2/main/render.c`: diff hunks 1-46, new L54-L2599
- `d2/main/render.h`: diff hunks 1-1, new L48-L55
- `d2/main/scores.c`: diff hunks 1-3, new L47-L249
- `d2/main/secretarea.h`: L1-L42
- `d2/main/songs.c`: diff hunks 1-25, new L26-L521
- `d2/main/songs.h`: diff hunks 1-1, new L29-L29
- `d2/main/state.c`: diff hunks 1-11, new L37-L153

### R1-CHUNK-0283

- `d2/main/game.c`: diff hunks 1-72, new L31-L2156
- `d2/main/game.h`: diff hunks 1-4, new L35-L148
- `d2/main/gamecntl.c`: diff hunks 1-32, new L46-L2317
- `d2/main/gamefont.c`: diff hunks 1-1, new L117-L121
- `d2/main/gamemine.c`: diff hunks 1-2, new L46-L50
- `d2/main/gamepal.c`: diff hunks 1-2, new L63-L103
- `d2/main/gamepal.h`: diff hunks 1-1, new L26-L26

### R1-CHUNK-0284

- `d2/main/escort.h`: diff hunks 1-3, new L9-L121
- `d2/main/fireball.c`: diff hunks 1-47, new L46-L1798
- `d2/main/fireball.h`: diff hunks 1-2, new L24-L77
- `d2/main/fuelcen.c`: diff hunks 1-3, new L34-L523
- `d2/main/fuelcen.h`: diff hunks 1-6, new L26-L174
- `d2/main/fvi.c`: diff hunks 1-10, new L37-L1262
- `d2/main/fvi.h`: diff hunks 1-3, new L55-L89

### R1-CHUNK-0285

- `d2/main/laser.h`: diff hunks 1-3, new L102-L153
- `d2/main/lighting.c`: diff hunks 1-3, new L44-L537
- `d2/main/menu.c`: diff hunks 1-41, new L59-L2317
- `d2/main/menu.h`: diff hunks 1-1, new L25-L25
- `d2/main/mglobal.c`: diff hunks 1-1, new L96-L98
- `d2/main/mission.c`: diff hunks 1-12, new L39-L1148
- `d2/main/mission.h`: diff hunks 1-3, new L44-L99
- `d2/main/movie.c`: diff hunks 1-18, new L55-L514

### R1-CHUNK-0286

- `d2/main/dxa_metadata_patch.cpp`: L751-L942
- `d2/main/dxa_metadata_patch.h`: L1-L7
- `d2/main/effects.c`: diff hunks 1-2, new L57-L99
- `d2/main/effects.h`: diff hunks 1-1, new L66-L74
- `d2/main/endlevel.c`: diff hunks 1-21, new L48-L1085
- `d2/main/escort_exit_policy.h`: L1-L15
- `d2/main/escort_owner_policy.c`: L1-L93
- `d2/main/escort_owner_policy.h`: L1-L35
- `d2/main/escort.c`: diff hunks 1-3, new L59-L125

### R1-CHUNK-0317

- `d2/misc/args.c`: diff hunks 1-8, new L147-L282
- `d2/misc/error.c`: diff hunks 1-3, new L23-L85
- `d2/misc/hmp.c`: diff hunks 1-2, new L16-L790
- `d2/misc/physfsx.c`: diff hunks 1-3, new L22-L298

### R1-CHUNK-0319

- `d2/xmodel/strfunc.h`: diff hunks 1-2, new L1-L93
- `d2/xmodel/tga.cpp`: diff hunks 1-1, new L742-L742
- `d2/xmodel/xmodel.cpp`: diff hunks 1-5, new L60-L218
- `d2/xmodel/xmodelnames.h`: diff hunks 1-2, new L1-L317

### R1-CHUNK-0326

- `d1/arch/cocoa/CMakeLists.txt`: diff hunks 1-1, new L1-L1
- `d1/arch/ogl/CMakeLists.txt`: diff hunks 1-4, new L1-L21
- `d1/arch/sdl/CMakeLists.txt`: diff hunks 1-7, new L1-L40
- `d1/arch/win32/CMakeLists.txt`: diff hunks 1-3, new L1-L8
- `d1/arch/x11/CMakeLists.txt`: diff hunks 1-3, new L1-L8

### R1-CHUNK-0339

- `d2/arch/cocoa/CMakeLists.txt`: diff hunks 1-1, new L1-L1
- `d2/arch/ogl/CMakeLists.txt`: diff hunks 1-4, new L1-L21
- `d2/arch/sdl/CMakeLists.txt`: diff hunks 1-7, new L1-L40
- `d2/arch/win32/CMakeLists.txt`: diff hunks 1-2, new L1-L8
- `d2/arch/x11/CMakeLists.txt`: diff hunks 1-2, new L1-L8

### R1-CHUNK-0350

- `server/.gitignore`: L1-L13
- `server/deploy_build.sh`: L1-L281
- `server/deploy_service.sh`: L1-L66
- `server/generate_lan_cert.sh`: L1-L48
- `server/run_nat_tests.ps1`: L1-L17
- `server/rust_dependency_lint.sh`: L1-L22
- `server/rust_lint.sh`: L1-L19
- `server/rust_upgrade.sh`: L1-L24
- `server/update_all.sh`: L1-L17

### R1-CHUNK-0354

- `android/app/src/test/java/com/dxxredux/app/ImportStorageGuardTest.kt`: L1-L31
- `android/app/src/test/java/com/dxxredux/app/LobbyProtocolStartOptionsTest.kt`: L1-L51

### R1-CHUNK-0355

- `android/tests/test_playsave_layout.c`: L1-L131
- `android/tests/test_playsave_text.c`: L1-L112
- `android/tests/test_playsave_transaction.c`: L1-L107

### R1-CHUNK-0361

- `.vscode/c_cpp_properties.json`: L1-L63
- `.vscode/extensions.json`: L1-L18
- `.vscode/settings.json`: L1-L43

### R1-CHUNK-0362

- `android/acoustid_config.json5.example`: L1-L17
- `android/keystore.properties.example`: L1-L15

### R1-CHUNK-0363

- `android/app/src/main/assets/known_discs.json5`: L901-L1418
- `android/app/src/main/assets/known_versions.json5`: L1-L250

### R1-CHUNK-0364

- `android/app/src/main/assets/configs/controller/default.json`: L1-L33
- `android/app/src/main/assets/configs/touch/advanced.json`: L1-L226
- `android/app/src/main/assets/configs/touch/claw.json`: L1-L205
- `android/app/src/main/assets/configs/touch/controller_menus.json`: L1-L41
- `android/app/src/main/assets/configs/touch/simple.json`: L1-L84
- `android/app/src/main/assets/fingerprint_config.json5`: L1-L15

### R1-CHUNK-0367

- `game_data/CD images/d2 mac/track_fingerprints.json`: L1-L142
- `game_data/CD images/d2 mac/track_hashes.json`: L1-L18
- `game_data/CD images/Descent - Anniversary Edition (Brazil) (Covermount)/track_fingerprints.json`: L1-L3
- `game_data/CD images/Descent - Anniversary Edition (USA)/track_fingerprints.json`: L1-L3
- `game_data/CD images/Descent - Destination Saturn (USA)/track_fingerprints.json`: L1-L3
- `game_data/CD images/Descent - Levels of the World (USA)/track_fingerprints.json`: L1-L3
- `game_data/CD images/Descent - Mac macplay/track_fingerprints.json`: L1-L124
- `game_data/CD images/Descent - Mac macplay/track_hashes.json`: L1-L16
- `game_data/CD images/Descent - Test Flight (USA)/track_fingerprints.json`: L1-L3
- `game_data/CD images/Descent (Europe) (Alt)/track_fingerprints.json`: L1-L3
- `game_data/CD images/Descent (Europe)/track_fingerprints.json`: L1-L3
- `game_data/CD images/Descent (USA)/track_fingerprints.json`: L1-L3
- `game_data/CD images/Descent I and II - The Definitive Collection (Europe) (Disc 1)/track_fingerprints.json`: L1-L3
- `game_data/CD images/Descent I and II - The Definitive Collection (Europe) (Disc 2)/track_fingerprints.json`: L1-L79
- `game_data/CD images/Descent I and II - The Definitive Collection (Europe) (Disc 3)/track_fingerprints.json`: L1-L70
- `game_data/CD images/Descent I and II - The Definitive Collection (USA) (Disc 1)/track_fingerprints.json`: L1-L3

### R1-CHUNK-0368

- `game_data/CD images/Descent II (Europe)/track_fingerprints.json`: L1-L115
- `game_data/CD images/Descent II (USA) (3-Level Interactive Preview)/track_fingerprints.json`: L1-L3
- `game_data/CD images/Descent II (USA) (Alt)/track_fingerprints.json`: L1-L115
- `game_data/CD images/Descent II (USA) (Rerelease)/track_fingerprints.json`: L1-L79
- `game_data/CD images/Descent II (USA) (v1.1)/track_fingerprints.json`: L1-L79
- `game_data/CD images/Descent II (USA)/track_fingerprints.json`: L1-L115
- `game_data/CD images/Descent II Infinite Abyss/track_fingerprints.json`: L1-L79
- `game_data/CD images/Descent II Infinite Abyss/track_hashes.json`: L1-L12
- `game_data/CD images/Dimensions for Descent (USA)/track_fingerprints.json`: L1-L3

### R1-CHUNK-0369

- `game_data/CD images/Descent I and II - The Definitive Collection (USA) (Disc 2)/track_fingerprints.json`: L1-L79
- `game_data/CD images/Descent I and II - The Definitive Collection (USA) (Disc 3)/track_fingerprints.json`: L1-L70
- `game_data/CD images/Descent II - Destination Quartzon (Europe)/track_fingerprints.json`: L1-L115
- `game_data/CD images/Descent II - Destination Quartzon (USA) (Diamond OEM)/track_fingerprints.json`: L1-L115
- `game_data/CD images/Descent II - Destination Quartzon (USA) (Logitech OEM)/track_fingerprints.json`: L1-L115
- `game_data/CD images/Descent II - Destination Quartzon (USA)/track_fingerprints.json`: L1-L115
- `game_data/CD images/Descent II - Destination Quartzon 3D (Europe)/track_fingerprints.json`: L1-L46
- `game_data/CD images/Descent II - The Vertigo Series (USA)/track_fingerprints.json`: L1-L70
- `game_data/CD images/Descent II (Europe) (v1.1)/track_fingerprints.json`: L1-L79

### R1-CHUNK-0371

- `game_data/gog installers/descent_2_enUS_1_0_51877_regression.json5`: L1-L30
- `game_data/gog installers/descent_enUS_1_0_35122_regression.json5`: L1-L27
- `game_data/gog installers/setup_descent_1.4a_(16596)_regression.json5`: L1-L27
- `game_data/gog installers/setup_descent_2_1.1_(16596)_regression.json5`: L1-L30

### R1-CHUNK-0372

- `game_data/music/D1 macplay mp3/chromaprint_info.json5`: L1-L20
- `game_data/music/D1 MIDI mp3 ARACHNO/chromaprint_info.json5`: L1-L34
- `game_data/music/D1 MIDI mp3 MU80/chromaprint_info.json5`: L1-L34
- `game_data/music/D1 MIDI mp3 opl3/chromaprint_info.json5`: L1-L38
- `game_data/music/D1 MIDI mp3 sc55/chromaprint_info.json5`: L1-L60
- `game_data/music/D1 MIDI mp3 SC88/chromaprint_info.json5`: L1-L34
- `game_data/music/D1 MIDI mp3 SC88Pro/chromaprint_info.json5`: L1-L34
- `game_data/music/D1 MIDI mp3/chromaprint_info.json5`: L1-L34
- `game_data/music/D1 playstation mp3/chromaprint_info.json5`: L1-L24
- `game_data/music/D1 sunspire remix/chromaprint_info.json5`: L1-L9
- `game_data/music/D2 infinite abyss redbook mp3/chromaprint_info.json5`: L1-L15
- `game_data/music/D2 macplay mp3/chromaprint_info.json5`: L1-L22
- `game_data/music/D2 MIDI mp3 ARACHNO/chromaprint_info.json5`: L1-L16
- `game_data/music/D2 MIDI mp3 MU80/chromaprint_info.json5`: L1-L16
- `game_data/music/D2 midi mp3 opl3/chromaprint_info.json5`: L1-L14
- `game_data/music/D2 midi mp3 sc55/chromaprint_info.json5`: L1-L26

### R1-CHUNK-0373

- `game_data/music/D2 MIDI mp3 SC88/chromaprint_info.json5`: L1-L16
- `game_data/music/D2 MIDI mp3 SC88Pro/chromaprint_info.json5`: L1-L16
- `game_data/music/D2 mp3/chromaprint_info.json5`: L1-L30
- `game_data/music/D2 redbook mp3 rips/chromaprint_info.json5`: L1-L21
- `game_data/music/D2 vampyro mp3/chromaprint_info.json5`: L1-L10
- `game_data/music/D2 vertigo mp3/chromaprint_info.json5`: L1-L14
- `game_data/music/Descent Maximum (ps1) mp3/chromaprint_info.json5`: L1-L22

### R1-CHUNK-0376

- `android/app/src/main/java/com/dxxredux/app/SetupGameFiles.kt`: L1-L419
- `android/app/src/main/java/com/dxxredux/app/SetupResumePanel.kt`: L1-L466

### R1-CHUNK-0377

- `android/app/src/main/java/com/dxxredux/app/AudioFilePreviewDialog.kt`: L1-L196
- `android/app/src/main/java/com/dxxredux/app/AudioSourceManager.kt`: L1-L579

### R1-CHUNK-0378

- `android/app/src/main/java/com/dxxredux/app/SaveExplorerBridge.kt`: L1-L178
- `android/app/src/main/java/com/dxxredux/app/ScrollIndicators.kt`: L1-L115

### R1-CHUNK-0379

- `android/app/src/main/java/com/dxxredux/app/CrashLog.kt`: L1-L382
- `android/app/src/main/java/com/dxxredux/app/CustomAudioSetManager.kt`: L1-L374

### R1-CHUNK-0380

- `android/app/src/main/java/com/dxxredux/app/EnginePreferencesPage.kt`: L1-L641
- `android/app/src/main/java/com/dxxredux/app/ExitButtonView.kt`: L1-L101

### R1-CHUNK-0381

- `android/app/src/main/java/com/dxxredux/app/FingerprintBridge.kt`: L1-L468
- `android/app/src/main/java/com/dxxredux/app/GameActivityState.kt`: L1-L76

### R1-CHUNK-0382

- `android/app/src/main/java/com/dxxredux/app/GraphicsSettingsPage.kt`: L1-L677
- `android/app/src/main/java/com/dxxredux/app/GyroInputManager.kt`: L1-L184

### R1-CHUNK-0383

- `android/app/src/main/java/com/dxxredux/app/multiplayer/CreateGameDialog.kt`: L1-L523
- `android/app/src/main/java/com/dxxredux/app/multiplayer/FriendsTab.kt`: L1-L291

### R1-CHUNK-0384

- `android/app/src/main/java/com/dxxredux/app/HumanReadableConfig.kt`: L1-L679
- `android/app/src/main/java/com/dxxredux/app/ImportChooserConfig.kt`: L1-L19

### R1-CHUNK-0385

- `android/app/src/main/java/com/dxxredux/app/SetupAutomationApi.kt`: L901-L1045
- `android/app/src/main/java/com/dxxredux/app/SetupConfigFiles.kt`: L1-L240

### R1-CHUNK-0386

- `android/app/src/main/java/com/dxxredux/app/multiplayer/LanDiscoveryTab.kt`: L901-L1244
- `android/app/src/main/java/com/dxxredux/app/multiplayer/LobbyScreen.kt`: L1-L435

### R1-CHUNK-0387

- `android/app/src/main/java/com/dxxredux/app/multiplayer/MultiplayerScreen.kt`: L901-L1333
- `android/app/src/main/java/com/dxxredux/app/multiplayer/MultiplayerStatsOverlay.kt`: L1-L415

### R1-CHUNK-0388

- `android/app/src/main/java/com/dxxredux/app/MusicControlPanel.kt`: L901-L906
- `android/app/src/main/java/com/dxxredux/app/MusicOverlaySourceOption.kt`: L1-L25

### R1-CHUNK-0389

- `android/app/src/main/java/com/dxxredux/app/ControllerLongPressDetector.kt`: L1-L245
- `android/app/src/main/java/com/dxxredux/app/ControllerOverlayNavigation.kt`: L1-L64
- `android/app/src/main/java/com/dxxredux/app/CoopStatsOverlay.kt`: L1-L314

### R1-CHUNK-0390

- `android/app/src/main/java/com/dxxredux/app/SkipButtonView.kt`: L1-L278
- `android/app/src/main/java/com/dxxredux/app/StartGameButtonView.kt`: L1-L104
- `android/app/src/main/java/com/dxxredux/app/TouchBindings.kt`: L1-L490

### R1-CHUNK-0391

- `android/app/src/main/java/com/dxxredux/app/ConfigImportExport.kt`: L1-L509
- `android/app/src/main/java/com/dxxredux/app/ConfigSlotDialog.kt`: L1-L326
- `android/app/src/main/java/com/dxxredux/app/ConfigSlotModel.kt`: L1-L29

### R1-CHUNK-0392

- `android/app/src/main/java/com/dxxredux/app/multiplayer/MatchmakingService.kt`: L901-L1309
- `android/app/src/main/java/com/dxxredux/app/multiplayer/MatchmakingState.kt`: L1-L235
- `android/app/src/main/java/com/dxxredux/app/multiplayer/MissionPicker.kt`: L1-L239

### R1-CHUNK-0393

- `android/app/src/main/java/com/dxxredux/app/AdvancedSettingsPage.kt`: L1801-L2375
- `android/app/src/main/java/com/dxxredux/app/AndroidGameFileExtensions.kt`: L1-L17
- `android/app/src/main/java/com/dxxredux/app/AssetManifest.kt`: L1-L255

### R1-CHUNK-0394

- `android/app/src/main/java/com/dxxredux/app/GameFileMetadata.kt`: L1-L594
- `android/app/src/main/java/com/dxxredux/app/GamepadButtonEdgeTracker.kt`: L1-L19
- `android/app/src/main/java/com/dxxredux/app/GogImportBridge.kt`: L1-L115
- `android/app/src/main/java/com/dxxredux/app/GraphicsDebugPrefs.kt`: L1-L3

### R1-CHUNK-0395

- `android/app/src/main/java/com/dxxredux/app/DebugLog.kt`: L1-L355
- `android/app/src/main/java/com/dxxredux/app/DebugLogCategory.kt`: L1-L22
- `android/app/src/main/java/com/dxxredux/app/DemoInstallerPackages.kt`: L1-L112
- `android/app/src/main/java/com/dxxredux/app/DiscIdentifier.kt`: L1-L179

### R1-CHUNK-0396

- `android/app/src/main/java/com/dxxredux/app/AcceptJoinButtonView.kt`: L1-L112
- `android/app/src/main/java/com/dxxredux/app/AcoustIdClient.kt`: L1-L155
- `android/app/src/main/java/com/dxxredux/app/AcoustIdPrefs.kt`: L1-L3
- `android/app/src/main/java/com/dxxredux/app/AdminTrayPolicy.kt`: L1-L273

### R1-CHUNK-0397

- `android/app/src/main/java/com/dxxredux/app/multiplayer/MultiplayerCallsigns.kt`: L1-L265
- `android/app/src/main/java/com/dxxredux/app/multiplayer/MultiplayerForegroundService.kt`: L1-L74
- `android/app/src/main/java/com/dxxredux/app/multiplayer/MultiplayerResumeOffer.kt`: L1-L60
- `android/app/src/main/java/com/dxxredux/app/multiplayer/MultiplayerResumePrefs.kt`: L1-L323

### R1-CHUNK-0398

- `android/app/src/main/java/com/dxxredux/app/DiscImportBridge.kt`: L1-L353
- `android/app/src/main/java/com/dxxredux/app/DpadFocusUtils.kt`: L1-L231
- `android/app/src/main/java/com/dxxredux/app/DxaTextureScanner.kt`: L1-L171
- `android/app/src/main/java/com/dxxredux/app/DxxReduxApp.kt`: L1-L62

### R1-CHUNK-0399

- `android/app/src/main/java/com/dxxredux/app/AutoselectEditorPage.kt`: L901-L913
- `android/app/src/main/java/com/dxxredux/app/BinHexDecoder.kt`: L1-L100
- `android/app/src/main/java/com/dxxredux/app/CdAudioSourceNormalization.kt`: L1-L154
- `android/app/src/main/java/com/dxxredux/app/CdPreviewBridge.kt`: L1-L133

### R1-CHUNK-0400

- `android/app/src/main/java/com/dxxredux/app/ControllerConfigPage.kt`: L2701-L2819
- `android/app/src/main/java/com/dxxredux/app/ControllerConfigSlotRepository.kt`: L1-L199
- `android/app/src/main/java/com/dxxredux/app/ControllerConfigStore.kt`: L1-L501
- `android/app/src/main/java/com/dxxredux/app/ControllerDisplayDevice.kt`: L1-L43

### R1-CHUNK-0401

- `android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt`: L4501-L5072
- `android/app/src/main/java/com/dxxredux/app/TvButtons.kt`: L1-L114
- `android/app/src/main/java/com/dxxredux/app/TvDetection.kt`: L1-L16
- `android/app/src/main/java/com/dxxredux/app/UpdateChecker.kt`: L1-L135

### R1-CHUNK-0402

- `android/app/src/main/java/com/dxxredux/app/Json5.kt`: L1-L26
- `android/app/src/main/java/com/dxxredux/app/KnownVersions.kt`: L1-L84
- `android/app/src/main/java/com/dxxredux/app/LauncherDebugLog.kt`: L1-L17
- `android/app/src/main/java/com/dxxredux/app/LauncherFileCopy.kt`: L1-L93
- `android/app/src/main/java/com/dxxredux/app/LauncherFileLabels.kt`: L1-L19

### R1-CHUNK-0403

- `android/app/src/main/java/com/dxxredux/app/multiplayer/NetLog.kt`: L1-L73
- `android/app/src/main/java/com/dxxredux/app/multiplayer/RecentAddressPrefs.kt`: L1-L74
- `android/app/src/main/java/com/dxxredux/app/multiplayer/StunClient.kt`: L1-L251
- `android/app/src/main/java/com/dxxredux/app/multiplayer/TvButtons.kt`: L1-L115
- `android/app/src/main/java/com/dxxredux/app/multiplayer/UpnpClient.kt`: L1-L341

### R1-CHUNK-0404

- `android/app/src/main/java/com/dxxredux/app/PendingResumeLaunch.kt`: L1-L162
- `android/app/src/main/java/com/dxxredux/app/RemainingTouchActions.kt`: L1-L215
- `android/app/src/main/java/com/dxxredux/app/ResumeSaveBridge.kt`: L1-L176
- `android/app/src/main/java/com/dxxredux/app/SafManifest.kt`: L1-L125
- `android/app/src/main/java/com/dxxredux/app/SafUriPermissions.kt`: L1-L124

### R1-CHUNK-0405

- `android/app/src/main/java/com/dxxredux/app/LevelPreviewActivity.kt`: L1-L497
- `android/app/src/main/java/com/dxxredux/app/LevelPreviewRequestStore.kt`: L1-L147
- `android/app/src/main/java/com/dxxredux/app/LevelPreviewReturnRefreshGate.kt`: L1-L18
- `android/app/src/main/java/com/dxxredux/app/LoadingProgressOverlayView.kt`: L1-L166
- `android/app/src/main/java/com/dxxredux/app/lobby/LobbyDiagnostics.kt`: L1-L15

### R1-CHUNK-0406

- `android/app/src/main/java/com/dxxredux/app/ImportLocationManager.kt`: L1-L361
- `android/app/src/main/java/com/dxxredux/app/ImportTreeScanner.kt`: L1-L84
- `android/app/src/main/java/com/dxxredux/app/InputDemoManager.kt`: L1-L286
- `android/app/src/main/java/com/dxxredux/app/InputDemoPrefs.kt`: L1-L3
- `android/app/src/main/java/com/dxxredux/app/InputMixer.kt`: L1-L157

### R1-CHUNK-0407

- `android/app/src/main/java/com/dxxredux/app/VideoInfoOverlay.kt`: L901-L988
- `android/app/src/main/java/com/dxxredux/app/VisualReplacementPolicy.kt`: L1-L119
- `android/app/src/main/java/com/dxxredux/app/WarpButtonOverlay.kt`: L1-L188
- `android/app/src/main/java/com/dxxredux/app/WeaponAmmoStatus.kt`: L1-L127
- `android/app/src/main/java/com/dxxredux/app/WeaponState.kt`: L1-L77

### R1-CHUNK-0408

- `android/app/src/main/java/com/dxxredux/app/TouchEditorPage.kt`: L3601-L3715
- `android/app/src/main/java/com/dxxredux/app/TouchLayoutRepository.kt`: L1-L372
- `android/app/src/main/java/com/dxxredux/app/TouchLayoutSlotRepository.kt`: L1-L196
- `android/app/src/main/java/com/dxxredux/app/TouchMouseAcceleration.kt`: L1-L54
- `android/app/src/main/java/com/dxxredux/app/TouchOverlayColors.kt`: L1-L12

### R1-CHUNK-0409

- `android/app/src/main/java/com/dxxredux/app/MainActivity.kt`: L3601-L3871
- `android/app/src/main/java/com/dxxredux/app/MetadataLoadProgress.kt`: L1-L59
- `android/app/src/main/java/com/dxxredux/app/MidiBytesPreviewDialog.kt`: L1-L202
- `android/app/src/main/java/com/dxxredux/app/MidiEnumerationBridge.kt`: L1-L50
- `android/app/src/main/java/com/dxxredux/app/MidiPreviewBridge.kt`: L1-L97

### R1-CHUNK-0410

- `android/app/src/main/java/com/dxxredux/app/MusicPickerPage.kt`: L1801-L1851
- `android/app/src/main/java/com/dxxredux/app/NativeAutoselectPatcher.kt`: L1-L144
- `android/app/src/main/java/com/dxxredux/app/NativeMetaActions.kt`: L1-L27
- `android/app/src/main/java/com/dxxredux/app/NativePilotPatcher.kt`: L1-L141
- `android/app/src/main/java/com/dxxredux/app/NativePilotPreferences.kt`: L1-L358
- `android/app/src/main/java/com/dxxredux/app/NativeTextureLookupCache.kt`: L1-L35
- `android/app/src/main/java/com/dxxredux/app/OwnedCacheDirectories.kt`: L1-L72

### R1-CHUNK-0411

- `android/app/src/main/java/com/dxxredux/app/ModManager.kt`: L1801-L1884
- `android/app/src/main/java/com/dxxredux/app/multiplayer/ActiveGamesTab.kt`: L1-L89
- `android/app/src/main/java/com/dxxredux/app/multiplayer/ChatArea.kt`: L1-L99
- `android/app/src/main/java/com/dxxredux/app/multiplayer/ClientIdentity.kt`: L1-L35
- `android/app/src/main/java/com/dxxredux/app/multiplayer/ConnectivityChecker.kt`: L1-L168
- `android/app/src/main/java/com/dxxredux/app/multiplayer/ControllerFocus.kt`: L1-L196
- `android/app/src/main/java/com/dxxredux/app/multiplayer/CoopDesyncLog.kt`: L1-L10

### R1-CHUNK-0473

- `.clang-format`: L1-L46
- `.cmake-format.yaml`: L1-L43
- `.editorconfig`: L1-L43
- `.gitattributes`: L1-L2
- `.gitignore`: diff hunks 1-2, new L61-L128
- `run-linux-build.sh`: L1-L257

### R1-CHUNK-0475

- `android/Run-TestMenu.ps1`: L1-L178
- `android/settings.gradle`: L1-L36

### R1-CHUNK-0476

- `android/run-code-quality.ps1`: L1-L503
- `android/Run-Emulator.ps1`: L1-L301

### R1-CHUNK-0477

- `android/run_all_tests.ps1`: L1801-L1852
- `android/run_quick_tests.ps1`: L1-L462

### R1-CHUNK-0478

- `android/.gitignore`: L1-L1
- `android/1_build-aab.ps1`: L1-L152
- `android/2_deploy-playstore.ps1`: L1-L431
- `android/build.gradle`: L1-L5
- `android/gradlew.bat`: L1-L69
- `android/install-aab.ps1`: L1-L203

### R1-CHUNK-0483

- `android/docker/nat-testbed/docker-compose.yml`: L1-L43
- `android/docker/nat-testbed/nat_proxy.py`: L1-L167

### R1-CHUNK-0484

- `android/get_deps/helpers/get_emulator.sh`: L1-L56
- `android/get_deps/helpers/get_fpcalc.ps1`: L1-L69
- `android/get_deps/helpers/get_gradle_wrapper.sh`: L1-L56
- `android/get_deps/helpers/get_imagemagick.ps1`: L1-L73
- `android/get_deps/helpers/get_jdk.sh`: L1-L78
- `android/get_deps/helpers/get_ktlint.sh`: L1-L44
- `android/get_deps/helpers/get_linux_android_prereqs.sh`: L1-L6
- `android/get_deps/helpers/get_linux_build_prereqs.sh`: L1-L228
- `android/get_deps/helpers/get_ndk.sh`: L1-L49
- `android/get_deps/helpers/get_powershell_ubuntu.sh`: L1-L6
- `android/get_deps/helpers/get_powershell.ps1`: L1-L131

### R1-CHUNK-0485

- `android/get_deps/helpers/platform.sh`: L1-L147
- `android/get_deps/helpers/resolve_dep_base.sh`: L1-L58
- `android/get_deps/update-powershell.ps1`: L1-L144

### R1-CHUNK-0486

- `android/get_deps/helpers/get_powershell.sh`: L1-L185
- `android/get_deps/helpers/get_rust.sh`: L1-L68
- `android/get_deps/helpers/get_sdk.sh`: L1-L86
- `android/get_deps/helpers/get_shellcheck.sh`: L1-L61
- `android/get_deps/helpers/get_shfmt.sh`: L1-L48
- `android/get_deps/helpers/get_soundfont.sh`: L1-L43
- `android/get_deps/helpers/get_stuffit.sh`: L1-L45
- `android/get_deps/helpers/get_unar.sh`: L1-L70
- `android/get_deps/helpers/Get-DepPlatform.ps1`: L1-L177

### R1-CHUNK-0487

- `android/get_deps/check-updates.ps1`: L1801-L1830
- `android/get_deps/get_all.sh`: L1-L144
- `android/get_deps/helpers/create_avd.sh`: L1-L113
- `android/get_deps/helpers/create_light_avds.ps1`: L1-L140
- `android/get_deps/helpers/finalize.sh`: L1-L70
- `android/get_deps/helpers/get_clang_format.sh`: L1-L89
- `android/get_deps/helpers/get_cmake_format.sh`: L1-L111
- `android/get_deps/helpers/get_cmake.sh`: L1-L62
- `android/get_deps/helpers/get_dosbox.sh`: L1-L88

### R1-CHUNK-0490

- `android/helpers/regenerate_all_mission_metadata_host.ps1`: L1-L690
- `android/helpers/regenerate_all_mission_metadata.ps1`: L1-L54

### R1-CHUNK-0491

- `android/helpers/strip_input_demo_frame_state.ps1`: L1-L189
- `android/helpers/summarize-profiling-log.ps1`: L1-L389
- `android/helpers/teardown_docker_nat.ps1`: L1-L44

### R1-CHUNK-0492

- `android/helpers/generate-stuffit-corpus-manifests.ps1`: L1-L340
- `android/helpers/introspect.sh`: L1-L148
- `android/helpers/kill-stale-emulators.ps1`: L1-L162
- `android/helpers/normalize_json.py`: L1-L111

### R1-CHUNK-0493

- `android/helpers/run_automation.sh`: L1-L222
- `android/helpers/run-clang-format.ps1`: L1-L140
- `android/helpers/run-cmake-format.ps1`: L1-L126
- `android/helpers/run-cmake-lint.ps1`: L1-L109
- `android/helpers/run-ktlint.ps1`: L1-L128

### R1-CHUNK-0494

- `android/helpers/build.sh`: L1-L23
- `android/helpers/collect_crash.ps1`: L1-L143
- `android/helpers/diff_vs_upstream.ps1`: L1-L87
- `android/helpers/emu_health.ps1`: L1-L420
- `android/helpers/gather-warnings-msvc.ps1`: L1-L83
- `android/helpers/gather-warnings.ps1`: L1-L91
- `android/helpers/generate-keystore.ps1`: L1-L48

### R1-CHUNK-0495

- `android/helpers/run-psscriptanalyzer.ps1`: L1-L181
- `android/helpers/run-shellcheck.ps1`: L1-L120
- `android/helpers/run-shfmt.ps1`: L1-L130
- `android/helpers/set_vars.sh`: L1-L54
- `android/helpers/setup_docker_nat.ps1`: L1-L165
- `android/helpers/standard_game_data.ps1`: L1-L56
- `android/helpers/stop-stale-formatters.ps1`: L1-L151

### R1-CHUNK-0498

- `android/tools/decode_object_packets.ps1`: L1-L415
- `android/tools/decode_object_packets.py`: L1-L385
- `android/tools/etc2tool/CMakeLists.txt`: L1-L73

### R1-CHUNK-0499

- `cmake/input-demo-build-metadata.cmake`: L1-L53
- `cmake/input-demo-codec-deps.cmake`: L1-L32
- `cmake/ndk-auto.cmake`: L1-L98
- `cmake/platform-math.cmake`: L1-L5
- `cmake/vcpkg-auto.cmake`: L1-L102

### R1-CHUNK-0500

- `game_data/update_known_discs_albums.ps1`: L1-L507
- `game_data/update_known_discs_fingerprints.ps1`: L1-L141

### R1-CHUNK-0501

- `game_data/fingerprint_disc_tracks.ps1`: L1-L263
- `game_data/fingerprint_music_packs.ps1`: L1-L407

### R1-CHUNK-0502

- `game_data/hash_assets.ps1`: L1-L351
- `game_data/hash_disc_tracks.ps1`: L1-L250

### R1-CHUNK-0503

- `game_data/manage_data.ps1`: L1-L356
- `game_data/update_all_fingerprints.ps1`: L1-L91

### R1-CHUNK-0504

- `game_data/generate_game_data_index.ps1`: L1-L269
- `game_data/generate_regression_specs.ps1`: L1-L398
- `game_data/generate_source_manifest.ps1`: L1-L59

### R1-CHUNK-0505

- `game_data/mods/xfing/d2x_sp/convert-uud2sp-ham-patch-dxa.ps1`: L1-L256
- `game_data/mods/xfing/d2x_sp/uud2sp_ham_patch_lib.ps1`: L1-L590

### R1-CHUNK-0506

- `game_data/mods/xfing/convert-xfing-minimal-dxa.ps1`: L1-L669
- `game_data/mods/xfing/d2x_sp/.gitignore`: L1-L4

### R1-CHUNK-0507

- `game_data/mods/xfing/d2x_sp/verify-uud2sp-ham-patch-dxa.ps1`: L1-L195
- `game_data/mods/xfing/d2x_sp/verify-uud2sp-uud2tp-ham-composition.ps1`: L1-L406
- `game_data/mods/xfing/verify-xfing-minimal-dxa.ps1`: L1-L271

### R1-CHUNK-0508

- `game_data/mods/.gitignore`: L1-L6
- `game_data/mods/d2x-xl/.gitignore`: L1-L4
- `game_data/mods/d2x-xl/convert_all.ps1`: L1-L403
- `game_data/mods/d2x-xl/convert_d2xxl_sounds.ps1`: L1-L328

### R1-CHUNK-0509

- `game_data/mods/d2x-xl/convert_progress_helpers.ps1`: L1-L72
- `game_data/mods/d2x-xl/d2xxl_pack_lib.ps1`: L1-L107
- `game_data/mods/d2x-xl/repack_d2xxl_docs_and_layout.ps1`: L1-L195
- `game_data/mods/xfing/.gitignore`: L1-L8

### R1-CHUNK-0514

- `README.md`: diff hunks 1-1, new L61-L64
- `test.txt`: L1-L1

### R1-CHUNK-0517

- `android/outstanding_bugs.md`: L1-L227
- `android/privacy_policy.md`: L1-L6
- `android/README.md`: L1-L30

### R1-CHUNK-0522

- `android/tests/HOST_INPUT_DEMO_REPLAY.md`: L1-L52
- `android/tests/INPUT_DEMO_DESYNC_ANALYSIS.md`: L1-L123
- `android/tests/input_demo_determinism_fixtures.txt`: L1-L4
- `android/tests/input_demo_graphics_canaries.txt`: L1-L3

### R1-CHUNK-0523

- `game_data/game_data_index.txt`: L1-L24
- `game_data/SOURCE_FILES.txt`: L1-L240

### R1-CHUNK-0526

- `game_data/mods/d2x-xl/README.md`: L1-L21
- `game_data/mods/README.md`: L1-L26
- `game_data/mods/xfing/README.md`: L1-L19

### R1-CHUNK-0528

- `android/app/src/test/java/com/dxxredux/app/AdminTrayUiTest.kt`: L1-L296
- `android/app/src/test/java/com/dxxredux/app/AdvancedPageLoadProgressTest.kt`: L1-L30
- `android/app/src/test/java/com/dxxredux/app/AndroidGameFileExtensionsTest.kt`: L1-L22
- `android/app/src/test/java/com/dxxredux/app/AssetManifestTest.kt`: L1-L78
- `android/app/src/test/java/com/dxxredux/app/AudioSourceManagerArtifactPathsTest.kt`: L1-L124
- `android/app/src/test/java/com/dxxredux/app/AudioSourceManagerPersistenceTest.kt`: L1-L46
- `android/app/src/test/java/com/dxxredux/app/AutomapTouchPolicyTest.kt`: L1-L116
- `android/app/src/test/java/com/dxxredux/app/AutoselectReorderScrollTest.kt`: L1-L84
- `android/app/src/test/java/com/dxxredux/app/BinHexDecoderTest.kt`: L1-L66
- `android/app/src/test/java/com/dxxredux/app/CdAudioSourceNormalizationTest.kt`: L1-L84
- `android/app/src/test/java/com/dxxredux/app/CdAudioSourceVisibilityTest.kt`: L1-L146

### R1-CHUNK-0529

- `android/app/src/test/java/com/dxxredux/app/ImportLocationMigrateTest.kt`: L1-L158
- `android/app/src/test/java/com/dxxredux/app/ImportTreeScannerTest.kt`: L1-L77
- `android/app/src/test/java/com/dxxredux/app/InputDemoManagerTest.kt`: L1-L107
- `android/app/src/test/java/com/dxxredux/app/InputMixerTest.kt`: L1-L83
- `android/app/src/test/java/com/dxxredux/app/LauncherFileCopyTest.kt`: L1-L33
- `android/app/src/test/java/com/dxxredux/app/LauncherFileLabelsTest.kt`: L1-L51
- `android/app/src/test/java/com/dxxredux/app/LauncherFocusPolicyTest.kt`: L1-L73
- `android/app/src/test/java/com/dxxredux/app/LevelMetadataAnalysisSingleFlightTest.kt`: L1-L32
- `android/app/src/test/java/com/dxxredux/app/LevelMetadataCheckpointProgressTest.kt`: L1-L53
- `android/app/src/test/java/com/dxxredux/app/LevelMetadataProgressDeadlineTest.kt`: L1-L69
- `android/app/src/test/java/com/dxxredux/app/LevelMetadataResultTest.kt`: L1-L58
- `android/app/src/test/java/com/dxxredux/app/LevelMetadataRouteNumberingTest.kt`: L1-L78

### R1-CHUNK-0530

- `android/app/src/test/java/com/dxxredux/app/ConfigSlotRepositoryTest.kt`: L1-L87
- `android/app/src/test/java/com/dxxredux/app/ControllerAxisExponentTest.kt`: L1-L54
- `android/app/src/test/java/com/dxxredux/app/ControllerConfigSerializationTest.kt`: L1-L31
- `android/app/src/test/java/com/dxxredux/app/ControllerDeviceSelectionTest.kt`: L1-L38
- `android/app/src/test/java/com/dxxredux/app/ControllerLongPressDetectorTest.kt`: L1-L139
- `android/app/src/test/java/com/dxxredux/app/ControllerMenuCycleTest.kt`: L1-L151
- `android/app/src/test/java/com/dxxredux/app/ControllerMenuTouchOverlayDefaultTest.kt`: L1-L65
- `android/app/src/test/java/com/dxxredux/app/CustomAudioSetManagerTest.kt`: L1-L46
- `android/app/src/test/java/com/dxxredux/app/DemoInstallerOfferTest.kt`: L1-L62
- `android/app/src/test/java/com/dxxredux/app/DemoInstallerPackagesTest.kt`: L1-L64
- `android/app/src/test/java/com/dxxredux/app/DiscImportHoistTest.kt`: L1-L56
- `android/app/src/test/java/com/dxxredux/app/DxaTextureScannerTest.kt`: L1-L126
- `android/app/src/test/java/com/dxxredux/app/GameFileFormatsTest.kt`: L1-L113

### R1-CHUNK-0531

- `android/app/src/test/java/com/dxxredux/app/RemainingKeyTouchActionsTest.kt`: L1-L487
- `android/app/src/test/java/com/dxxredux/app/ResumeSavePanelTest.kt`: L1-L166
- `android/app/src/test/java/com/dxxredux/app/RewindPreferencePolicyTest.kt`: L1-L18
- `android/app/src/test/java/com/dxxredux/app/SafUriPermissionsTest.kt`: L1-L76
- `android/app/src/test/java/com/dxxredux/app/SaveExplorerTest.kt`: L1-L265

### R1-CHUNK-0532

- `android/app/src/test/java/com/dxxredux/app/GameFileMetadataTest.kt`: L1-L286
- `android/app/src/test/java/com/dxxredux/app/GamepadButtonDispatchPolicyTest.kt`: L1-L76
- `android/app/src/test/java/com/dxxredux/app/GamepadButtonEdgeTrackerTest.kt`: L1-L34
- `android/app/src/test/java/com/dxxredux/app/GogAudioSourceRegistrationTest.kt`: L1-L52
- `android/app/src/test/java/com/dxxredux/app/GraphicsConfigHelpersTest.kt`: L1-L70
- `android/app/src/test/java/com/dxxredux/app/GuidebotLockedWheelTest.kt`: L1-L167
- `android/app/src/test/java/com/dxxredux/app/GyroToggleConfigTest.kt`: L1-L334
- `android/app/src/test/java/com/dxxredux/app/ImportChooserConfigTest.kt`: L1-L28

### R1-CHUNK-0533

- `android/app/src/test/java/com/dxxredux/app/MouseModeTuningTest.kt`: L1-L91
- `android/app/src/test/java/com/dxxredux/app/multiplayer/MultiplayerCallsignsTest.kt`: L1-L58
- `android/app/src/test/java/com/dxxredux/app/multiplayer/MultiplayerControllerFocusPolicyTest.kt`: L1-L67
- `android/app/src/test/java/com/dxxredux/app/multiplayer/MultiplayerResumePrefsTest.kt`: L1-L350
- `android/app/src/test/java/com/dxxredux/app/MusicLaunchPolicyTest.kt`: L1-L105
- `android/app/src/test/java/com/dxxredux/app/MusicOverlaySourcesTest.kt`: L1-L64
- `android/app/src/test/java/com/dxxredux/app/OverlayVisibilityPolicyTest.kt`: L1-L167
- `android/app/src/test/java/com/dxxredux/app/OwnedCacheDirectoriesTest.kt`: L1-L57

### R1-CHUNK-0534

- `android/app/src/test/java/com/dxxredux/app/LevelMetadataTargetsTest.kt`: L1-L239
- `android/app/src/test/java/com/dxxredux/app/LevelPreviewRequestStoreTest.kt`: L1-L92
- `android/app/src/test/java/com/dxxredux/app/LevelPreviewReturnRefreshGateTest.kt`: L1-L35
- `android/app/src/test/java/com/dxxredux/app/LoadingProgressOverlayLayoutTest.kt`: L1-L38
- `android/app/src/test/java/com/dxxredux/app/LobbyDiagnosticsTest.kt`: L1-L52
- `android/app/src/test/java/com/dxxredux/app/MainActivityInputTypeTest.kt`: L1-L85
- `android/app/src/test/java/com/dxxredux/app/MetadataLoadProgressTest.kt`: L1-L46
- `android/app/src/test/java/com/dxxredux/app/MissionDescriptorFileDetailsTest.kt`: L1-L61
- `android/app/src/test/java/com/dxxredux/app/ModManagerDetailsTest.kt`: L1-L366

### R1-CHUNK-0535

- `android/app/src/test/java/com/dxxredux/app/SettingsChildOverlayControllerTest.kt`: L1-L105
- `android/app/src/test/java/com/dxxredux/app/SetupLaunchReadinessTest.kt`: L1-L189
- `android/app/src/test/java/com/dxxredux/app/TouchAxisRegionTravelTest.kt`: L1-L30
- `android/app/src/test/java/com/dxxredux/app/TouchCheatInjectionTest.kt`: L1-L112
- `android/app/src/test/java/com/dxxredux/app/TouchEditorZoneEdgeTest.kt`: L1-L113
- `android/app/src/test/java/com/dxxredux/app/TouchOverlayDragZonePolicyTest.kt`: L1-L127
- `android/app/src/test/java/com/dxxredux/app/TouchStickExtremeActionTest.kt`: L1-L161
- `android/app/src/test/java/com/dxxredux/app/VideoInfoOverlayLayoutTest.kt`: L1-L76
- `android/app/src/test/java/com/dxxredux/app/WeaponStateTest.kt`: L1-L32

### R1-CHUNK-0537

- `android/game_scripts/test_guidebot_hud_progress_layout.json5`: L1-L59
- `android/game_scripts/test_guidebot_unexplored_goal.json5`: L1-L90
- `android/game_scripts/test_intro_skip_inputs_unified.json5`: L1-L45
- `android/game_scripts/test_joystick_menu.json5`: L1-L60
- `android/game_scripts/test_kcxf2_guidebot_blastable_wall_next.json5`: L1-L90
- `android/game_scripts/test_kcxf2_guidebot_hidden_door_next.json5`: L1-L132
- `android/game_scripts/test_kcxf2_guidebot_route_next.json5`: L1-L180
- `android/game_scripts/test_kcxf2_level5_guidebot_route.json5`: L1-L119
- `android/game_scripts/test_kcxf2_level6_objectives.json5`: L1-L109
- `android/game_scripts/test_keyboard_defaults.json5`: L1-L96
- `android/game_scripts/test_keyboard_manual.json5`: L1-L66

### R1-CHUNK-0538

- `android/game_scripts/.gitignore`: L1-L2
- `android/game_scripts/profile_uneasy4_level_preview.json5`: L1-L16
- `android/game_scripts/test_abort_game_to_main_menu_d2.json5`: L1-L83
- `android/game_scripts/test_android_saveload_dispatch_unified.json5`: L1-L91
- `android/game_scripts/test_autosave_resume_missing_pilot_unified.json5`: L1-L112
- `android/game_scripts/test_autoselect_crash_unified.json5`: L1-L168
- `android/game_scripts/test_axis_mapping.json5`: L1-L247
- `android/game_scripts/test_button_discovery.json5`: L1-L23
- `android/game_scripts/test_controller_compare_unified.json5`: L1-L98
- `android/game_scripts/test_controls_readability_d2.json5`: L1-L117
- `android/game_scripts/test_coop_guidebot_observer_host.json5`: L1-L32
- `android/game_scripts/test_coop_guidebot_observer_joiner.json5`: L1-L30
- `android/game_scripts/test_coop_guidebot_owner_host.json5`: L1-L56

### R1-CHUNK-0539

- `android/game_scripts/test_obsidian_flythrough_guidebot_goal.json5`: L1-L57
- `android/game_scripts/test_obsidian_guidebot_route_next.json5`: L1-L142
- `android/game_scripts/test_obsidian_level1_next_objectives.json5`: L1-L62
- `android/game_scripts/test_obsidian_level1_objective_markers.json5`: L1-L108
- `android/game_scripts/test_obsidian_level2_route.json5`: L1-L82
- `android/game_scripts/test_obsidian_unresolved_switch_frontier.json5`: L1-L54
- `android/game_scripts/test_ogl_runtime_texture_options_unified.json5`: L1-L79
- `android/game_scripts/test_pause_menu_return.json5`: L1-L75
- `android/game_scripts/test_pause_menu_viewport_d2.json5`: L1-L56
- `android/game_scripts/test_pilot_long_hold_delete_unified.json5`: L1-L65
- `android/game_scripts/test_quick_record_classic_sidecar.json5`: L1-L88
- `android/game_scripts/test_random_level_preview.json5`: L1-L19
- `android/game_scripts/test_readable_tiny_help_d2.json5`: L1-L61
- `android/game_scripts/test_resolution_unified.json5`: L1-L95
- `android/game_scripts/test_reticle_options_stage_d2.json5`: L1-L36

### R1-CHUNK-0540

- `android/game_scripts/test_coop_guidebot_owner_joiner.json5`: L1-L61
- `android/game_scripts/test_coop_guidebot_restore_remap_host.json5`: L1-L22
- `android/game_scripts/test_coop_guidebot_restore_remap_joiner.json5`: L1-L21
- `android/game_scripts/test_coop_guidebot_restore_save_host.json5`: L1-L40
- `android/game_scripts/test_coop_guidebot_restore_save_joiner.json5`: L1-L24
- `android/game_scripts/test_coop_host_migration_prepare_host.json5`: L1-L28
- `android/game_scripts/test_coop_host_migration_prepare_joiner.json5`: L1-L25
- `android/game_scripts/test_counterstrike_level2_switch_completion.json5`: L1-L65
- `android/game_scripts/test_death.json5`: L1-L94
- `android/game_scripts/test_door45_cover_gpu_regression.json5`: L1-L92
- `android/game_scripts/test_engine_prefs_unified.json5`: L1-L109
- `android/game_scripts/test_fire_primary.json5`: L1-L74
- `android/game_scripts/test_fire_secondary.json5`: L1-L80
- `android/game_scripts/test_gog_installer_d1_unified.json5`: L1-L135
- `android/game_scripts/test_gog_installer_redbook_unified.json5`: L1-L197

### R1-CHUNK-0541

- `android/game_scripts/test_keyboard_viewport.json5`: L1-L82
- `android/game_scripts/test_launch_to_automap.json5`: L1-L164
- `android/game_scripts/test_launcher_graphics_debug_prefs.json5`: L1-L66
- `android/game_scripts/test_level_metadata_launcher_base_d2.json5`: L1-L22
- `android/game_scripts/test_level_metadata_launcher_d2_obsidian.json5`: L1-L24
- `android/game_scripts/test_levelcomplete_touch_skip.json5`: L1-L60
- `android/game_scripts/test_lunar_series_revamped_metadata_only.json5`: L1-L15
- `android/game_scripts/test_menu_scale_d2.json5`: L1-L48
- `android/game_scripts/test_merged_wall_snapshot_regression.json5`: L1-L87
- `android/game_scripts/test_merged_wall_two_pass_probe.json5`: L1-L110
- `android/game_scripts/test_midi_preview_hmp_unified.json5`: L1-L38
- `android/game_scripts/test_mine_exit_movie_touch_skip.json5`: L1-L59
- `android/game_scripts/test_mod_loading.json5`: L1-L90
- `android/game_scripts/test_msaa_fbo_smoke_d2.json5`: L1-L46
- `android/game_scripts/test_music_track_controls_unified.json5`: L1-L82
- `android/game_scripts/test_newmenu_render_paths_unified.json5`: L1-L101

### R1-CHUNK-0542

- `android/game_scripts/test_saf_basic.json5`: L1-L67
- `android/game_scripts/test_saf_redbook.json5`: L1-L122
- `android/game_scripts/test_secret_reveal_automap_d2.json5`: L1-L65
- `android/game_scripts/test_skip_every_launch_button_manual_unified.json5`: L1-L38
- `android/game_scripts/test_title_music_skip_pref_unified.json5`: L1-L61
- `android/game_scripts/test_trine2_d1_in_d2_custom_textures.json5`: L1-L94

### R1-CHUNK-0543

- `android/helpers/run_test.ps1`: L1-L399
- `android/helpers/test_env.ps1`: L1-L105

### R1-CHUNK-0544

- `android/helpers/test_helpers.ps1`: L2201-L2206
- `android/helpers/test_host_platform.ps1`: L1-L581

### R1-CHUNK-0547

- `android/tests/compare_input_demo_rng_trace.ps1`: L1-L355
- `android/tests/compare_input_demo_state_trace.ps1`: L1-L677

### R1-CHUNK-0548

- `android/tests/test_input_demo_controls.cpp`: L1-L385
- `android/tests/test_input_demo_determinism_matrix.ps1`: L1-L291

### R1-CHUNK-0549

- `android/tests/test_input_demo_regressions_graphics.ps1`: L1-L39
- `android/tests/test_input_demo_regressions.ps1`: L1-L64

### R1-CHUNK-0550

- `android/tests/test_lan.ps1`: L1101-L1349
- `android/tests/test_launcher_dpad.ps1`: L1-L277

### R1-CHUNK-0551

- `android/tests/test_random_level_preview.ps1`: L1-L293
- `android/tests/test_rng_seed_resume.c`: L1-L132
- `android/tests/test_route_analysis_cache.c`: L1-L112

### R1-CHUNK-0552

- `android/tests/test_mp.ps1`: L1-L815
- `android/tests/test_native_host_unit_tests.ps1`: L1-L55
- `android/tests/test_obsidian_level2_key_preference.ps1`: L1-L58

### R1-CHUNK-0553

- `android/tests/test_lan_broadcast.ps1`: L1-L328
- `android/tests/test_lan_discovery.ps1`: L1-L160
- `android/tests/test_lan_lobby_discovery.ps1`: L1-L164

### R1-CHUNK-0554

- `android/tests/run_input_demo_replay.ps1`: L1101-L1742
- `android/tests/secret_area_baseline_helpers.ps1`: L1-L146
- `android/tests/test_android_axis_mailbox.cpp`: L1-L167

### R1-CHUNK-0555

- `android/tests/test_dual_emu_setup.ps1`: L1-L343
- `android/tests/test_dual_emu.ps1`: L1-L433
- `android/tests/test_escort_exit_policy.c`: L1-L14
- `android/tests/test_escort_owner_policy.c`: L1-L128

### R1-CHUNK-0556

- `android/tests/test_android_rewind_policy.c`: L1-L213
- `android/tests/test_android_save_meta.c`: L1-L199
- `android/tests/test_android_save_set.c`: L1-L73
- `android/tests/test_base_mission_route_status.ps1`: L1-L37
- `android/tests/test_bot_client.ps1`: L1-L348

### R1-CHUNK-0557

- `android/tests/test_level_metadata_scan.c`: L1101-L1603
- `android/tests/test_manual_lan_coop.ps1`: L1-L24
- `android/tests/test_mission_metadata_json_normalization.ps1`: L1-L48
- `android/tests/test_mission_metadata_travel_times.ps1`: L1-L39
- `android/tests/test_mission_route_corpus.ps1`: L1-L176

### R1-CHUNK-0558

- `android/tests/test_input_demo_result.cpp`: L1-L351
- `android/tests/test_input_demo_rng_mode.c`: L1-L111
- `android/tests/test_input_demo_rng_trace_compare.ps1`: L1-L62
- `android/tests/test_input_demo_runtime_smoke.ps1`: L1-L279
- `android/tests/test_input_demo_state_trace_compare.ps1`: L1-L87
- `android/tests/test_kconfig_android_shared.c`: L1-L75

### R1-CHUNK-0559

- `android/tests/export_input_demo_state_trace.ps1`: L1-L157
- `android/tests/input_demo_game_data.ps1`: L1-L256
- `android/tests/input_demo_graphics_canary_helpers.ps1`: L1-L67
- `android/tests/input_demo_host_build_guard.ps1`: L1-L245
- `android/tests/probe_autoselect_plx.ps1`: L1-L174
- `android/tests/run_input_demo_headless.ps1`: L1-L97

### R1-CHUNK-0560

- `android/tests/test_fpcalc_and_acoustid.ps1`: L1-L281
- `android/tests/test_game_data_asset_manifest_writer.ps1`: L1-L174
- `android/tests/test_gog_installer_d1_unified.ps1`: L1-L143
- `android/tests/test_gog_installer_redbook_unified.ps1`: L1-L147
- `android/tests/test_gradle_unit_tests.ps1`: L1-L17
- `android/tests/test_homing_compat.c`: L1-L40
- `android/tests/test_hud_layout.c`: L1-L94

### R1-CHUNK-0561

- `android/tests/test_classic_demo_json.c`: L1-L269
- `android/tests/test_coop_host_migration_policy.c`: L1-L111
- `android/tests/test_coop_indicator_lines_math.c`: L1-L60
- `android/tests/test_coop_start_fanout_mapset.ps1`: L1-L119
- `android/tests/test_counterstrike_level2_trigger21_route.ps1`: L1-L110
- `android/tests/test_deterministic_math.c`: L1-L33
- `android/tests/test_double_launch.ps1`: L1-L182

### R1-CHUNK-0562

- `android/tests/test_route_snapshot.cpp`: L1101-L1215
- `android/tests/test_saf_redbook.ps1`: L1-L59
- `android/tests/test_secret_area_baseline_diff.ps1`: L1-L34
- `android/tests/test_secret_area_baseline.ps1`: L1-L238
- `android/tests/test_server_integration.ps1`: L1-L46
- `android/tests/test_standard_game_data_resolution.ps1`: L1-L54
- `android/tests/test_test_helpers_process_wait.ps1`: L1-L67
- `android/tests/test_validate_automation_catalog.ps1`: L1-L113
- `android/tests/update_secret_area_baseline.ps1`: L1-L39

### R1-CHUNK-0571

- `.tmp`
- `android/app_images/icon_512.png`
- `android/build_output.txt`
- `android/test_fixtures/allext_out/data.bin`
- `android/test_fixtures/allext_test.bin`
- `android/test_fixtures/bad_pvd.bin`
- `android/test_fixtures/extracted_iso_image/image.hog`
- `android/test_fixtures/extracted/test.hog`
- `android/test_fixtures/filter_test.bin`
- `android/test_fixtures/name_test1.bin`
- `android/test_fixtures/name_test2.bin`
- `android/test_fixtures/name_test3.bin`
- `android/test_fixtures/roundtrip_out/descent2.hog`
- `android/test_fixtures/roundtrip.bin`
- `android/test_fixtures/test_extract.bin`
- `android/test_fixtures/test_iso.bin`
- `android/test_output.txt`
- `android/tests/test_redbook_data/test_disc.bin`
- `dxx_matchmaking.db`
- `dxx_matchmaking.db-shm`
- `dxx_matchmaking.db-wal`
- `game_data_to_copy_to_emulator/debuglog_20260520_211753.txt`

### R1-CHUNK-0572

- `server/Cargo.lock`

### R1-CHUNK-0573

- `game_data/mission_files/THEHIVE.json`
- `game_data/mission_files/TheOmicronProject.json`
- `game_data/mission_files/trainng.json`
- `game_data/mission_files/Trine1.json`
- `game_data/mission_files/Trine1.tracklist.json`
- `game_data/mission_files/trine2.json`
- `game_data/mission_files/trine2.tracklist.json`
- `game_data/mission_files/TRINITY.json`
- `game_data/mission_files/tu.json`
- `game_data/mission_files/Tyrsis.json`
- `game_data/mission_files/U3AAH.json`
- `game_data/mission_files/ulterior_v1.0.6b.json`
- `game_data/mission_files/ulterior_v1.0.6b.tracklist.json`
- `game_data/mission_files/Uneasy4.json`
- `game_data/mission_files/Vela1.json`
- `game_data/mission_files/Vertigo Missions.json`
- `game_data/mission_files/Vesta.json`
- `game_data/mission_files/vignett2.json`
- `game_data/mission_files/Vignettes.json`
- `game_data/test_data_manifest.json`

### R1-CHUNK-0574

- `android/test_fixtures/secret_area_base_game_baseline.json`
- `android/tests/fixtures/mission_route_baseline.json`
- `game_data/mission_files/-MOON-.json`
- `game_data/mission_files/af_d1_beta.json`
- `game_data/mission_files/af-d2x.json`
- `game_data/mission_files/anachron.json`
- `game_data/mission_files/ascent.json`
- `game_data/mission_files/Bahagad.json`
- `game_data/mission_files/BelialSystemXL.json`
- `game_data/mission_files/bratmaze.json`
- `game_data/mission_files/castaway_redux.json`
- `game_data/mission_files/castaway_redux.tracklist.json`
- `game_data/mission_files/cererian_1.3.json`
- `game_data/mission_files/cererian_1.3.tracklist.json`
- `game_data/mission_files/Chasm.json`
- `game_data/mission_files/chromium.json`
- `game_data/mission_files/chron10b.json`
- `game_data/mission_files/Colossus.json`
- `game_data/mission_files/Countd2.json`
- `game_data/mission_files/Counterstrike.json`
- `game_data/mission_files/D1Lost.json`
- `game_data/mission_files/d1mercen.json`
- `game_data/mission_files/d1secret.json`
- `game_data/mission_files/D2Crossfire.json`
- `game_data/mission_files/dd1lvls1.json`
- `game_data/mission_files/dd2lvls1.json`
- `game_data/mission_files/Descend Again.json`
- `game_data/mission_files/Descent 1 to Descent 2 Conversion.json`
- `game_data/mission_files/descent_maximum_fixed.json`
- `game_data/mission_files/Descent- Invertaus 1.2.json`
- `game_data/mission_files/Descent.json`
- `game_data/mission_files/diehard.json`
- `game_data/mission_files/Disint_Beta_3.json`
- `game_data/mission_files/dontpnic.json`
- `game_data/mission_files/dozen.json`
- `game_data/mission_files/driller1.json`
- `game_data/mission_files/driller2.json`
- `game_data/mission_files/drmsaga.json`
- `game_data/mission_files/DVLVLS1.json`
- `game_data/mission_files/EAF.json`
- `game_data/mission_files/EAF2.json`
- `game_data/mission_files/entropy_v.1.0.json`
- `game_data/mission_files/Entropy.json`
- `game_data/mission_files/Entropy2.json`
- `game_data/mission_files/eq-set.json`
- `game_data/mission_files/erisreb.json`
- `game_data/mission_files/ex0core.json`
- `game_data/mission_files/Extra_Missions.json`
- `game_data/mission_files/fimbul.json`
- `game_data/mission_files/freelan.json`

### R1-CHUNK-0575

- `game_data/mission_files/GALAXY ASTEROIDS ALL.json`
- `game_data/mission_files/galmond2.json`
- `game_data/mission_files/gigalo.json`
- `game_data/mission_files/gnomes.json`
- `game_data/mission_files/grad3d.json`
- `game_data/mission_files/HDPACK1.json`
- `game_data/mission_files/HDVBETA2.json`
- `game_data/mission_files/Hydro.json`
- `game_data/mission_files/icerealm.json`
- `game_data/mission_files/Imds.json`
- `game_data/mission_files/INSANE MISSION PACK ULTRA.json`
- `game_data/mission_files/ironblade.json`
- `game_data/mission_files/ironstar.json`
- `game_data/mission_files/k_sos.json`
- `game_data/mission_files/KAK.json`
- `game_data/mission_files/kcxf2.json`
- `game_data/mission_files/KCXF2RMv11.json`
- `game_data/mission_files/KCXF2RMv11.tracklist.json`
- `game_data/mission_files/KKR.json`
- `game_data/mission_files/lagrange.json`
- `game_data/mission_files/legacy.json`
- `game_data/mission_files/levigen.json`
- `game_data/mission_files/Lostlvls.json`
- `game_data/mission_files/Lunar Series Revamped.json`
- `game_data/mission_files/magma.json`
- `game_data/mission_files/Mandrill.json`
- `game_data/mission_files/megalo.json`
- `game_data/mission_files/mna.json`
- `game_data/mission_files/mustfind.json`
- `game_data/mission_files/nefarious.json`
- `game_data/mission_files/nefarious.tracklist.json`
- `game_data/mission_files/Obsidian.json`
- `game_data/mission_files/odyssee.json`
- `game_data/mission_files/ORION-D2.json`
- `game_data/mission_files/Orion.json`
- `game_data/mission_files/outerrch11.json`
- `game_data/mission_files/phenomia.json`
- `game_data/mission_files/Phobos.json`
- `game_data/mission_files/plutonia.json`
- `game_data/mission_files/plutonionOutbreak.json`
- `game_data/mission_files/prophecy.json`
- `game_data/mission_files/quotienc_1.1.1.json`
- `game_data/mission_files/revdrav2.json`
- `game_data/mission_files/revodrav.json`
- `game_data/mission_files/ROGUE.json`
- `game_data/mission_files/sanitati2.json`
- `game_data/mission_files/sens9.json`
- `game_data/mission_files/sirius.json`
- `game_data/mission_files/sirius2.json`
- `game_data/mission_files/TEW.json`

### R1-CHUNK-0576

- `android/ai tool plans/testing/plan_test_dpad_triggers_report_20260522_232545.md`
- `android/ai tool plans/testing/plan_test_dpad_triggers_report_20260523_114346.md`
- `android/ai tool plans/testing/plan_test_failure_robustness_20260628.md`
- `android/ai tool plans/testing/plan_test_input_demo_runtime_smoke_20260523_110600.md`
- `android/ai tool plans/testing/plan_test_parameterization.md`
- `android/ai tool plans/testing/plan_test_report_fixes.md`
- `android/ai tool plans/testing/plan_test_report_rt_pause.md`
- `android/ai tool plans/testing/plan_test_suite_cleanup_20260620.md`
- `android/ai tool plans/testing/plan_test_suite_fixes.md`
- `android/ai tool plans/testing/plan_test_suite_runtime_survey_20260620.md`
- `android/ai tool plans/testing/plan.TEST_RELIABILITY.md`
- `android/ai tool plans/testing/plan.TEST_REPORT_FIXES_20260328.md`
- `android/ai tool plans/testing/regenerate_all_mission_metadata_20260705.md`
- `android/ai tool plans/testing/report_20260612_165248_intro_state_cluster.md`
- `android/ai tool plans/testing/report_20260612_165248_pause_cluster.md`
- `android/ai tool plans/testing/report_20260612_201722_failure_triage.md`
- `android/ai tool plans/testing/report_20260612_220011_autosave_missing_pilot_timeout.md`
- `android/ai tool plans/testing/test_fixes_and_speed_survey_20260328.md`
- `android/ai tool plans/testing/test_suite_rationalization_and_flake_hardening_20260709.md`
- `android/ai tool plans/testing/test_suite_speed_fixed_wait_audit_20260612.md`
- `android/ai tool plans/testing/unify_test_cleanup_and_controller_compare.md`
- `android/ai tool plans/testing/zero_param_metadata_regen_helper_20260705.md`

### R1-CHUNK-0577

- `android/ai tool plans/asset management/ASSET_SYSTEM_PLAN.md`
- `android/ai tool plans/asset management/dxa-hash-integration.md`
- `android/ai tool plans/asset management/export_regression_demos_20260531.md`
- `android/ai tool plans/asset management/file_import_generality.md`
- `android/ai tool plans/asset management/MOD_SYSTEM_PLAN.md`
- `android/ai tool plans/asset management/oversized_import_warning_investigation.md`
- `android/ai tool plans/asset management/plan_7z_mission_import_20260619.md`
- `android/ai tool plans/asset management/plan_briefing_single_tap_section_advance_20260609.md`
- `android/ai tool plans/asset management/plan_d1_d2_overlay_mod_compatibility_20260614.md`
- `android/ai tool plans/asset management/plan_d1_in_d2_full_support_design_20260613.md`
- `android/ai tool plans/asset management/plan_d1_trine2_blown_texture_mismatch_20260614.md`
- `android/ai tool plans/asset management/plan_d1_trine2_texture_precedence_20260614.md`
- `android/ai tool plans/asset management/plan_d2x_xl_hires_mods.md`
- `android/ai tool plans/asset management/plan_descent_max_two_pack_zip_view_20260611.md`
- `android/ai tool plans/asset management/plan_descent_maximum_briefing_screens_20260609.md`
- `android/ai tool plans/asset management/plan_descent2workshop_ham_library_review_20260518.md`
- `android/ai tool plans/asset management/plan_descriptor_hog_metadata_pairing_20260608.md`
- `android/ai tool plans/asset management/plan_dxa_all_in_one_and_texture_test.md`
- `android/ai tool plans/asset management/plan_dxa_download_suffix_import_20260518.md`
- `android/ai tool plans/asset management/plan_dxa_error_reporting_and_fix.md`
- `android/ai tool plans/asset management/plan_dxa_multi_mod_assessment_20260518.md`
- `android/ai tool plans/asset management/plan_dxa_pack_docs_and_layout_20260518.md`
- `android/ai tool plans/asset management/plan_dxa_parallel_speed_survey.md`
- `android/ai tool plans/asset management/plan_dxa_patch_overlap_detection_20260518.md`
- `android/ai tool plans/asset management/plan_enemy_within_15th_zip_support_20260606.md`
- `android/ai tool plans/asset management/plan_etc2tool_logging_and_dxa_rebuild.md`
- `android/ai tool plans/asset management/plan_extract_d1_in_d2_code_20260614.md`
- `android/ai tool plans/asset management/plan_extracted_mission_music_preview_20260619.md`
- `android/ai tool plans/asset management/plan_fix_mod_loading_0pct.md`
- `android/ai tool plans/asset management/plan_game_file_format_registry_consolidation_20260605.md`
- `android/ai tool plans/asset management/plan_game_file_metadata_support_20260605.md`
- `android/ai tool plans/asset management/plan_generalize_d1_effect_destinations_20260614.md`
- `android/ai tool plans/asset management/plan_generic_dxa_texture_paths_20260518.md`
- `android/ai tool plans/asset management/plan_import_storage_feedback_20260425.md`
- `android/ai tool plans/asset management/plan_include_large_ewithin_zip_analysis_20260611.md`
- `android/ai tool plans/asset management/plan_infinite_abyss_game_ready_import_20260521.md`
- `android/ai tool plans/asset management/plan_large_mission_archive_extraction_design_20260619.md`
- `android/ai tool plans/asset management/plan_large_mission_archive_extraction_feedback_20260619.md`
- `android/ai tool plans/asset management/plan_large_mission_archive_extraction_impl_20260619.md`
- `android/ai tool plans/asset management/plan_large_mission_zip_extraction_20260611.md`
- `android/ai tool plans/asset management/plan_large_zip_metadata_resume_rescan_20260611.md`
- `android/ai tool plans/asset management/plan_lunar_series_metadata_failure_json_20260611.md`
- `android/ai tool plans/asset management/plan_metadata_introspection_tranche_20260605.md`
- `android/ai tool plans/asset management/plan_metadata_tool_idea_review_20260605.md`
- `android/ai tool plans/asset management/plan_mission_archive_import_progress_20260619.md`
- `android/ai tool plans/asset management/plan_mission_zip_batch_failures_20260611.md`
- `android/ai tool plans/asset management/plan_mission_zip_batch_order_debug_20260612.md`
- `android/ai tool plans/asset management/plan_mission_zip_batch_ordering_20260611.md`
- `android/ai tool plans/asset management/plan_mission_zip_external_readmes_20260610.md`
- `android/ai tool plans/asset management/plan_mission_zip_metadata_music_and_advanced_lazy_20260619.md`

### R1-CHUNK-0578

- `android/ai tool plans/asset management/plan_mission_zip_term_cleanup_20260605.md`
- `android/ai tool plans/asset management/plan_mod_detail_path_and_hashes_20260518.md`
- `android/ai tool plans/asset management/plan_mod_details_dialog_20260518.md`
- `android/ai tool plans/asset management/plan_mod_details_visibility_and_hashes_20260518.md`
- `android/ai tool plans/asset management/plan_mod_manager.md`
- `android/ai tool plans/asset management/plan_mod_pack_tooling_relocation_20260518.md`
- `android/ai tool plans/asset management/plan_mod_preview_scroll_clipping_20260606.md`
- `android/ai tool plans/asset management/plan_multi_mission_zip_metadata_picker_20260608.md`
- `android/ai tool plans/asset management/plan_music_track_chromaprint_progress_20260611.md`
- `android/ai tool plans/asset management/plan_obsidian_zip_feature_support_20260606.md`
- `android/ai tool plans/asset management/plan_outerrch11_mission_name_truncation_20260611.md`
- `android/ai tool plans/asset management/plan_plain_texture_dxa_base_version_preflight_20260518.md`
- `android/ai tool plans/asset management/plan_plutonia_metadata_crash_20260609.md`
- `android/ai tool plans/asset management/plan_rar_import_support_20260619.md`
- `android/ai tool plans/asset management/plan_relocate_imported_files_storage.md`
- `android/ai tool plans/asset management/plan_remove_mod_detail_md5_20260518.md`
- `android/ai tool plans/asset management/plan_run_all_mission_zip_sample_20260612.md`
- `android/ai tool plans/asset management/plan_single_mission_zip_metadata_button_20260608.md`
- `android/ai tool plans/asset management/plan_uud2sp_ham_patch_dxa_20260518.md`
- `android/ai tool plans/asset management/plan_vertigo_import_mn2_20260608.md`
- `android/ai tool plans/asset management/plan_vertigo_secret_level_metadata_20260608.md`
- `android/ai tool plans/asset management/plan_xfing_engine_texture_patch_support_20260518.md`
- `android/ai tool plans/asset management/plan_xfing_minimal_dxa_conversion_20260518.md`
- `android/ai tool plans/asset management/plan_xfing_plain_texture_dxa_conversion_20260518.md`
- `android/ai tool plans/asset management/plan_xfing_texture_pack_dxa_study_20260517.md`
- `android/ai tool plans/asset management/plan-precompressed-etc2-dxa.md`
- `android/ai tool plans/asset management/stuffit-manifest-relocation-plan.md`
- `android/ai tool plans/build, release, packaging/build-1025-music-gyro-touch.md`
- `android/ai tool plans/build, release, packaging/check_updates_compile_sdk_dependency_bump_20260608.md`
- `android/ai tool plans/build, release, packaging/check_updates_windows_powershell_20260608.md`
- `android/ai tool plans/build, release, packaging/plan_build_95x_fixes.md`
- `android/ai tool plans/build, release, packaging/plan_build_malformed_unicode_properties.md`
- `android/ai tool plans/build, release, packaging/plan_d2tp_sp_combined_patch_alignment_20260518.md`
- `android/ai tool plans/build, release, packaging/plan_d2x_sp_gitignore_20260518.md`
- `android/ai tool plans/build, release, packaging/plan_extension_list_dedup_android.md`
- `android/ai tool plans/build, release, packaging/plan_fix_android_net_udp_build_interruption.md`
- `android/ai tool plans/build, release, packaging/plan_fix_android_upload_collide_probe_decls_20260502.md`
- `android/ai tool plans/build, release, packaging/plan_fix_android_upload_physics_probe_build.md`
- `android/ai tool plans/build, release, packaging/plan_imagemagick_128_downscale_readmes.md`
- `android/ai tool plans/build, release, packaging/server-deploy-scripts.md`
- `android/ai tool plans/build, release, packaging/version-code-rev-suffix.md`
- `android/ai tool plans/build, release, packaging/warning_lint_cleanup_20260714.md`
- `android/ai tool plans/CD audio/cd-image-acoustid-lookup.md`
- `android/ai tool plans/CD audio/plan_android_sfx_latency_survey_20260605.md`
- `android/ai tool plans/CD audio/plan_build_1030_music_fixes.md`
- `android/ai tool plans/CD audio/plan_cd_audio_resume_programming_20260522.md`
- `android/ai tool plans/CD audio/plan_cd_audio_saf_multibin_followup_20260508.md`
- `android/ai tool plans/CD audio/plan_cd_audio_saf_tree_permission_crash_20260507.md`
- `android/ai tool plans/CD audio/plan_cd_audio_synthetic_storage_cleanup_20260507.md`
- `android/ai tool plans/CD audio/plan_chromaprint_song_recognition.md`

### R1-CHUNK-0579

- `android/ai tool plans/CD audio/plan_d2_redbook_mp3_manifest_20260525.md`
- `android/ai tool plans/CD audio/plan_fix_acoustid_lookups.md`
- `android/ai tool plans/CD audio/plan_fix_chromaprint_stereo_and_tests.md`
- `android/ai tool plans/CD audio/plan_general_ui_audio_fixes.md`
- `android/ai tool plans/CD audio/plan_gog_dialog_audio_import_saf.md`
- `android/ai tool plans/CD audio/plan_gog_inst_music_visibility_20260525.md`
- `android/ai tool plans/CD audio/plan_gog_tv_cd_audio_debug_20260507.md`
- `android/ai tool plans/CD audio/plan_infinite_abyss_static_cd_audio_20260511.md`
- `android/ai tool plans/CD audio/plan_mission_tracklist_sweep_20260619.md`
- `android/ai tool plans/CD audio/plan_mission_zip_tracklist_fallback_20260619.md`
- `android/ai tool plans/CD audio/plan_multi_cd_menu_radial_jukebox.md`
- `android/ai tool plans/CD audio/plan_music_fixes.md`
- `android/ai tool plans/CD audio/plan_music_import_and_midi.md`
- `android/ai tool plans/CD audio/plan_music_playback_verification.md`
- `android/ai tool plans/CD audio/plan_music_ui_improvements.md`
- `android/ai tool plans/CD audio/plan_reuse_hmp2mid.md`
- `android/ai tool plans/CD audio/plan_saf_redbook_test_triage_20260516.md`
- `android/ai tool plans/CD audio/plan_title_music_skip_all_20260425.md`
- `android/ai tool plans/CD audio/plan_ui_audio_music_picker.md`
- `android/ai tool plans/CD audio/plan_unbound_actions_and_multibin_track_investigation_20260522.md`
- `android/ai tool plans/CD audio/plan_unified_music_controls.md`
- `android/ai tool plans/CD, installer parsing/BINCUE_PLAN.md`
- `android/ai tool plans/CD, installer parsing/extract-reorganization-plan.md`
- `android/ai tool plans/CD, installer parsing/fix_gog_import_issues.md`
- `android/ai tool plans/CD, installer parsing/GOG_AUDIO_OPTIONAL_EXTRACT.md`
- `android/ai tool plans/CD, installer parsing/GOG_EXTRACTION_PLAN.md`
- `android/ai tool plans/CD, installer parsing/INNOSETUP_FORMAT_SPEC.md`
- `android/ai tool plans/CD, installer parsing/integrate_mac_macplay_cd.md`
- `android/ai tool plans/CD, installer parsing/iso-data-loading-plan.md`
- `android/ai tool plans/CD, installer parsing/mac_cd_desktop_script_consolidation.md`
- `android/ai tool plans/CD, installer parsing/mac_extraction_pipeline.md`
- `android/ai tool plans/CD, installer parsing/on_device_mac_extract.md`
- `android/ai tool plans/CD, installer parsing/PKG_PARSER_PLAN.md`
- `android/ai tool plans/CD, installer parsing/plan_android_tv_saf_multiselect_bin_cue.md`
- `android/ai tool plans/CD, installer parsing/plan_chromaprint_import_recognition_research_20260620.md`
- `android/ai tool plans/CD, installer parsing/plan_d1_gog_installer_regression.md`
- `android/ai tool plans/CD, installer parsing/plan_demo_installer_extraction_20260524.md`
- `android/ai tool plans/CD, installer parsing/plan_expanded_cd_image_support_20260521.md`
- `android/ai tool plans/CD, installer parsing/plan_gog_installer_variant_coverage.md`
- `android/ai tool plans/CD, installer parsing/plan_mission_archive_count_progress_20260621.md`
- `android/ai tool plans/CD, installer parsing/plan_mission_zip_chromaprint_import_implementation_20260620.md`
- `android/ai tool plans/CD, installer parsing/plan_mission_zip_metadata_decoded_names_20260621.md`
- `android/ai tool plans/CD, installer parsing/plan_mission_zip_metadata_skip_cached_chromaprint_20260621.md`
- `android/ai tool plans/CD, installer parsing/plan_pc_android_cd_extract_audit_20260521.md`
- `android/ai tool plans/CD, installer parsing/plan_pc_android_extract_parity_audit_20260524.md`
- `android/ai tool plans/CD, installer parsing/plan_pc_side_native_extract_utilities.md`
- `android/ai tool plans/CD, installer parsing/plan_test_all_extracts_preview_disc_20260522.md`
- `android/ai tool plans/code management/build-919-fixes.md`
- `android/ai tool plans/code management/cleanup_metl154_rename_finalize.md`
- `android/ai tool plans/code management/cleanup_tranche_20260529_survey.md`

### R1-CHUNK-0580

- `android/ai tool plans/code management/CODE_QUALITY_TOOLING_PLAN.md`
- `android/ai tool plans/code management/controller-config-model-store-split-20260524.md`
- `android/ai tool plans/code management/D1_D2_COMPILATION_FIXES.md`
- `android/ai tool plans/code management/D1_INTEGRATION_PLAN.md`
- `android/ai tool plans/code management/d1d2_diff_candidate_catalog_20260711.md`
- `android/ai tool plans/code management/d1d2_diff_shrink_study.md`
- `android/ai tool plans/code management/d1d2_shrink_phase2_remaining_and_phase3_candidates.md`
- `android/ai tool plans/code management/d1d2_shrink_phase3_execution_plan.md`
- `android/ai tool plans/code management/dual-game-test-markers.md`
- `android/ai tool plans/code management/kotlin-refactor-survey-20260524.md`
- `android/ai tool plans/code management/msaa_fbo_helper_extraction_20260530.md`
- `android/ai tool plans/code management/ogl_overwrite_and_stale_formatter_defenses.md`
- `android/ai tool plans/code management/ogl_phase24_31_recovery_after_checkout.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase1.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase10.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase11.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase12.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase13.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase14.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase15.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase16.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase17.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase18.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase19.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase2.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase20.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase21.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase22.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase23.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase24.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase25.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase26.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase27.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase28.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase29.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase3.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase30.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase31.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase32.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase33.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase34.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase35.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase36.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase4.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase5.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase6.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase7.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase8.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase9.md`
- `android/ai tool plans/code management/PATH_B_OGLES_NOTES.md`

### R1-CHUNK-0581

- `android/ai tool plans/code management/plan_add_pwsh_shebangs_20260524.md`
- `android/ai tool plans/code management/plan_android_build_failures_20260508.md`
- `android/ai tool plans/code management/plan_android_lint_cleanup_20260523.md`
- `android/ai tool plans/code management/plan_android_test_gitignore_audit_20260527.md`
- `android/ai tool plans/code management/plan_base_loop_render_side_effect_cleanup_20260503.md`
- `android/ai tool plans/code management/plan_centralize_debug_logging.md`
- `android/ai tool plans/code management/plan_check_updates_first_pass_install_20260703.md`
- `android/ai tool plans/code management/plan_cmake_format_lint.md`
- `android/ai tool plans/code management/plan_code_cleanup_and_test_cleanup_20260514.md`
- `android/ai tool plans/code management/plan_crlf_lf_normalization.md`
- `android/ai tool plans/code management/plan_d1d2_diff_android_egl_surface_extract_20260712.md`
- `android/ai tool plans/code management/plan_d1d2_diff_android_fatal_bridge_20260712.md`
- `android/ai tool plans/code management/plan_d1d2_diff_android_scaled_font_extract_20260712.md`
- `android/ai tool plans/code management/plan_d1d2_diff_automap_metadata_overlay_extract_20260712.md`
- `android/ai tool plans/code management/plan_d1d2_diff_coop_multi_status_extract_20260711.md`
- `android/ai tool plans/code management/plan_d1d2_diff_coop_restore_remap_extract_20260712.md`
- `android/ai tool plans/code management/plan_d1d2_diff_coop_start_fanout_extract_20260712.md`
- `android/ai tool plans/code management/plan_d1d2_diff_effect_runtime_extract_20260712.md`
- `android/ai tool plans/code management/plan_d1d2_diff_expansive_resurvey_20260710.md`
- `android/ai tool plans/code management/plan_d1d2_diff_gamecntl_saveload_extract_20260711.md`
- `android/ai tool plans/code management/plan_d1d2_diff_hmp_mem_extract_20260711.md`
- `android/ai tool plans/code management/plan_d1d2_diff_hud_counts_extract_20260712.md`
- `android/ai tool plans/code management/plan_d1d2_diff_input_demo_probe_centralization_20260712.md`
- `android/ai tool plans/code management/plan_d1d2_diff_jukebox_names_extract_20260712.md`
- `android/ai tool plans/code management/plan_d1d2_diff_kconfig_scaled_render_extract_20260712.md`
- `android/ai tool plans/code management/plan_d1d2_diff_live_difficulty_extract_20260712.md`
- `android/ai tool plans/code management/plan_d1d2_diff_masked_bitmap_scale_extract_20260711.md`
- `android/ai tool plans/code management/plan_d1d2_diff_minimization_campaign_20260711.md`
- `android/ai tool plans/code management/plan_d1d2_diff_msaa_frame_extract_20260712.md`
- `android/ai tool plans/code management/plan_d1d2_diff_ogl_dxa_mask_reuse_20260711.md`
- `android/ai tool plans/code management/plan_d1d2_diff_ogl_runtime_texture_controls_20260711.md`
- `android/ai tool plans/code management/plan_d1d2_diff_ogl_viewport_extract_20260712.md`
- `android/ai tool plans/code management/plan_d1d2_diff_physfs_upstream_sync_20260711.md`
- `android/ai tool plans/code management/plan_d1d2_diff_physfsx_extract_20260711.md`
- `android/ai tool plans/code management/plan_d1d2_diff_refresh_biggest_changes_20260519.md`
- `android/ai tool plans/code management/plan_d1d2_diff_restore_netgame_count_extract_20260712.md`
- `android/ai tool plans/code management/plan_d1d2_diff_sdl_mixer_diagnostics_extract_20260712.md`
- `android/ai tool plans/code management/plan_d1d2_diff_shader_runtime_extract_20260712.md`
- `android/ai tool plans/code management/plan_d1d2_diff_shrink_next_20260519.md`
- `android/ai tool plans/code management/plan_d1d2_diff_shrink_refresh_20260710.md`
- `android/ai tool plans/code management/plan_d1d2_diff_songs_extract_20260710.md`
- `android/ai tool plans/code management/plan_d1d2_diff_startup_resume_extract_20260712.md`
- `android/ai tool plans/code management/plan_d1d2_diff_texmerge_owner_extract_20260712.md`
- `android/ai tool plans/code management/plan_d1d2_high_coupling_cleanup_campaign_20260712.md`
- `android/ai tool plans/code management/plan_d2_classic_demo_serialization_boundary_20260712.md`
- `android/ai tool plans/code management/plan_fix_android_upload_build_20260428.md`
- `android/ai tool plans/code management/plan_fix_android_upload_build_20260501.md`
- `android/ai tool plans/code management/plan_fix_branch_touched_warnings_20260428.md`
- `android/ai tool plans/code management/plan_game_data_manifest_20260524.md`
- `android/ai tool plans/code management/plan_header_source_survey_20260519.md`

### R1-CHUNK-0582

- `android/ai tool plans/code management/plan_headless_dump_tool_split_20260611.md`
- `android/ai tool plans/code management/plan_host_migration_disconnect_extraction_20260712.md`
- `android/ai tool plans/code management/plan_input_demo_direct_command_policy_20260712.md`
- `android/ai tool plans/code management/plan_lint_cleanup_and_vscode_exclusions.md`
- `android/ai tool plans/code management/plan_linux_dependency_bootstrap_20260525.md`
- `android/ai tool plans/code management/plan_main_vs_cmake_diff_shrink_20260504.md`
- `android/ai tool plans/code management/plan_powershell_msi_system_update_20260624.md`
- `android/ai tool plans/code management/plan_powershell_stable_only_20260624.md`
- `android/ai tool plans/code management/plan_powershell_update_helper_20260624.md`
- `android/ai tool plans/code management/plan_private_newmenu_rendering_state_20260712.md`
- `android/ai tool plans/code management/plan_repo_root_cleanup_20260524.md`
- `android/ai tool plans/code management/plan_sdk_cmdline_tools_install_sync_20260624.md`
- `android/ai tool plans/code management/plan_targeted_code_quality_guidance_20260606.md`
- `android/ai tool plans/code management/plan_task_mixup_audit_20260611.md`
- `android/ai tool plans/code management/plan_windows_build_autofind.md`
- `android/ai tool plans/code management/plan_write_side_cleanup_20260514.md`
- `android/ai tool plans/code management/plan-reliable-debug-output.md`
- `android/ai tool plans/code management/script_cleanup_20260526.md`
- `android/ai tool plans/code management/script_duplicate_cleanup_20260527.md`
- `android/ai tool plans/code management/setup-activity-automation-api-split-20260524.md`
- `android/ai tool plans/code management/setup-activity-config-disc-split-20260524.md`
- `android/ai tool plans/code management/setup-activity-dialogs-split-20260524.md`
- `android/ai tool plans/code management/setup-activity-file-import-split-20260524.md`
- `android/ai tool plans/code management/setup-activity-game-files-split-20260524.md`
- `android/ai tool plans/code management/setup-activity-resume-panel-split-20260524.md`
- `android/ai tool plans/code management/setup-activity-sections-split-20260524.md`
- `android/ai tool plans/code management/touch-overlay-helper-split-20260524.md`
- `android/ai tool plans/control mapping/axis-fix-and-parameterized-tests.md`
- `android/ai tool plans/control mapping/axis-test-completion.md`
- `android/ai tool plans/control mapping/binding_limit_enforcement.md`
- `android/ai tool plans/control mapping/control-mapping-root-cause-fixes.md`
- `android/ai tool plans/control mapping/controller-bugs-audio-menu-buttons.md`
- `android/ai tool plans/control mapping/controller-defaults-and-guidebot-fix.md`
- `android/ai tool plans/control mapping/double-tap-fix-and-cleanup.md`
- `android/ai tool plans/control mapping/emulator-gyro-axisregion.md`
- `android/ai tool plans/control mapping/etc2-reenable-launch-button.md`
- `android/ai tool plans/control mapping/exit-button-disk-rebuild.md`
- `android/ai tool plans/control mapping/EXTRA_CONTROLS_PLAN.md`
- `android/ai tool plans/control mapping/fix-logging-and-exit-button.md`
- `android/ai tool plans/control mapping/fix-start-exit-buttons-and-jni-log-bridge.md`
- `android/ai tool plans/control mapping/gyro-overhaul-touch-drag-gl-error.md`
- `android/ai tool plans/control mapping/half-axis-trigger-ui.md`
- `android/ai tool plans/control mapping/multiple-controller-touch-config-slots.md`
- `android/ai tool plans/control mapping/overlay-controller-admin-fixes.md`
- `android/ai tool plans/control mapping/phases-b-d-touch-mouse-missions.md`
- `android/ai tool plans/control mapping/plan_android_tv_controller_support.md`
- `android/ai tool plans/control mapping/plan_automap_marker_quick_menu_20260606.md`
- `android/ai tool plans/control mapping/plan_config_stale_cleanup.md`
- `android/ai tool plans/control mapping/plan_controller_deadzone_and_live_view.md`
- `android/ai tool plans/control mapping/plan_controller_mapping_and_exit_button_fixes.md`

### R1-CHUNK-0583

- `android/ai tool plans/control mapping/plan_controller_menu_and_slider_fixes.md`
- `android/ai tool plans/control mapping/plan_controller_menu_cycle_and_overlay_focus_20260520.md`
- `android/ai tool plans/control mapping/plan_controller_touch_editor_updates_20260507.md`
- `android/ai tool plans/control mapping/plan_game_menu_guidebot_controller_20260522.md`
- `android/ai tool plans/control mapping/plan_game_menu_touch_wheel_labels_20260522.md`
- `android/ai tool plans/control mapping/plan_general_scroll_repeat_20260507.md`
- `android/ai tool plans/control mapping/plan_input_mixer.md`
- `android/ai tool plans/control mapping/plan_launcher_controller_focus_outline_20260601.md`
- `android/ai tool plans/control mapping/plan_launcher_dpad_focus_fix.md`
- `android/ai tool plans/control mapping/plan_level_select_numeric_keyboard_fix_20260504.md`
- `android/ai tool plans/control mapping/plan_menu_checkbox_controller_activate.md`
- `android/ai tool plans/control mapping/plan_mp_controller_fixes.md`
- `android/ai tool plans/control mapping/plan_multiplayer_lazy_dpad_focus_20260601.md`
- `android/ai tool plans/control mapping/plan_shield_controller_focus_fixes.md`
- `android/ai tool plans/control mapping/plan_slide_up_button_col2.md`
- `android/ai tool plans/control mapping/plan_touch_button_dynamic_weapon_labels_20260512.md`
- `android/ai tool plans/control mapping/plan_touch_controls_music_radial.md`
- `android/ai tool plans/control mapping/plan_touch_editor_floating_stick_regions_20260507.md`
- `android/ai tool plans/control mapping/plan_touch_menu_and_exit_movie_research_20260525.md`
- `android/ai tool plans/control mapping/plan_touch_more_action_filters_20260512.md`
- `android/ai tool plans/control mapping/plan_touch_more_gyro_light_followup_20260512.md`
- `android/ai tool plans/control mapping/plan_touch_more_overlay_fixes_20260511.md`
- `android/ai tool plans/control mapping/plan_touch_stick_extreme_action_20260613.md`
- `android/ai tool plans/control mapping/plan_touch_stick_initial_position_travel_20260613.md`
- `android/ai tool plans/control mapping/plan_unbound_settings_controller_bindings_20260701.md`
- `android/ai tool plans/control mapping/plan_unbound_settings_weapon_cycle_fallback_20260701.md`
- `android/ai tool plans/control mapping/scroll-gyro-doubletap-chain.md`
- `android/ai tool plans/control mapping/test_mp_button_nav_rewrite.md`
- `android/ai tool plans/control mapping/TOUCH_INTERFACE_PLAN.md`
- `android/ai tool plans/control mapping/touch-controller-fixes-build916.md`
- `android/ai tool plans/control mapping/touch-controls-sensitivity-gyro.md`
- `android/ai tool plans/control mapping/touch-interface-bugs.md`
- `android/ai tool plans/controls/guidebot_unexplored_wheel_slice_20260711.md`
- `android/ai tool plans/crash, logging, diagnostics/coop_crosshair_face_and_palette_probe_20260707.md`
- `android/ai tool plans/crash, logging, diagnostics/coop_door26_palette_logs_20260707.md`
- `android/ai tool plans/crash, logging, diagnostics/coop_door26_still_wrong_after_palette_invalidate_20260707.md`
- `android/ai tool plans/crash, logging, diagnostics/coop_hidden_door_texture_reasoning_20260707.md`
- `android/ai tool plans/crash, logging, diagnostics/coop_level_state_tap_compare_20260707.md`
- `android/ai tool plans/crash, logging, diagnostics/coop_level7_blue_texture_log_review_20260706.md`
- `android/ai tool plans/crash, logging, diagnostics/coop_level7_new_logs_20260707.md`
- `android/ai tool plans/crash, logging, diagnostics/coop_level7_no_visual_change_after_route_unify_20260707.md`
- `android/ai tool plans/crash, logging, diagnostics/coop_level7_no_visual_change_log_review_20260707.md`
- `android/ai tool plans/crash, logging, diagnostics/coop_level7_ten_round_log_review_20260707.md`
- `android/ai tool plans/crash, logging, diagnostics/coop_rendering_difference_audit_20260707.md`
- `android/ai tool plans/crash, logging, diagnostics/coop_restore_desync_log_study_20260708.md`
- `android/ai tool plans/crash, logging, diagnostics/coop_sp_vs_coop_tap_compare_20260707.md`
- `android/ai tool plans/crash, logging, diagnostics/coop_texture_cleanup_retrospective_20260707.md`
- `android/ai tool plans/crash, logging, diagnostics/coop_texture_desync_logging_20260706.md`
- `android/ai tool plans/crash, logging, diagnostics/coop_texture_ten_round_logging_20260707.md`
- `android/ai tool plans/crash, logging, diagnostics/crash-reporting.md`

### R1-CHUNK-0584

- `android/ai tool plans/crash, logging, diagnostics/multiplayer_loading_palette_20260708.md`
- `android/ai tool plans/crash, logging, diagnostics/palette_lifetime_audit_20260708.md`
- `android/ai tool plans/crash, logging, diagnostics/plan_crash_handler_and_flip_logging.md`
- `android/ai tool plans/crash, logging, diagnostics/plan_death_animation_border_static_20260610.md`
- `android/ai tool plans/crash, logging, diagnostics/plan_profiling_log_category_20260521.md`
- `android/ai tool plans/crash, logging, diagnostics/plan_purge_gamelog_txt.md`
- `android/ai tool plans/crash, logging, diagnostics/plan_xcrash_integration.md`
- `android/ai tool plans/crash, logging, diagnostics/sigabrt-crash-fix.md`
- `android/ai tool plans/emulator/plan_emulator_recovery_hardening_and_lint_20260518.md`
- `android/ai tool plans/emulator/plan_emulator_start_crash_fallback_20260526.md`
- `android/ai tool plans/emulator/plan_lan_multicast_emulator_365.md`
- `android/ai tool plans/emulator/plan_lightweight_avds_and_separate_process.md`
- `android/ai tool plans/emulator/plan_run_all_tests_auto_stale_emulator_cleanup_20260526.md`
- `android/ai tool plans/emulator/plan_suite_emulator_recovery_20260515.md`
- `android/ai tool plans/emulator/plan.DUAL_EMU_DOCKER_NAT.md`
- `android/ai tool plans/emulator/plan.TWO_PLAYER_TEST.md`
- `android/ai tool plans/emulator/texfilt-text-menu-emulator.md`
- `android/ai tool plans/emulator/two-emu-reliability-phase8.md`
- `android/ai tool plans/emulator/windows_host_build_repair_and_emulator_verify.md`
- `android/ai tool plans/file import/plan_gradle_unit_tests_hashing_loop_20260619.md`
- `android/ai tool plans/file import/plan_hashing_loop_cleanup_20260619.md`
- `android/ai tool plans/file import/plan_launcher_hashing_loop_introspection_20260619.md`
- `android/ai tool plans/file import/plan_run_all_tests_hashing_loop_20260619.md`
- `android/ai tool plans/floating-point determinism/fp-determinism-tranche-priority-lockdown-20260429.md`
- `android/ai tool plans/floating-point determinism/fp-determinism-tranche-revision-20260429.md`
- `android/ai tool plans/floating-point determinism/fp-startup-hardening-20260429.md`
- `android/ai tool plans/floating-point determinism/plan_fp_environment_shared_helper_20260502.md`
- `android/ai tool plans/gameplay/automap_objective_display_modes_20260713.md`
- `android/ai tool plans/gameplay/base_campaign_route_regression_audit_20260711.md`
- `android/ai tool plans/gameplay/base_mission_boss_guidebot_metadata_plan_20260704.md`
- `android/ai tool plans/gameplay/contained_key_robot_objectives_20260713.md`
- `android/ai tool plans/gameplay/counterstrike_level2_switch_completion_20260716.md`
- `android/ai tool plans/gameplay/descent_level9_blue_key_route_20260705.md`
- `android/ai tool plans/gameplay/guidebot_coop_key_goal_20260701.md`
- `android/ai tool plans/gameplay/guidebot_metadata_pathing_parity_20260705.md`
- `android/ai tool plans/gameplay/guidebot_metadata_pathing_parity_survey_20260709.md`
- `android/ai tool plans/gameplay/guidebot_metadata_pathing_unification_plan_20260711.md`
- `android/ai tool plans/gameplay/guidebot_mid_level_trigger_routing_plan_20260705.md`
- `android/ai tool plans/gameplay/guidebot_navigation_cpu_audit_20260711.md`
- `android/ai tool plans/gameplay/guidebot_nearest_shoot_guidance_point_20260715.md`
- `android/ai tool plans/gameplay/guidebot_owner_menu_crash_20260702.md`
- `android/ai tool plans/gameplay/guidebot_pathing_modernization_remediation_20260709.md`
- `android/ai tool plans/gameplay/guidebot_phase6_blastable_wall_completion_20260713.md`
- `android/ai tool plans/gameplay/guidebot_phase6_target_completion_20260713.md`
- `android/ai tool plans/gameplay/guidebot_phase7_dynamic_ownership_invalidation_20260713.md`
- `android/ai tool plans/gameplay/guidebot_phase7_event_generations_20260714.md`
- `android/ai tool plans/gameplay/guidebot_phase7_multiplayer_lifecycle_20260714.md`
- `android/ai tool plans/gameplay/guidebot_phase8_superseded_pathing_removal_20260714.md`
- `android/ai tool plans/gameplay/guidebot_placement_metadata_plan_20260705.md`
- `android/ai tool plans/gameplay/guidebot_save_restore_route_goal_reset_20260709.md`

### R1-CHUNK-0585

- `android/ai tool plans/gameplay/guidebot_spawn_release_sync_20260701.md`
- `android/ai tool plans/gameplay/guidebot_unexplored_wheel_goal_20260709.md`
- `android/ai tool plans/gameplay/guidebot_unreachable_fallback_explore_goal_20260709.md`
- `android/ai tool plans/gameplay/headlight_not_on_default_qol_20260706.md`
- `android/ai tool plans/gameplay/investigate_mission_travel_time_text_20260711.md`
- `android/ai tool plans/gameplay/kcxf2_automap_scan_and_route_regressions_20260713.md`
- `android/ai tool plans/gameplay/kcxf2_level1_partial_route_regression_20260709.md`
- `android/ai tool plans/gameplay/kcxf2_level2_exit_path_20260704.md`
- `android/ai tool plans/gameplay/kcxf2_level3_shootable_pathing_20260705.md`
- `android/ai tool plans/gameplay/kcxf2_level5_multi_switch_route_20260711.md`
- `android/ai tool plans/gameplay/kcxf2_metadata_status_and_route_wording_20260710.md`
- `android/ai tool plans/gameplay/key_preference_unresolved_regression_20260717.md`
- `android/ai tool plans/gameplay/key_preferred_transparent_shortcuts_20260717.md`
- `android/ai tool plans/gameplay/level_metadata_trigger_route_guidebot_plan_20260704.md`
- `android/ai tool plans/gameplay/mission_route_failure_corpus_audit_20260716.md`
- `android/ai tool plans/gameplay/mission_route_failure_corpus_audit_round2_20260716.md`
- `android/ai tool plans/gameplay/mission_route_trigger_dependency_audit_20260716.md`
- `android/ai tool plans/gameplay/objective_numbering_unification_20260715.md`
- `android/ai tool plans/gameplay/obsidian_level1_next_objectives_20260715.md`
- `android/ai tool plans/gameplay/obsidian_level1_objective_marker_remediation_20260715.md`
- `android/ai tool plans/gameplay/obsidian_level1_post_blue_route_20260715.md`
- `android/ai tool plans/gameplay/obsidian_level2_route_investigation_20260715.md`
- `android/ai tool plans/gameplay/ordinary_shoot_open_door_routes_20260716.md`
- `android/ai tool plans/gameplay/original_homing_behavior_20260714.md`
- `android/ai tool plans/gameplay/plan_d1_in_d2_gameplay_semantics_detail_20260616.md`
- `android/ai tool plans/gameplay/plan_d1_in_d2_gameplay_semantics_survey_20260614.md`
- `android/ai tool plans/gameplay/plan_d2_hud_counts_pref_20260703.md`
- `android/ai tool plans/gameplay/plan_flythrough_exit_metadata_guidebot_20260704.md`
- `android/ai tool plans/gameplay/plan_indicator_line_fade_visibility_20260703.md`
- `android/ai tool plans/gameplay/plan_kcxf2_guidebot_hidden_wall_20260709.md`
- `android/ai tool plans/gameplay/projectile_valid_switch_routing_20260715.md`
- `android/ai tool plans/gameplay/remove_legacy_metadata_travel_calculation_20260710.md`
- `android/ai tool plans/gameplay/route_activation_awareness_20260708.md`
- `android/ai tool plans/gameplay/route_cache_android_audit_20260715.md`
- `android/ai tool plans/gameplay/shoot_through_waypoint_rule_audit_20260715.md`
- `android/ai tool plans/gameplay/transparent_projectile_route_surfaces_20260716.md`
- `android/ai tool plans/gameplay/unexplored_route_endpoint_correction_20260711.md`
- `android/ai tool plans/gameplay/unused_level_key_metadata_20260717.md`
- `android/ai tool plans/graphics/merged_wall_experiment_test_compile_20260708.md`
- `android/ai tool plans/graphics/merged_wall_stale_script_cleanup_20260708.md`
- `android/ai tool plans/graphics/render_gl_gequal_build_fix_20260708.md`
- `android/ai tool plans/graphics/restore_merged_wall_two_pass_tests_20260708.md`
- `android/ai tool plans/guidebot_warp_to_me_20260705.md`
- `android/ai tool plans/input demo, replay, determinism/input-demo-camera-wake-engine-fix-20260512.md`
- `android/ai tool plans/input demo, replay, determinism/input-demo-d1-d2-parity-audit-20260512.md`
- `android/ai tool plans/input demo, replay, determinism/input-demo-hard-coded-probe-cleanup-20260512.md`
- `android/ai tool plans/input demo, replay, determinism/input-demo-level9-death-replay-20260511.md`
- `android/ai tool plans/input demo, replay, determinism/input-demo-level9-headlight-20260512.md`
- `android/ai tool plans/input demo, replay, determinism/input-demo-schema.md`
- `android/ai tool plans/input demo, replay, determinism/plan_awareness_probe_logging_20260501.md`

### R1-CHUNK-0586

- `android/ai tool plans/input demo, replay, determinism/plan_awareness_source_instrumentation_20260502.md`
- `android/ai tool plans/input demo, replay, determinism/plan_collision_pose_logging_20260501.md`
- `android/ai tool plans/input demo, replay, determinism/plan_d1_demo_d1_in_d2_replay_20260616.md`
- `android/ai tool plans/input demo, replay, determinism/plan_d1_demo_d1_in_d2_runner_path_20260616.md`
- `android/ai tool plans/input demo, replay, determinism/plan_d1_in_d2_full_demo_pass_20260619.md`
- `android/ai tool plans/input demo, replay, determinism/plan_d1_in_d2_input_demo_centralization_20260617.md`
- `android/ai tool plans/input demo, replay, determinism/plan_d1_level12_demo_replay_20260617.md`
- `android/ai tool plans/input demo, replay, determinism/plan_d1_level13_14_demo_replay_20260617.md`
- `android/ai tool plans/input demo, replay, determinism/plan_d1_level16_17_demo_replay_20260618.md`
- `android/ai tool plans/input demo, replay, determinism/plan_d1_level16_18_demo_replay_20260618.md`
- `android/ai tool plans/input demo, replay, determinism/plan_d1_level4_20260616_121645_replay_20260616.md`
- `android/ai tool plans/input demo, replay, determinism/plan_d1_level6_9_15_demo_replay_20260617.md`
- `android/ai tool plans/input demo, replay, determinism/plan_d1_level9_recreated_demo_instrumentation_20260617.md`
- `android/ai tool plans/input demo, replay, determinism/plan_dem_to_json_and_desync_analysis_20260429.md`
- `android/ai tool plans/input demo, replay, determinism/plan_demo_134049_rng_regression_20260511.md`
- `android/ai tool plans/input demo, replay, determinism/plan_demo_analysis.md`
- `android/ai tool plans/input demo, replay, determinism/plan_demo_final_robot_kills_fix_20260502.md`
- `android/ai tool plans/input demo, replay, determinism/plan_demo_log_sufficiency_20260510.md`
- `android/ai tool plans/input demo, replay, determinism/plan_demo_pref_key_cleanup_and_object_gate_20260505.md`
- `android/ai tool plans/input demo, replay, determinism/plan_demo_runner_crash_and_cleanup_20260505.md`
- `android/ai tool plans/input demo, replay, determinism/plan_engine_determinism_roadmap_20260510.md`
- `android/ai tool plans/input demo, replay, determinism/plan_enhanced_save_state_for_checkpoint_replay.md`
- `android/ai tool plans/input demo, replay, determinism/plan_export_rng_trace_from_advanced_page.md`
- `android/ai tool plans/input demo, replay, determinism/plan_first_divergent_object_detail_20260510.md`
- `android/ai tool plans/input demo, replay, determinism/plan_fix_headless_checkpoint_crash_20260714.md`
- `android/ai tool plans/input demo, replay, determinism/plan_fresh_demo_desync_analysis_20260429.md`
- `android/ai tool plans/input demo, replay, determinism/plan_full_buddy_checkpoint_state_20260501.md`
- `android/ai tool plans/input demo, replay, determinism/plan_gauss_spawn_probe_20260511.md`
- `android/ai tool plans/input demo, replay, determinism/plan_headless_console_demo_runner_20260430.md`
- `android/ai tool plans/input demo, replay, determinism/plan_headless_demo_regressions_20260714.md`
- `android/ai tool plans/input demo, replay, determinism/plan_headless_runner_ux_and_replay_logging_trim_20260503.md`
- `android/ai tool plans/input demo, replay, determinism/plan_host_replay_sandbox_guardrails.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_actual_result_sidecar_cleanup_20260504.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_base64_sha256_centralization_20260429.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_build_freshness_helper_20260504.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_classic_dem_sidecar_20260429.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_codec_dependency_swap_20260429.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_device_recording_and_regression.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_frame_events_20260501.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_header_result_and_checkpoint_compression_20260429.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_helper_extraction_survey_20260504.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_live_state_trace_20260430.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_mid_level_start.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_original_homing_20260714.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_per_frame_state_20260430.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_phase2_coalescer.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_phase2_controls_json.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_phase3_fixture_writer.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_phase3_live_recorder.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_phase4_d1_runtime_bootstrap.md`

### R1-CHUNK-0587

- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_phase4_d2_runtime_bootstrap.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_phase4_replay_session.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_phase5_desktop_runtime_smoke.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_phase5_recorder_rich_baseline.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_phase5_result_compare.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_phase5_result_serializer.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_playercfg_header_subset_20260429.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_replay_rng_trace_20260501.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_replay_state_compare_alignment_20260501.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_single_file_rework.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_temp_game_logs_20260430.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_unrecorded_action_survey_20260503.md`
- `android/ai tool plans/input demo, replay, determinism/plan_level6_demo_replay_analysis_20260508_123202.md`
- `android/ai tool plans/input demo, replay, determinism/plan_level6_demo_replay_analysis_20260508_193201.md`
- `android/ai tool plans/input demo, replay, determinism/plan_level7_demo_replay_analysis_20260508_233937.md`
- `android/ai tool plans/input demo, replay, determinism/plan_level7_demo_replay_analysis_20260509_140807.md`
- `android/ai tool plans/input demo, replay, determinism/plan_level8_demo_replay_analysis_20260509_173157.md`
- `android/ai tool plans/input demo, replay, determinism/plan_level8_demo_replay_analysis_20260509_202642_202756_202946.md`
- `android/ai tool plans/input demo, replay, determinism/plan_level9_demo_replay_analysis_20260510_102637_102738.md`
- `android/ai tool plans/input demo, replay, determinism/plan_level9_demo_replay_analysis_20260511_091552.md`
- `android/ai tool plans/input demo, replay, determinism/plan_long_demo_desync_d2_level2_20260503_203112.md`
- `android/ai tool plans/input demo, replay, determinism/plan_new_l2_demo_replay_analysis_20260430.md`
- `android/ai tool plans/input demo, replay, determinism/plan_next_demo_recording_logging_20260502.md`
- `android/ai tool plans/input demo, replay, determinism/plan_nonheadless_robot_disappear_20260503.md`
- `android/ai tool plans/input demo, replay, determinism/plan_object_list_order_detection_20260510.md`
- `android/ai tool plans/input demo, replay, determinism/plan_on_device_replay_and_forcefield_sync_20260505.md`
- `android/ai tool plans/input demo, replay, determinism/plan_optional_per_frame_state_tracking.md`
- `android/ai tool plans/input demo, replay, determinism/plan_player_drag_localization_20260501_141150.md`
- `android/ai tool plans/input demo, replay, determinism/plan_player_hit_logging_20260501_141150.md`
- `android/ai tool plans/input demo, replay, determinism/plan_player_motion_probe_20260501_141150.md`
- `android/ai tool plans/input demo, replay, determinism/plan_regression_demos_headless_20260430.md`
- `android/ai tool plans/input demo, replay, determinism/plan_replay_analysis_20260509_level9_225113_level8_224859.md`
- `android/ai tool plans/input demo, replay, determinism/plan_replay_debug_demo_20260428_141502.md`
- `android/ai tool plans/input demo, replay, determinism/plan_replay_debug_with_rngtrace_20260427.md`
- `android/ai tool plans/input demo, replay, determinism/plan_replay_helper_cleanup_and_optional_logging_20260503.md`
- `android/ai tool plans/input demo, replay, determinism/plan_replay_manual_step_controls_20260503.md`
- `android/ai tool plans/input demo, replay, determinism/plan_replay_playercfg_audit_20260429.md`
- `android/ai tool plans/input demo, replay, determinism/plan_replay_probe_d2_20260428_191632.md`
- `android/ai tool plans/input demo, replay, determinism/plan_replay_probe_d2_20260428_195254.md`
- `android/ai tool plans/input demo, replay, determinism/plan_replay_probe_d2_20260428_212524.md`
- `android/ai tool plans/input demo, replay, determinism/plan_replay_probe_d2_20260428_220300.md`
- `android/ai tool plans/input demo, replay, determinism/plan_replay_probe_d2_20260429_074558.md`
- `android/ai tool plans/input demo, replay, determinism/plan_replay_probe_d2_20260429_124801.md`
- `android/ai tool plans/input demo, replay, determinism/plan_replay_root_cause_ai_weapon_order_20260509.md`
- `android/ai tool plans/input demo, replay, determinism/plan_replay_runner_build_guardrails_20260502.md`
- `android/ai tool plans/input demo, replay, determinism/plan_replay_runner_render_profiles_20260502.md`
- `android/ai tool plans/input demo, replay, determinism/plan_replay_script_dedup_20260502.md`
- `android/ai tool plans/input demo, replay, determinism/plan_rewind_support_research_20260516.md`
- `android/ai tool plans/input demo, replay, determinism/plan_rng_code_local_notes_20260511.md`
- `android/ai tool plans/input demo, replay, determinism/plan_rng_discipline_frametime_math_20260510.md`

### R1-CHUNK-0588

- `android/ai tool plans/input demo, replay, determinism/plan_rng_discipline_survey_20260510.md`
- `android/ai tool plans/input demo, replay, determinism/plan_rng_full_instrumentation_20260501.md`
- `android/ai tool plans/input demo, replay, determinism/plan_rng_fx_revert_tracking_20260511.md`
- `android/ai tool plans/input demo, replay, determinism/plan_rng_omega_and_fireball_audit_20260511.md`
- `android/ai tool plans/input demo, replay, determinism/plan_rng_origin_compare_detail_20260510.md`
- `android/ai tool plans/input demo, replay, determinism/plan_rng_sim_vs_nonsim_split.md`
- `android/ai tool plans/input demo, replay, determinism/plan_rng_trace_sidecar_for_input_demos.md`
- `android/ai tool plans/input demo, replay, determinism/plan_robot_ai_state_logging_20260501.md`
- `android/ai tool plans/input demo, replay, determinism/plan_robot_spawn_state_audit_20260502.md`
- `android/ai tool plans/input demo, replay, determinism/plan_sim_determinism_render_decoupling_20260502.md`
- `android/ai tool plans/input demo, replay, determinism/plan_spreadfire_demo_playback_ogl_audit_20260429.md`
- `android/ai tool plans/input demo, replay, determinism/plan_strip_input_demo_frame_state_20260504.md`
- `android/ai tool plans/input demo, replay, determinism/plan_temp_game_logs_homing_desync_20260505.md`
- `android/ai tool plans/input demo, replay, determinism/plan_temp_game_logs_replay_analysis_20260501_103312.md`
- `android/ai tool plans/input demo, replay, determinism/plan_temp_game_logs_replay_analysis_20260501_125538.md`
- `android/ai tool plans/input demo, replay, determinism/plan_temp_game_logs_replay_analysis_20260501_133242.md`
- `android/ai tool plans/input demo, replay, determinism/plan_temp_game_logs_replay_analysis_20260501_141150.md`
- `android/ai tool plans/input demo, replay, determinism/plan_temp_game_logs_replay_analysis_20260501_154249.md`
- `android/ai tool plans/input demo, replay, determinism/plan_temp_game_logs_replay_analysis_20260501.md`
- `android/ai tool plans/input demo, replay, determinism/plan_temp_game_logs_replay_analysis_20260502_223549.md`
- `android/ai tool plans/input demo, replay, determinism/plan_temp_game_logs_replay_analysis_20260502_231831_headless_rng_audit.md`
- `android/ai tool plans/input demo, replay, determinism/plan_temp_game_logs_replay_analysis_20260503_184215_rng_gap.md`
- `android/ai tool plans/input demo, replay, determinism/plan_temp_game_logs_replay_analysis_20260504_fresh_desyncs.md`
- `android/ai tool plans/input demo, replay, determinism/plan_temp_game_logs_replay_analysis_20260506_135119.md`
- `android/ai tool plans/input demo, replay, determinism/plan_temp_game_logs_replay_analysis_20260506_223956.md`
- `android/ai tool plans/input demo, replay, determinism/plan_temp_game_logs_replay_analysis_20260507_210511.md`
- `android/ai tool plans/input demo, replay, determinism/plan_terminal_exit_marker_20260506.md`
- `android/ai tool plans/input demo, replay, determinism/plan_thief_checkpoint_state_audit_20260503.md`
- `android/ai tool plans/input demo, replay, determinism/plan_thief_checkpoint_state_impl_20260503.md`
- `android/ai tool plans/input demo, replay, determinism/tap_now_pose_logging_and_pose_warp.md`
- `android/ai tool plans/launcher admin/CONFIG_IMPORT_EXPORT_PLAN.md`
- `android/ai tool plans/launcher admin/fix-home-button-crash.md`
- `android/ai tool plans/launcher admin/fix-stuck-launcher-and-regressions.md`
- `android/ai tool plans/launcher admin/in-progress-games-config-sharing-bugfixes.md`
- `android/ai tool plans/launcher admin/pilot-file-management-and-advanced-tab.md`
- `android/ai tool plans/launcher admin/plan_abort_autosave_slot_20260602.md`
- `android/ai tool plans/launcher admin/plan_about_store_page_link.md`
- `android/ai tool plans/launcher admin/plan_add_launcher_stale_file_debug_logging.md`
- `android/ai tool plans/launcher admin/plan_advanced_tab_open_progress_20260621.md`
- `android/ai tool plans/launcher admin/plan_android_autosave_resume_20260512.md`
- `android/ai tool plans/launcher admin/plan_autosave_postlaunch_timeout_20260515.md`
- `android/ai tool plans/launcher admin/plan_check_updates_cmake_sdk_managed.md`
- `android/ai tool plans/launcher admin/plan_check_updates_dependency_coverage.md`
- `android/ai tool plans/launcher admin/plan_check_updates_github_fallback_and_pwsh.md`
- `android/ai tool plans/launcher admin/plan_check_updates_hard_lock_guard.md`
- `android/ai tool plans/launcher admin/plan_check_updates_installed_versions.md`
- `android/ai tool plans/launcher admin/plan_check_updates_loading_and_jdk_sync_20260523.md`
- `android/ai tool plans/launcher admin/plan_check_updates_optional_tool_cleanup.md`
- `android/ai tool plans/launcher admin/plan_check_updates_powershell_host_drift.md`
- `android/ai tool plans/launcher admin/plan_check_updates_reload_conf_failure.md`

### R1-CHUNK-0589

- `android/ai tool plans/launcher admin/plan_check_updates_stable_versions.md`
- `android/ai tool plans/launcher admin/plan_check_updates_unknowns_and_powershell.md`
- `android/ai tool plans/launcher admin/plan_check_updates_unknowns_and_pwsh.md`
- `android/ai tool plans/launcher admin/plan_check_updates_unrar_sort_and_gradle_tracking.md`
- `android/ai tool plans/launcher admin/plan_clear_all_game_data_20260521.md`
- `android/ai tool plans/launcher admin/plan_demo_installer_launch_offers_20260525.md`
- `android/ai tool plans/launcher admin/plan_engine_prefs_touch_launcher_qol.md`
- `android/ai tool plans/launcher admin/plan_fix_launcher_gpgs_app_id_manifest_type.md`
- `android/ai tool plans/launcher admin/plan_forget_selected_files_confirm_20260602.md`
- `android/ai tool plans/launcher admin/plan_game_preferences_launcher_section_20260613.md`
- `android/ai tool plans/launcher admin/plan_gamepad_admin_tray_consolidation.md`
- `android/ai tool plans/launcher admin/plan_get_deps_linux_compatibility_20260524.md`
- `android/ai tool plans/launcher admin/plan_launcher_icons_20260507.md`
- `android/ai tool plans/launcher admin/plan_launcher_icons_tv_banner_fix_20260507.md`
- `android/ai tool plans/launcher admin/plan_launcher_touch_highlight_flash_20260526.md`
- `android/ai tool plans/launcher admin/plan_resume_recent_save_button_swap_20260526.md`
- `android/ai tool plans/launcher admin/plan_resume_save_thumbnail_fullscreen_20260526.md`
- `android/ai tool plans/launcher admin/plan_save_preview_thumbnail_2x_20260527.md`
- `android/ai tool plans/launcher admin/plan_skip_every_launch_ui_and_pref_sync.md`
- `android/ai tool plans/launcher admin/play-store-update-check.md`
- `android/ai tool plans/launcher admin/TOUCH_EDITOR_FIXES_PLAN.md`
- `android/ai tool plans/launcher admin/unified_launcher_game_scripting.md`
- `android/ai tool plans/launcher automation, editor, scripting/autoselect_refactor_plan.md`
- `android/ai tool plans/launcher automation, editor, scripting/button_based_launcher_automation.md`
- `android/ai tool plans/launcher automation, editor, scripting/feature_c_autoselect_editor.md`
- `android/ai tool plans/launcher automation, editor, scripting/plan_autoselect_newest_mismatch_20260604.md`
- `android/ai tool plans/launcher automation, editor, scripting/plan_autoselect_save_condition_from_shown_order_20260604.md`
- `android/ai tool plans/launcher automation, editor, scripting/plan_autoselect_save_only_mismatch_20260604.md`
- `android/ai tool plans/launcher automation, editor, scripting/plan_tv_perf_test_autosetup_20260520.md`
- `android/ai tool plans/launcher/af_d1_beta_boss_reactor_investigation_20260718.md`
- `android/ai tool plans/launcher/callsign_picker_create_foldin_20260708.md`
- `android/ai tool plans/launcher/large_level_metadata_progress_timeout_20260718.md`
- `android/ai tool plans/launcher/metadata_route_step_layout_20260708.md`
- `android/ai tool plans/launcher/plan_level_automap_preview_study_20260717.md`
- `android/ai tool plans/launcher/plan_menu_reload_progress_20260703.md`
- `android/ai tool plans/launcher/plan_metadata_browser_progress_20260704.md`
- `android/ai tool plans/launcher/plan_multiplayer_callsign_selection_20260703.md`
- `android/ai tool plans/launcher/plan_player_file_selectability_followup_20260703.md`
- `android/ai tool plans/launcher/plan_report_20260620_launcher_dpad.md`
- `android/ai tool plans/launcher/plan_report_20260620_next_failure.md`
- `android/ai tool plans/launcher/plan_report_20260620_one_failure.md`
- `android/ai tool plans/launcher/plan_save_explorer_coop_message_20260703.md`
- `android/ai tool plans/launcher/save_explorer_centered_tabs_indicators_20260708.md`
- `android/ai tool plans/launcher/save_explorer_choose_tab_20260708.md`
- `android/ai tool plans/launcher/save_explorer_compact_swipe_tabs_20260709.md`
- `android/ai tool plans/launcher/save_explorer_live_pager_tabs_20260708.md`
- `android/ai tool plans/launcher/save_explorer_pop_open_20260708.md`
- `android/ai tool plans/launcher/save_explorer_readable_scrolling_tabs_20260709.md`
- `android/ai tool plans/launcher/view_saves_collapsed_label_20260708.md`
- `android/ai tool plans/lunar_series_revamped_zip_parse_20260612.md`

### R1-CHUNK-0590

- `android/ai tool plans/metadata_crash_context_20260705.md`
- `android/ai tool plans/metadata_d2_level7_gold_key_20260705.md`
- `android/ai tool plans/metadata_resume_invalid_station_type_20260705.md`
- `android/ai tool plans/mission_json_name_regression_20260612.md`
- `android/ai tool plans/mission_json_stale_level_name_diff_20260612.md`
- `android/ai tool plans/mission_zip_batch_used_old_code_20260612.md`
- `android/ai tool plans/mixed batch, umbrella/build-info-admin-api-cancel-overlay-clog.md`
- `android/ai tool plans/mixed batch, umbrella/code_quality_and_test_lan_fix.md`
- `android/ai tool plans/mixed batch, umbrella/coop_indicator_and_controls_diag.md`
- `android/ai tool plans/mixed batch, umbrella/FIVE_FIXES_PLAN.md`
- `android/ai tool plans/mixed batch, umbrella/gog-installer-redbook-regression.md`
- `android/ai tool plans/mixed batch, umbrella/outstanding_bugs_triage_20260612.md`
- `android/ai tool plans/mixed batch, umbrella/outstanding_bugs_workplan.md`
- `android/ai tool plans/mixed batch, umbrella/plan_11_items_batch.md`
- `android/ai tool plans/mixed batch, umbrella/plan_7_items_round2.md`
- `android/ai tool plans/mixed batch, umbrella/plan_audio_import_advanced_tab_lan_discovery.md`
- `android/ai tool plans/mixed batch, umbrella/plan_automap_translation_speed_20260604.md`
- `android/ai tool plans/mixed batch, umbrella/plan_axis_music_touch.md`
- `android/ai tool plans/mixed batch, umbrella/plan_blown02_settings_rename_readme.md`
- `android/ai tool plans/mixed batch, umbrella/plan_controls_overlay_graphics_followups_20260522.md`
- `android/ai tool plans/mixed batch, umbrella/plan_d1d2_diff_shrink_net_udp_next_20260519.md`
- `android/ai tool plans/mixed batch, umbrella/plan_d1d2_diff_shrink_net_udp_observer_followup_20260519.md`
- `android/ai tool plans/mixed batch, umbrella/plan_energy_center_count_20260608.md`
- `android/ai tool plans/mixed batch, umbrella/plan_four_bugs_study.md`
- `android/ai tool plans/mixed batch, umbrella/plan_game_menu_guidebot_followups_20260522b.md`
- `android/ai tool plans/mixed batch, umbrella/plan_header_guards_and_net_udp_cleanup_20260519.md`
- `android/ai tool plans/mixed batch, umbrella/plan_inf_abyss_import_refresh_texture_audit_20260522.md`
- `android/ai tool plans/mixed batch, umbrella/plan_linux_root_build_script_20260524.md`
- `android/ai tool plans/mixed batch, umbrella/plan_nice_to_haves.md`
- `android/ai tool plans/mixed batch, umbrella/plan_outstanding_bugs_import_fire_resume_pilot_20260520.md`
- `android/ai tool plans/mixed batch, umbrella/plan_parameterize_mod_loading_tests.md`
- `android/ai tool plans/mixed batch, umbrella/plan_redbook_textures_diagnostics.md`
- `android/ai tool plans/mixed batch, umbrella/plan_shield_tv_launcher_followups.md`
- `android/ai tool plans/mixed batch, umbrella/study_secret_area_autolabel_20260606.md`
- `android/ai tool plans/mixed batch, umbrella/test_failures_20260612.md`
- `android/ai tool plans/music/plan_trine2_embedded_soundtrack_investigation_20260609.md`
- `android/ai tool plans/music/plan_zip_music_metadata_browser_study_20260609.md`
- `android/ai tool plans/networking/C4a_NAT_TRAVERSAL_TEST_PLAN.md`
- `android/ai tool plans/networking/C5_GPGS_SIGN_IN_PLAN.md`
- `android/ai tool plans/networking/cleanup_coop_files_move.md`
- `android/ai tool plans/networking/cleanup_net_udp_extract.md`
- `android/ai tool plans/networking/CLIENT_NETWORKING_PLAN.md`
- `android/ai tool plans/networking/coop_autosave_restore_existing_protocol_fix_20260708.md`
- `android/ai tool plans/networking/coop_desync_logging_20260704.md`
- `android/ai tool plans/networking/coop_guidebot_rejoin_exit_fixes.md`
- `android/ai tool plans/networking/coop_host_only_restore_fix_20260708.md`
- `android/ai tool plans/networking/coop_indicator_and_save_bugs.md`
- `android/ai tool plans/networking/coop_indicator_lines.md`
- `android/ai tool plans/networking/coop_inventory_preservation.md`
- `android/ai tool plans/networking/coop_restore_repeat_fire_fix.md`

### R1-CHUNK-0591

- `android/ai tool plans/networking/coop_sync_investigation.md`
- `android/ai tool plans/networking/coop-autosave-lobby.md`
- `android/ai tool plans/networking/coop-guidebot-and-host-migration.md`
- `android/ai tool plans/networking/coop-hmig-phase9-diagnostics.md`
- `android/ai tool plans/networking/coop-inventory-rejoin-research.md`
- `android/ai tool plans/networking/coop-lobby-save-offer.md`
- `android/ai tool plans/networking/coop-multi-slot-autosave.md`
- `android/ai tool plans/networking/coop-peer-status-broadcast.md`
- `android/ai tool plans/networking/coop-qol-features.md`
- `android/ai tool plans/networking/coop-restore-diagnostics-round3.md`
- `android/ai tool plans/networking/disk-dem-lansave.md`
- `android/ai tool plans/networking/fix_host_migration_pdata_loss.md`
- `android/ai tool plans/networking/fix_isyou_address_collision.md`
- `android/ai tool plans/networking/fix_mtu_truncation.md`
- `android/ai tool plans/networking/fix_test_mp_phase8.md`
- `android/ai tool plans/networking/fix-coop-invuln-faking.md`
- `android/ai tool plans/networking/guidebot coop resume desync fix 20260526.md`
- `android/ai tool plans/networking/guidebot coop resume desync investigation 20260526.md`
- `android/ai tool plans/networking/host_migration_and_guidebot_summary.md`
- `android/ai tool plans/networking/host_migration_pdata_conntype_diagnosis.md`
- `android/ai tool plans/networking/host_migration_proxy_plan.md`
- `android/ai tool plans/networking/HOST_MIGRATION_REJOIN_FIX.md`
- `android/ai tool plans/networking/lan_host_options_bottom_20260708.md`
- `android/ai tool plans/networking/lan_only_multiplayer_release_20260708.md`
- `android/ai tool plans/networking/MP_LOGGING_DEBUG_GUIDE.md`
- `android/ai tool plans/networking/multiplayer_prefs_persistence_20260703.md`
- `android/ai tool plans/networking/multiplayer-quick-resume-plan.md`
- `android/ai tool plans/networking/net_udp_cleanup_candidates.md`
- `android/ai tool plans/networking/NETWORKING_PLAN.md`
- `android/ai tool plans/networking/PHASE_C1_MATCHMAKING_CLIENT_PLAN.md`
- `android/ai tool plans/networking/PHASE_C2_C3_LOBBY_PLAN.md`
- `android/ai tool plans/networking/PHASE_C7_MESSAGING_PLAN.md`
- `android/ai tool plans/networking/PHASE2_3_REMAINING_PLAN.md`
- `android/ai tool plans/networking/phase2.5-server-implementation.md`
- `android/ai tool plans/networking/PHASE3_IDENTITY_ANTIABUSE_PLAN.md`
- `android/ai tool plans/networking/PHASE3_POW_AUTH_PLAN.md`
- `android/ai tool plans/networking/plan_5_mp_bugfixes.md`
- `android/ai tool plans/networking/plan_956_lan_mp_fixes.md`
- `android/ai tool plans/networking/plan_coop_desync_crash_log_analysis_20260617.md`
- `android/ai tool plans/networking/plan_coop_level_2_3_log_analysis_20260619.md`
- `android/ai tool plans/networking/plan_coop_rewind_save_load_20260701.md`
- `android/ai tool plans/networking/plan_coop_saves_callsign_guidebot.md`
- `android/ai tool plans/networking/plan_coop_start_fanout_clean_room_20260611.md`
- `android/ai tool plans/networking/plan_coop_start_fresh_level_reset_20260706.md`
- `android/ai tool plans/networking/plan_coop_start_fresh_savegame_20260618.md`
- `android/ai tool plans/networking/plan_desync_log_analysis_20260616.md`
- `android/ai tool plans/networking/plan_desync_resume_sync_implementation_20260616.md`
- `android/ai tool plans/networking/plan_fix_pdata_loss_after_host_migration.md`
- `android/ai tool plans/networking/plan_fix_proxy_crash_and_logs.md`
- `android/ai tool plans/networking/plan_guidebot_multiplayer.md`

### R1-CHUNK-0592

- `android/ai tool plans/networking/plan_ice_progress_ui.md`
- `android/ai tool plans/networking/plan_lan_directconnect_and_logging.md`
- `android/ai tool plans/networking/plan_lan_discovery_resume_self_heal_20260526.md`
- `android/ai tool plans/networking/plan_lan_games_autoselect_layout_cleanup.md`
- `android/ai tool plans/networking/plan_lan_join_by_ip_row_cleanup_20260526.md`
- `android/ai tool plans/networking/plan_lan_mp_fixes.md`
- `android/ai tool plans/networking/plan_lobby_unification.md`
- `android/ai tool plans/networking/plan_mp_bugfixes_round2.md`
- `android/ai tool plans/networking/plan_mp_bugfixes_round3.md`
- `android/ai tool plans/networking/plan_mp_test_fixes.md`
- `android/ai tool plans/networking/plan_multiplayer_100_spew_rate_20260527.md`
- `android/ai tool plans/networking/plan_multiplayer_controller_focus_20260527.md`
- `android/ai tool plans/networking/plan_multiplayer_resume_texture_palette_20260615.md`
- `android/ai tool plans/networking/plan_net_failure_log_analysis_20260615.md`
- `android/ai tool plans/networking/plan_networking_audit.md`
- `android/ai tool plans/networking/plan_networking_round4.md`
- `android/ai tool plans/networking/plan_player_spew_no_expire_20260616.md`
- `android/ai tool plans/networking/plan_relay_limit_ip_privacy_netlog.md`
- `android/ai tool plans/networking/plan_test_failures_20260619.md`
- `android/ai tool plans/networking/plan_test_mp_phase8_fix.md`
- `android/ai tool plans/networking/plan_thief_bot_multiplayer_drops_20260527.md`
- `android/ai tool plans/networking/plan_uuid_player_identity.md`
- `android/ai tool plans/networking/PLAN.BACKGROUND_RESUME_TEST.md`
- `android/ai tool plans/networking/plan.C4_GAME_LAUNCH.md`
- `android/ai tool plans/networking/PLAN.D1_D2_TOUCH_BINDINGS_FIXES.md`
- `android/ai tool plans/networking/PLAN.D1_PILOT_SEPARATION_TOUCH_BLACKSCREEN.md`
- `android/ai tool plans/networking/plan.ICE_STRATEGY_REWRITE.md`
- `android/ai tool plans/networking/PLAN.KEYBOARD_VIEWPORT_OFFSET.md`
- `android/ai tool plans/networking/plan.NAT_SIMULATOR.md`
- `android/ai tool plans/networking/plan.NAT_TRAVERSAL_IMPL.md`
- `android/ai tool plans/networking/rejoin-exit-diag-phase2.md`
- `android/ai tool plans/networking/rejoin-sync-diagnostics-v4.md`
- `android/ai tool plans/networking/rejoin-sync-fixes-build1115.md`
- `android/ai tool plans/networking/rejoin-sync-log-analysis-build1112.md`
- `android/ai tool plans/networking/relay-fix-plan.md`
- `android/ai tool plans/networking/relay-removal-and-test-dedup.md`
- `android/ai tool plans/networking/round4-mp-fixes.md`
- `android/ai tool plans/networking/study_guidebot_multiplayer.md`
- `android/ai tool plans/overlay, menu, etc/address-caching-overlay-fixes.md`
- `android/ai tool plans/overlay, menu, etc/admin_tray_pause_overlay_fixes.md`
- `android/ai tool plans/overlay, menu, etc/automap_next_objectives_position_20260715.md`
- `android/ai tool plans/overlay, menu, etc/centralize-touch-active-highlight-bindings.md`
- `android/ai tool plans/overlay, menu, etc/cockpit-hud-resolution-fix.md`
- `android/ai tool plans/overlay, menu, etc/crash_investigation_menu_close.md`
- `android/ai tool plans/overlay, menu, etc/darker-touch-active-highlight.md`
- `android/ai tool plans/overlay, menu, etc/debug_overlays.md`
- `android/ai tool plans/overlay, menu, etc/debug-logging-black-textures.md`
- `android/ai tool plans/overlay, menu, etc/demo-recording-active-highlight-state.md`
- `android/ai tool plans/overlay, menu, etc/etc2comp_hud_labels.md`
- `android/ai tool plans/overlay, menu, etc/fix-multi-item-menu-keyboard-viewport.md`

### R1-CHUNK-0593

- `android/ai tool plans/overlay, menu, etc/fix-overlay-color-and-texture-issues.md`
- `android/ai tool plans/overlay, menu, etc/font-rendering-deep-dive.md`
- `android/ai tool plans/overlay, menu, etc/frame-bar-supertrans-graphics-settings.md`
- `android/ai tool plans/overlay, menu, etc/graphics-page-msaa-af-persist.md`
- `android/ai tool plans/overlay, menu, etc/hires-texture-alpha-fix.md`
- `android/ai tool plans/overlay, menu, etc/hires-texture-gap-analysis.md`
- `android/ai tool plans/overlay, menu, etc/hires-transparency-perf-graphics.md`
- `android/ai tool plans/overlay, menu, etc/implement_touch_overlay_music_menu_refactor_20260612.md`
- `android/ai tool plans/overlay, menu, etc/LEVEL_NAME_OVERLAY_PLAN.md`
- `android/ai tool plans/overlay, menu, etc/menu_scale_ogl_plan_20260513.md`
- `android/ai tool plans/overlay, menu, etc/metl154_hires_premerge_fix.md`
- `android/ai tool plans/overlay, menu, etc/metl154_merge_clip_fix.md`
- `android/ai tool plans/overlay, menu, etc/metl154_plain_alpha_fix.md`
- `android/ai tool plans/overlay, menu, etc/metl154_post_merge_clip_followup.md`
- `android/ai tool plans/overlay, menu, etc/metl154_postfix_cleanup_and_debug_harness.md`
- `android/ai tool plans/overlay, menu, etc/metl154_prior_work.md`
- `android/ai tool plans/overlay, menu, etc/metl154_runtime_diagnostic.md`
- `android/ai tool plans/overlay, menu, etc/metl154_tranche1_rename_and_mode_cleanup.md`
- `android/ai tool plans/overlay, menu, etc/metl154_tranche2_hardcoded_diagnostic_removal.md`
- `android/ai tool plans/overlay, menu, etc/metl154_tranche3_shared_snapshot_and_regression.md`
- `android/ai tool plans/overlay, menu, etc/metl154_tranche4_shader_and_gl_state_audit.md`
- `android/ai tool plans/overlay, menu, etc/metl154_tranche5_cache_and_efficiency.md`
- `android/ai tool plans/overlay, menu, etc/metl154_tranche6_two_pass_cull_cleanup.md`
- `android/ai tool plans/overlay, menu, etc/metl154_tranche7_two_pass_polygon_offset_cleanup.md`
- `android/ai tool plans/overlay, menu, etc/metl154_tranche8_two_pass_debug_depth_cleanup.md`
- `android/ai tool plans/overlay, menu, etc/metl154_tranche9_launcher_debug_controls.md`
- `android/ai tool plans/overlay, menu, etc/metl154_visibility_backoff_and_occlusion_probe.md`
- `android/ai tool plans/overlay, menu, etc/metl154-mipmap-alpha-fix.md`
- `android/ai tool plans/overlay, menu, etc/mission_zip_soundtrack_unified_20260612.md`
- `android/ai tool plans/overlay, menu, etc/music_one_track_checkbox_snap_20260612.md`
- `android/ai tool plans/overlay, menu, etc/music_overlay_midi_to_mission_freeze_20260612.md`
- `android/ai tool plans/overlay, menu, etc/music_overlay_refresh_and_controls_20260612.md`
- `android/ai tool plans/overlay, menu, etc/music_overlay_source_dropdown_20260612.md`
- `android/ai tool plans/overlay, menu, etc/music_overlay_source_semantics_and_castaway_20260612.md`
- `android/ai tool plans/overlay, menu, etc/music_overlay_testing_fixes_20260612.md`
- `android/ai tool plans/overlay, menu, etc/net-stats-overlay-improvements.md`
- `android/ai tool plans/overlay, menu, etc/objective_automap_overlay_plan_20260711.md`
- `android/ai tool plans/overlay, menu, etc/objective_number_connector_and_metadata_info_20260715.md`
- `android/ai tool plans/overlay, menu, etc/ogl_merge_super_transparent.md`
- `android/ai tool plans/overlay, menu, etc/ogl_render_resolution.md`
- `android/ai tool plans/overlay, menu, etc/overlay_save_preview_supertransparency_research.md`
- `android/ai tool plans/overlay, menu, etc/overlay-fixes-and-selective-filtering.md`
- `android/ai tool plans/overlay, menu, etc/overlay-fixes-round2.md`
- `android/ai tool plans/overlay, menu, etc/overlay-launcher-graphics-settings.md`
- `android/ai tool plans/overlay, menu, etc/overlay-rendering-four-cases.md`
- `android/ai tool plans/overlay, menu, etc/phase4_metl154_regression_followup.md`
- `android/ai tool plans/overlay, menu, etc/phase5_metl154_new_angles_diagnosis.md`
- `android/ai tool plans/overlay, menu, etc/phase5-network-events-overlay.md`
- `android/ai tool plans/overlay, menu, etc/plan_afterburner_stick_highlight_fill_20260704.md`
- `android/ai tool plans/overlay, menu, etc/plan_android_original_graphics_options_reuse_study.md`

### R1-CHUNK-0594

- `android/ai tool plans/overlay, menu, etc/plan_automap_touch_overlay_rewrite_20260602.md`
- `android/ai tool plans/overlay, menu, etc/plan_autosave_thumbnail_reliability_research_20260525.md`
- `android/ai tool plans/overlay, menu, etc/plan_autoselect_dpad_reorder_20260602.md`
- `android/ai tool plans/overlay, menu, etc/plan_broad_android_border_backing_20260610.md`
- `android/ai tool plans/overlay, menu, etc/plan_controller_only_menu_buttons_overlay_20260703.md`
- `android/ai tool plans/overlay, menu, etc/plan_controller_only_warp_accept_unbound_20260527.md`
- `android/ai tool plans/overlay, menu, etc/plan_controls_editor_device_logging_20260603.md`
- `android/ai tool plans/overlay, menu, etc/plan_controls_editor_device_text_20260603.md`
- `android/ai tool plans/overlay, menu, etc/plan_controls_editor_scroll_20260603.md`
- `android/ai tool plans/overlay, menu, etc/plan_controls_editor_text_readability_20260603.md`
- `android/ai tool plans/overlay, menu, etc/plan_controls_menu_touch_coordinates_20260603.md`
- `android/ai tool plans/overlay, menu, etc/plan_cutscene_video_borders_20260526.md`
- `android/ai tool plans/overlay, menu, etc/plan_d1_difficulty_menu_tap_debug_20260619.md`
- `android/ai tool plans/overlay, menu, etc/plan_d1_render_crash_alignment.md`
- `android/ai tool plans/overlay, menu, etc/plan_d1_touch_afterburner_bar_20260621.md`
- `android/ai tool plans/overlay, menu, etc/plan_d2_skip_title_screen.md`
- `android/ai tool plans/overlay, menu, etc/plan_death_saying_keyboard_back_20260618.md`
- `android/ai tool plans/overlay, menu, etc/plan_diagnose_black_3d_textures.md`
- `android/ai tool plans/overlay, menu, etc/plan_door45_base_texture_pose_repro.md`
- `android/ai tool plans/overlay, menu, etc/plan_door45_paged_out_buf_stale.md`
- `android/ai tool plans/overlay, menu, etc/plan_door45_single_draw_state_reset.md`
- `android/ai tool plans/overlay, menu, etc/plan_dpad_text_input_nav_audit_20260602.md`
- `android/ai tool plans/overlay, menu, etc/plan_energy_shield_hold_20260610.md`
- `android/ai tool plans/overlay, menu, etc/plan_energy_shield_unbound_message_20260606.md`
- `android/ai tool plans/overlay, menu, etc/plan_etc2_to_ktx2_migration.md`
- `android/ai tool plans/overlay, menu, etc/plan_exit_overlay_button_20260526.md`
- `android/ai tool plans/overlay, menu, etc/plan_extract_reorder_helper_20260604.md`
- `android/ai tool plans/overlay, menu, etc/plan_fix_host_build_blank_thumbnail_20260513.md`
- `android/ai tool plans/overlay, menu, etc/plan_fix_msaa_missile_glitch.md`
- `android/ai tool plans/overlay, menu, etc/plan_general_touch_region_diagnostics_20260605.md`
- `android/ai tool plans/overlay, menu, etc/plan_gles3_etc2_hires_textures.md`
- `android/ai tool plans/overlay, menu, etc/plan_guidebot_d1_vs_d2_texture_feasibility_20260613.md`
- `android/ai tool plans/overlay, menu, etc/plan_guidebot_locked_spawn_20260606.md`
- `android/ai tool plans/overlay, menu, etc/plan_guidebot_nearest_point_navigation_20260611.md`
- `android/ai tool plans/overlay, menu, etc/plan_guidebot_progress_text_investigation_20260714.md`
- `android/ai tool plans/overlay, menu, etc/plan_guidebot_wheel_next_release_20260704.md`
- `android/ai tool plans/overlay, menu, etc/plan_guidebot_wheel_unlock_next_goal_20260703.md`
- `android/ai tool plans/overlay, menu, etc/plan_hidden_cover_focus_audit.md`
- `android/ai tool plans/overlay, menu, etc/plan_hud_pip_rear_view_research_20260613.md`
- `android/ai tool plans/overlay, menu, etc/plan_hud_strips_overlay_button.md`
- `android/ai tool plans/overlay, menu, etc/plan_in_game_difficulty_change_20260604.md`
- `android/ai tool plans/overlay, menu, etc/plan_in_game_fov_slider_20260604.md`
- `android/ai tool plans/overlay, menu, etc/plan_indicator_line_bold_warp_liberal_20260703.md`
- `android/ai tool plans/overlay, menu, etc/plan_indicator_line_fixes.md`
- `android/ai tool plans/overlay, menu, etc/plan_indicator_line_thickness_20260703.md`
- `android/ai tool plans/overlay, menu, etc/plan_ingame_autoselect_long_press_drag_20260604.md`
- `android/ai tool plans/overlay, menu, etc/plan_ingame_load_save_preview_highlight_20260605.md`
- `android/ai tool plans/overlay, menu, etc/plan_ingame_menu_text_readability_20260603.md`
- `android/ai tool plans/overlay, menu, etc/plan_intro_movie_loading_android.md`
- `android/ai tool plans/overlay, menu, etc/plan_intro_skip_input_relaunch_fix.md`

### R1-CHUNK-0595

- `android/ai tool plans/overlay, menu, etc/plan_intro_timeout_batch_20260515.md`
- `android/ai tool plans/overlay, menu, etc/plan_landscape_portrait_ui_fixes.md`
- `android/ai tool plans/overlay, menu, etc/plan_launcher_outline_menu_fixes_20260602.md`
- `android/ai tool plans/overlay, menu, etc/plan_level_complete_stats_page_20260515.md`
- `android/ai tool plans/overlay, menu, etc/plan_menu_loading_texture_filter_regression_20260508.md`
- `android/ai tool plans/overlay, menu, etc/plan_merged_wall_texture_shift_diagnostic.md`
- `android/ai tool plans/overlay, menu, etc/plan_mine_exit_movie_skip.md`
- `android/ai tool plans/overlay, menu, etc/plan_minimize_save_thumbnails_20260514.md`
- `android/ai tool plans/overlay, menu, etc/plan_modal_border_static_survey_20260610.md`
- `android/ai tool plans/overlay, menu, etc/plan_multiplayer_abdicate_guidebot_settings_20260527.md`
- `android/ai tool plans/overlay, menu, etc/plan_multiplayer_join_wait_background_20260526.md`
- `android/ai tool plans/overlay, menu, etc/plan_multiplayer_stats_overlay.md`
- `android/ai tool plans/overlay, menu, etc/plan_overlay_menu_followups_20260522b.md`
- `android/ai tool plans/overlay, menu, etc/plan_oversized_texture_fix.md`
- `android/ai tool plans/overlay, menu, etc/plan_pilot_select_touch_delete_filter_20260605.md`
- `android/ai tool plans/overlay, menu, etc/plan_pilot_touch_log_analysis_20260606.md`
- `android/ai tool plans/overlay, menu, etc/plan_pilot_touch_second_log_analysis_20260606.md`
- `android/ai tool plans/overlay, menu, etc/plan_preexisting_bug_triage_20260601.md`
- `android/ai tool plans/overlay, menu, etc/plan_radiused_corner_hud_text_inset_20260602.md`
- `android/ai tool plans/overlay, menu, etc/plan_relax_hires_texture_limits.md`
- `android/ai tool plans/overlay, menu, etc/plan_robot_hostage_hud_counts_20260601.md`
- `android/ai tool plans/overlay, menu, etc/plan_robot_total_runtime_spawns_20260603.md`
- `android/ai tool plans/overlay, menu, etc/plan_rounded_corner_hud_text_inset_20260602.md`
- `android/ai tool plans/overlay, menu, etc/plan_rounded_corner_hud_text_inset_tristate_20260602.md`
- `android/ai tool plans/overlay, menu, etc/plan_save_game_default_level_name_20260606.md`
- `android/ai tool plans/overlay, menu, etc/plan_score_added_counter_stack_20260602.md`
- `android/ai tool plans/overlay, menu, etc/plan_selective_android_frame_clear_20260610.md`
- `android/ai tool plans/overlay, menu, etc/plan_shield_gap_brightness_and_perf_probe_20260520.md`
- `android/ai tool plans/overlay, menu, etc/plan_shield_graphics_perf_20260521.md`
- `android/ai tool plans/overlay, menu, etc/plan_shield_keyboard_gap_and_texture_load_20260520.md`
- `android/ai tool plans/overlay, menu, etc/plan_shield_texture_log_analysis_20260520.md`
- `android/ai tool plans/overlay, menu, etc/plan_skippable_intro_movies.md`
- `android/ai tool plans/overlay, menu, etc/plan_start_game_overlay_select_players_20260526.md`
- `android/ai tool plans/overlay, menu, etc/plan_startup_error_static_background_20260607.md`
- `android/ai tool plans/overlay, menu, etc/plan_stats_persistence_20260608.md`
- `android/ai tool plans/overlay, menu, etc/plan_tex_naming_128_readme_mine_exit.md`
- `android/ai tool plans/overlay, menu, etc/plan_texture_loading_diagnostics.md`
- `android/ai tool plans/overlay, menu, etc/plan_texture_scan_netstats_debuglog_shader_warnings.md`
- `android/ai tool plans/overlay, menu, etc/plan_three_autosave_categories_20260525.md`
- `android/ai tool plans/overlay, menu, etc/plan_thumbnail_cleanup_20260514.md`
- `android/ai tool plans/overlay, menu, etc/plan_touch_editor_dpad_navigation_20260602.md`
- `android/ai tool plans/overlay, menu, etc/plan_touch_pip_window_cycle_buttons_20260613.md`
- `android/ai tool plans/overlay, menu, etc/plan_transparency_effects_menu_freeze_20260602.md`
- `android/ai tool plans/overlay, menu, etc/plan_video_overlay_af_msaa_live_apply_research_20260524.md`
- `android/ai tool plans/overlay, menu, etc/plan_video_overlay_brightness_20260523.md`
- `android/ai tool plans/overlay, menu, etc/plan_video_overlay_height_scaling_20260524.md`
- `android/ai tool plans/overlay, menu, etc/plan_weapon_wheel_ammo_status_20260601.md`
- `android/ai tool plans/overlay, menu, etc/plan-cockpit-fix-overlay-consolidation-build-step.md`
- `android/ai tool plans/overlay, menu, etc/plan-cutscene-death-overlays-mouse-mode-autoselect.md`
- `android/ai tool plans/overlay, menu, etc/plan-etc2-black-texture-diagnostics.md`

### R1-CHUNK-0596

- `android/ai tool plans/overlay, menu, etc/plan-video-overlay-stats-etc2-hang.md`
- `android/ai tool plans/overlay, menu, etc/save_music_preferences_to_player_file_20260612.md`
- `android/ai tool plans/overlay, menu, etc/SKIP_BUTTON_PLAN.md`
- `android/ai tool plans/overlay, menu, etc/strip-split-and-tmap2-labels.md`
- `android/ai tool plans/overlay, menu, etc/study_cheats_menu_redesign_20260608.md`
- `android/ai tool plans/overlay, menu, etc/study_touch_overlay_music_menu_refactor_20260612.md`
- `android/ai tool plans/overlay, menu, etc/super_transparent_mask_fix.md`
- `android/ai tool plans/overlay, menu, etc/supertransparent_threshold_docs_cleanup.md`
- `android/ai tool plans/overlay, menu, etc/texture_visual_issues_investigation.md`
- `android/ai tool plans/overlay, menu, etc/ui_cleanup_batch_2.md`
- `android/ai tool plans/overlay, menu, etc/ui_cleanup_batch.md`
- `android/ai tool plans/overlay, menu, etc/VIDEO_INFO_OVERLAY_PLAN.md`
- `android/ai tool plans/overlay, menu, etc/video_overlay_admin_tray_race_and_windows_build_helper.md`
- `android/ai tool plans/plan_mission_descriptor_file_details_20260605.md`
- `android/ai tool plans/plan_mission_zip_readme_view_20260606.md`
- `android/ai tool plans/plan_sectorgame_zip_mission_import_20260604.md`
- `android/ai tool plans/plan_test_saf_archiver_report_20260523_232445.md`
- `android/ai tool plans/plan_uneasy_mission_zip_ogg_music_20260606.md`
- `android/ai tool plans/plan_uneasy_zip_hog_level_loading_20260605.md`
- `android/ai tool plans/plan_uneasy_zip_hog_realpath_fix_20260605.md`
- `android/ai tool plans/plan_zip_mod_file_button_clipping_20260605.md`
- `android/ai tool plans/release_upload_build_stamp_20260603.md`
- `android/ai tool plans/reusable/cleanup.md`
- `android/ai tool plans/reusable/demo-file-format-reference.md`
- `android/ai tool plans/reusable/fp-determinism-survey-20260428-report.md`
- `android/ai tool plans/reusable/fp-determinism-survey-20260428.md`
- `android/ai tool plans/reusable/fp-determinism-test-notes-20260429.md`
- `android/ai tool plans/reusable/plan_reusable_cleanup_instructions_20260517.md`
- `android/ai tool plans/reusable/polling-not-timeouts.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_cd_storage_pilot_delete_20260520.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_d1_level14_route_debug_20260608.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_door45_and_saf_archiver_triage_20260516.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_level_metadata_crash_repro_fix_20260608.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_level_metadata_implementation_20260608.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_level_metadata_native_safety_study_20260608.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_level_metadata_route_completion_20260608.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_level_metadata_scroll_ui_20260608.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_level_metadata_view_study_20260608.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_level_metadata_volume_travel_time_20260608.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_masked_save_sets_and_explorer_20260606.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_metadata_start_secret_diff_20260608.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_metadata_volume_display_precision_20260608.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_missing_reactor_travel_note_20260608.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_obsidian_trigger_wall_travel_20260608.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_periodic_android_autosaves_20260606.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_plutonian_shores_unreachable_research_20260610.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_reactor_shootable_route_metadata_20260611.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_resume_recent_scoped_save_crash_20260606.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_resume_save_logging_cleanup_20260606.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_saf_archiver_wrapper_hang_20260516.md`

### R1-CHUNK-0597

- `android/ai tool plans/storage, SAF, edge cases/plan_saf_link_removal_and_cd_filter_20260507.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_save_explorer_base_save_visibility_20260606.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_save_explorer_default_recent_sort_20260606.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_save_explorer_detail_popover_20260607.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_save_explorer_most_recent_default_20260607.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_save_explorer_recent_dedup_20260606.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_save_structure_resume_regression_20260608.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_unified_free_space_preflight_20260611.md`
- `android/ai tool plans/storage, SAF, edge cases/study_level_metadata_volume_travel_time_20260608.md`
- `android/ai tool plans/test runner/plan_run_all_tests_ctrl_c_20260619.md`
- `android/ai tool plans/testing/audit_powershell_cwd_leaks_20260707.md`
- `android/ai tool plans/testing/auto-infra-test-suite.md`
- `android/ai tool plans/testing/demo-regression-testing.md`
- `android/ai tool plans/testing/draft_copilot_instructions_additions.md`
- `android/ai tool plans/testing/fix_host_metadata_helper_cwd_20260707.md`
- `android/ai tool plans/testing/fix_test_failures_20260329.md`
- `android/ai tool plans/testing/fix-test-failures-20260328.md`
- `android/ai tool plans/testing/fix-test-failures-20260406.md`
- `android/ai tool plans/testing/fix-test-suite-failures.md`
- `android/ai tool plans/testing/full_mission_metadata_regeneration_20260705.md`
- `android/ai tool plans/testing/host_metadata_regeneration_implementation_20260705.md`
- `android/ai tool plans/testing/host_metadata_regeneration_repair_20260705.md`
- `android/ai tool plans/testing/host_mission_metadata_regeneration_design_20260705.md`
- `android/ai tool plans/testing/level_metadata_json_normalization_plan_20260705.md`
- `android/ai tool plans/testing/manual_metadata_regeneration_progress_20260705.md`
- `android/ai tool plans/testing/metadata_regeneration_script_docs_20260705.md`
- `android/ai tool plans/testing/mission_metadata_numeric_canonicalization_20260718.md`
- `android/ai tool plans/testing/plan_autosave_resume_test_triage_20260516b.md`
- `android/ai tool plans/testing/plan_autosave_resume_timeout_20260621.md`
- `android/ai tool plans/testing/plan_cleanup_report_quibbles_20260628_195417.md`
- `android/ai tool plans/testing/plan_coop_start_metadata_20260609.md`
- `android/ai tool plans/testing/plan_engine_prefs_test_triage_20260516.md`
- `android/ai tool plans/testing/plan_failure_fault_tolerance_20260621.md`
- `android/ai tool plans/testing/plan_fault_tolerance_report_20260621_140747.md`
- `android/ai tool plans/testing/plan_fire_primary_test_probe_20260516.md`
- `android/ai tool plans/testing/plan_fix_test_failures_20260409_round2.md`
- `android/ai tool plans/testing/plan_fix_test_failures_20260409.md`
- `android/ai tool plans/testing/plan_full_suite_rerun_after_demo_cleanup_20260517.md`
- `android/ai tool plans/testing/plan_game_data_index_canonicalization_20260526.md`
- `android/ai tool plans/testing/plan_game_data_index_minimal_test_deps_20260526.md`
- `android/ai tool plans/testing/plan_get_deps_helpers_reorg_20260526.md`
- `android/ai tool plans/testing/plan_gog_regression_launch_timeout_cleanup_20260526.md`
- `android/ai tool plans/testing/plan_intro_test_cleanup.md`
- `android/ai tool plans/testing/plan_json_baseline_pretty_print_20260609.md`
- `android/ai tool plans/testing/plan_json_output_normalization_in_tests_20260609.md`
- `android/ai tool plans/testing/plan_levelcomplete_touch_skip_triage_20260516.md`
- `android/ai tool plans/testing/plan_linux_data_recovery_helpers_20260526.md`
- `android/ai tool plans/testing/plan_linux_regression_testing_compat_20260525.md`
- `android/ai tool plans/testing/plan_merge_conflict_cleanup_20260526.md`
- `android/ai tool plans/testing/plan_merged_wall_next_failure_20260628.md`

### R1-CHUNK-0598

- `android/ai tool plans/testing/plan_merged_wall_two_pass_geometry_contract_20260624.md`
- `android/ai tool plans/testing/plan_merged_wall_two_pass_probe_20260624.md`
- `android/ai tool plans/testing/plan_mission_zip_batch_d1_d2_detection_20260609.md`
- `android/ai tool plans/testing/plan_mission_zip_batch_emulator_recovery_and_crashes_20260609.md`
- `android/ai tool plans/testing/plan_mission_zip_batch_manual_run_20260609.md`
- `android/ai tool plans/testing/plan_mission_zip_batch_metadata_launch_20260608.md`
- `android/ai tool plans/testing/plan_mission_zip_regression_gitignore_20260609.md`
- `android/ai tool plans/testing/plan_mission_zip_regression_output_and_assets_20260609.md`
- `android/ai tool plans/testing/plan_next_failing_tests_20260516.md`
- `android/ai tool plans/testing/plan_next_failure_transform_20260628.md`
- `android/ai tool plans/testing/plan_next_remaining_failure_20260628.md`
- `android/ai tool plans/testing/plan_pilot_long_hold_delete_hardening_20260624.md`
- `android/ai tool plans/testing/plan_preexisting_test_fixes.md`
- `android/ai tool plans/testing/plan_quick_record_sidecar_hardening_20260624.md`
- `android/ai tool plans/testing/plan_quick_record_sidecar_install_hardening_20260624.md`
- `android/ai tool plans/testing/plan_quick_tests_historical_seconds_20260701.md`
- `android/ai tool plans/testing/plan_recent_test_report_fixes_20260612.md`
- `android/ai tool plans/testing/plan_remaining_test_failures_20260516.md`
- `android/ai tool plans/testing/plan_remaining_test_failures_fresh_report_round_20260517.md`
- `android/ai tool plans/testing/plan_report_20260518_223317_fixes.md`
- `android/ai tool plans/testing/plan_report_20260519_232520_fixes.md`
- `android/ai tool plans/testing/plan_report_20260520_232305_triage.md`
- `android/ai tool plans/testing/plan_report_20260522_232545_followup.md`
- `android/ai tool plans/testing/plan_report_20260526_144107_windows_compat.md`
- `android/ai tool plans/testing/plan_report_20260528_115705_failures.md`
- `android/ai tool plans/testing/plan_report_20260528_153202_axis_mapping.md`
- `android/ai tool plans/testing/plan_report_20260528_201452_failures.md`
- `android/ai tool plans/testing/plan_report_20260620_153831_state_cluster.md`
- `android/ai tool plans/testing/plan_report_20260624_201136_hardening.md`
- `android/ai tool plans/testing/plan_report_20260627_231136_hardening.md`
- `android/ai tool plans/testing/plan_report_20260627_231136_remaining_hardening.md`
- `android/ai tool plans/testing/plan_report_20260628_131121_followup.md`
- `android/ai tool plans/testing/plan_report_20260628_151027_followup.md`
- `android/ai tool plans/testing/plan_report_20260628_195417_audit.md`
- `android/ai tool plans/testing/plan_report_20260628_222248_timeout_failure.md`
- `android/ai tool plans/testing/plan_report_triage_20260516.md`
- `android/ai tool plans/testing/plan_resolution_test_triage_20260516.md`
- `android/ai tool plans/testing/plan_reticle_options_test_triage_20260516.md`
- `android/ai tool plans/testing/plan_reusable_level_metadata_zip_repro_20260608.md`
- `android/ai tool plans/testing/plan_run_all_extract_subset_research_20260526.md`
- `android/ai tool plans/testing/plan_run_all_tests_failure_followup_20260617.md`
- `android/ai tool plans/testing/plan_run_all_tests_input_demo_matrix_20260617.md`
- `android/ai tool plans/testing/plan_run_all_tests_linux_20260525.md`
- `android/ai tool plans/testing/plan_run_all_tests_linux_20260602.md`
- `android/ai tool plans/testing/plan_run_all_tests_prereq_preflight_20260515.md`
- `android/ai tool plans/testing/plan_run_quick_tests_20260519.md`
- `android/ai tool plans/testing/plan_run_testmenu_mission_zip_batch_20260609.md`
- `android/ai tool plans/testing/plan_saf_basic_test_triage_20260516.md`
- `android/ai tool plans/testing/plan_test_abort_game_to_main_menu_d2_report_20260523_114346.md`
- `android/ai tool plans/testing/plan_test_centralization_survey_20260525.md`

### R1-CHUNK-0599

- `android/app/src/main/cpp/extract/test/data/bad_backwards_index.cue`
- `android/app/src/main/cpp/extract/test/data/bad_empty.cue`
- `android/app/src/main/cpp/extract/test/data/bad_garbage_msf.cue`
- `android/app/src/main/cpp/extract/test/data/bad_missing_index.cue`
- `android/app/src/main/cpp/extract/test/data/bad_no_file_directive.cue`
- `android/app/src/main/cpp/extract/test/data/bad_start_past_eof.cue`
- `android/app/src/main/cpp/extract/test/data/bad_unclosed_quote.cue`
- `android/app/src/main/cpp/extract/test/data/valid_data_plus_audio.cue`
- `android/app/src/main/cpp/extract/test/data/valid_extra_whitespace.cue`
- `android/app/src/main/cpp/extract/test/data/valid_many_audio.cue`
- `android/app/src/main/cpp/extract/test/data/valid_mode2.cue`
- `android/app/src/main/cpp/extract/test/data/valid_multi_file.cue`
- `android/app/src/main/cpp/extract/test/data/valid_pregap.cue`
- `android/app/src/main/cpp/extract/test/data/valid_single_data.cue`
- `android/docker/nat-testbed/Dockerfile`
- `android/gradlew`
- `android/tests/test_redbook_data/test_disc.cue`
- `android/tests/udp_test_tool_android.csrc`
- `game_data_to_copy_to_emulator/data/.gitkeep`
- `game_data_to_copy_to_emulator/download/.gitkeep`
- `server/config.json5.lan`

## Chunk completion notes

Append one completion note for every finished queue item using the process template

### R1-PREFLIGHT-001 completion

- Completed: 2026-07-19
- Model: sol-5.6-medium
- Result: BR-0001, BR-0002, BR-0003, INV-0001
- Assigned scope checked: complete frozen diff composition, provenance, generated artifacts, secret-bearing content, license surface, and upstream split strategy
- Context checked: all 2,638 changed paths by numstat and blob size, 1,076-commit history shape, tracked and ignored status of suspicious outputs, runtime database schema, credential templates, server packaging metadata, root license, generated-data provenance markers, and representative historical plans
- Commands or validation: `git diff --numstat`, `git diff --name-only`, `git ls-tree -r -l`, `git check-ignore -v`, `git grep`, `git log -G`, `git show`, and binary-safe `rg -a` inspection against `fb555eec75e1..c01d8fe4686c`
- No-finding areas: no real credential was found by the targeted head or history marker scans; checked-in credential files contained placeholders; generated disc and fixture data named generator scripts or provenance in representative samples; no submodules or Git LFS pointers were introduced

### R1-PREFLIGHT-002 completion

- Completed: 2026-07-19
- Model: sol-5.6-medium
- Result: BR-0004, BR-0005
- Assigned scope checked: complete frozen architecture, subsystem ownership, process and language boundaries, entry points, and highest-risk data flows
- Context checked: default-process launcher and file management; separate `:game`, level-preview, and level-metadata processes; D1/D2 native libraries and shared Android C/C++ layer; Kotlin/JNI configuration helpers; matchmaking WebSocket, UDP proxy, LAN discovery, and Rust server; archive and SAF import paths; automation broadcasts; generated metadata and regression infrastructure
- Commands or validation: `git diff --name-only`, `git diff --name-status`, `git ls-tree`, `git show`, and `git grep` against `fb555eec75e1..c01d8fe4686c`, plus schema-literal comparison of Kotlin and Rust protocol messages and line-numbered source-to-sink traces across Android lifecycle and server state transitions
- No-finding areas: D1/D2 are built as separate libraries with the common Android-native source list attached to both; game identity is carried across launcher, LAN, matchmaking, and server structures; native metadata analysis services are non-exported and isolated by game; representative pilot, save, autoselect, and engine-preference mutations call native helpers rather than duplicating binary layouts in Kotlin; archive and SAF inputs are staged through dedicated import layers; no additional root-cause architecture finding was admitted for inactive protocol variants or file-size alone

### R1-PREFLIGHT-003 completion

- Completed: 2026-07-19
- Model: sol-5.6-medium
- Result: BR-0006
- Assigned scope checked: all 1,076 commits in the frozen range for feature clusters, high-churn paths, generic or repeated commit subjects, deleted paths, superseded implementations, partial migrations, compatibility fallbacks, and abandoned protocol surfaces
- Context checked: 9 merge commits; repeated history labels including 32 `test fixes`, 29 `pathing`, 25 `more`, 23 `cleanup`, and 18 `fixes`; major music/audio, touch/input, metadata/pathing, demo/replay, multiplayer, graphics, extraction, save/config, build, and D1-in-D2 clusters; 73 deleted non-plan paths; 238 commits touching `SetupActivity.kt`, 196 touching `MainActivity.kt`, and 115 touching the final introspection implementation; the C-to-C++ introspection conversion and later D1/D2 shared-source move; emulator helper removal; generated `BuildInfo.kt` and Gradle wrapper handling; JSON-to-JSON5 automation and disc-database transitions; shared-native deduplication; launcher data migrations; and server protocol variants
- Commands or validation: `git log` subject, path-frequency, merge, rename, deletion, and pickaxe scans; `git show`; `git blame`; `git diff --name-status`; `git ls-tree`; `git cat-file -e`; and final-tree `git grep` against `fb555eec75e1..c01d8fe4686c`
- No-finding areas: BR-0002 already captures the unreviewable generic commit history and clean-series requirement; deleted native sources generally resolve to linked shared replacements; generated `BuildInfo.kt` and the Gradle wrapper have regeneration paths; removed test scripts are not called by the final tree; launcher preference and file-layout migrations are active, versioned, or retain required user-data compatibility; dormant server `VERSION_WARNING` and match-result messages were not admitted without a concrete broken client flow

### R1-CHUNK-0001 completion

- Completed: 2026-07-19
- Model: sol-5.6-medium
- Result: BR-0007
- Assigned scope checked: all 45 assigned lines in `android/auth_config.json5.template` and `android/play-store-credentials.sample.json`
- Context checked: frozen additions and history, `.gitignore` coverage, Gradle parsing and generated resource/build-config consumers, Google Play Games authentication call sites, Play Store upload entry points, shared JWT signing helper, JSON syntax, placeholder values, PEM envelope handling, and both modern and Windows PowerShell fallback RSA import paths
- Commands or validation: `git diff`, `git show`, `git log --full-history`, `git grep`, `git check-ignore -v`, PowerShell `ConvertFrom-Json`, and an in-memory generated RSA key test comparing PKCS#1 and PKCS#8 imports through the exact helper paths
- No-finding areas: both real credential filenames are ignored; the checked-in samples contain placeholders rather than credentials; the auth template fields are intentionally public identifiers and match their Gradle, manifest, and Kotlin consumers; both files parse as intended; no secret-handling or Google Play Games configuration defect was found in the assigned lines

### R1-CHUNK-0002 completion

- Completed: 2026-07-19
- Model: sol-5.6-medium
- Result: BR-0008, BR-0009, BR-0010
- Assigned scope checked: all 566 assigned lines across 12 extraction regression specs and the D1 Mac `track_hashes.json` and `track_fingerprints.json`
- Context checked: frozen additions and history; JSON/JSON5 parsing; canonical spec writer; spec generator; CUE reference validator; direct CUE/BIN and ISO test paths; result persistence and suite aggregation; all locally available assigned source images and extracted `data_tracks`; `known_discs.json5`; and D1 Mac track identity, audio metadata, and fingerprint records
- Commands or validation: `git diff`, `git show`, `git log --full-history`, `git grep`, `rg`, PowerShell JSON parsing and invariant scripts, SHA-256 hashing of all 23 assigned source files, cross-file SHA-1 comparison for all 14 D1 Mac tracks, and `android/tests/validate_extract_regression_specs.ps1` which validated all 34 available CD specs
- No-finding areas: every assigned file parses; all 23 source hashes match the available CUE/BIN/ISO files; all recorded extracted-file counts and required filenames match `data_tracks`; every disc ID has one matching database entry with the same game and audio-track count; the D1 Mac hash and fingerprint files have identical contiguous tracks, types, and SHA-1 values with no duplicates or extras; all audio entries have a nonempty chromaprint and positive duration; CUE references, import modes, and source-file existence pass the repository validator

### R1-CHUNK-0003 completion

- Completed: 2026-07-19
- Model: sol-5.6-medium
- Result: none
- Assigned scope checked: all 485 assigned lines across six extraction regression specs and the Destination Quartzon `track_fingerprints.json`
- Context checked: frozen additions and history; JSON/JSON5 parsing; canonical spec writer; spec generator and validator; direct CUE/BIN test paths; locally available source images, CUE references, extracted `data_tracks`, and `track_hashes.json` files; `known_discs.json5`; Destination Quartzon track identity and audio metadata; and the shared Descent II v1.1/Infinite Abyss disc identity
- Commands or validation: `git diff`, `git show`, `git log --full-history`, `git grep`, `rg`, PowerShell JSON parsing and invariant scripts, SHA-256 hashing of all 48 assigned source files, SHA-1 comparison among Destination Quartzon fingerprints, hashes, and database tracks, full extracted-tree and track-hash comparison for Descent II v1.1 and Infinite Abyss, and `android/tests/validate_extract_regression_specs.ps1` which validated all 34 available CD specs
- No-finding areas: every assigned file parses; all 48 source hashes match the available CUE/BIN/IMG files; every disc ID has one matching database entry with the same game and audio-track count; all expected files are present in local extraction output and the file-only Dimensions count matches its 88-file threshold; Destination Quartzon has 13 contiguous tracks whose types and SHA-1 values exactly match its hash manifest and database, with a nonempty chromaprint and positive duration on every audio track; its CUE references only the listed IMG payload; the v1.1 and Infinite Abyss aliases have identical nine-track hashes and identical 476-file extracted trees; runner-level weaknesses were deduplicated to BR-0008, BR-0009, and BR-0010

### R1-CHUNK-0004 completion

- Completed: 2026-07-19
- Model: sol-5.6-medium
- Result: BR-0011
- Assigned scope checked: all 516 assigned lines across eight extraction regression specs
- Context checked: frozen additions and full file histories; JSON/JSON5 parsing; source CUE/BIN payloads and references; local `data_tracks`, `track_hashes.json`, and `track_fingerprints.json` outputs; `known_discs.json5`; the exact disc-identification algorithm and hashing documentation; regression result and launch logic; and the regression-spec validator
- Commands or validation: `git diff`, `git show`, `git log --follow --full-history`, `git grep`, `rg`, PowerShell JSON parsing and invariant scripts, SHA-256 hashing of all 70 assigned source files, per-track SHA-1 comparison among manifests, fingerprints, and database records, and `android/tests/validate_extract_regression_specs.ps1` which validated all 34 available CD specs
- No-finding areas: every assigned file parses; all 70 source hashes match the available CUE/BIN files; all CUE references and import modes pass validation; all expected extraction files are present locally; all eight track hash and fingerprint manifests agree exactly and contain contiguous track records with complete audio fingerprints and positive durations; six specs identify database records whose complete track manifests match; source integrity and incomplete-import behavior were deduplicated to BR-0008 and BR-0010

### R1-CHUNK-0005 completion

- Completed: 2026-07-19
- Model: sol-5.6-medium
- Result: BR-0011, BR-0012
- Assigned scope checked: all 575 assigned lines across eight extraction regression specs
- Context checked: frozen additions and full file histories; JSON/JSON5 parsing; source CUE/BIN payloads and references; local `data_tracks`, `track_hashes.json`, and `track_fingerprints.json` outputs; `known_discs.json5`; exact and partial disc-identification behavior; setup introspection; regression result persistence; preview-disc handling; direct-import and launch decisions; and the regression-spec validator
- Commands or validation: `git diff`, `git show`, `git log --follow --full-history`, `git grep`, `rg`, PowerShell JSON parsing and invariant scripts, SHA-256 hashing of all 84 assigned source files, per-track SHA-1 comparison among manifests, fingerprints, and database records, and `android/tests/validate_extract_regression_specs.ps1` which validated all 34 available CD specs
- No-finding areas: every assigned file parses; all 84 source hashes match the available CUE/BIN files; all CUE references and import modes pass validation; all expected extraction files are present locally; every track hash and fingerprint manifest agrees exactly and contains contiguous track records with complete audio fingerprints and positive durations; seven of eight specs identify database records whose complete manifests match; preview launch exclusion is explicit; source integrity, level mismatch, and incomplete-import behavior were deduplicated to BR-0008, BR-0009, and BR-0010

### R1-CHUNK-0006 completion

- Completed: 2026-07-19
- Model: sol-5.6-medium
- Result: none
- Assigned scope checked: all 241 assigned lines across nine mission-archive music fingerprint metadata files, comprising 160 track records
- Context checked: frozen additions and full file histories; JSON5 parsing and metadata invariants; all nine locally available ZIP/7z source archives; extracted OGG inventories; the mission-music fingerprint generator; album database merger and CD-track deduplication; generated `known_discs.json5` album records; Kotlin database flattening; native fingerprint tie-breaking; and launcher consumers of matched names and track numbers
- Commands or validation: `git diff`, `git show`, `git log --follow --full-history`, `git grep`, `rg`, PowerShell JSON5 parsing and invariant scripts, SHA-1 hashing of all nine source archives, exact metadata-to-OGG filename comparison, and field-by-field comparison of every active generated album record with its source metadata track
- No-finding areas: every assigned file parses; all nine source archive hashes match `source_sha1`; every metadata filename has exactly one extracted OGG and every source path and output filename is unique within its album; all 160 records have provenance, a syntactically valid nonempty chromaprint, and a positive duration; every active database record preserves the source fingerprint, duration, generated name, and optional naming fields; lower database counts for four albums are explained by the merger's intentional CD-fingerprint exclusions; repeated fingerprints within multi-version archives retain distinct source paths, and no concrete incorrect match identity or displayed-name behavior justified a finding

### R1-CHUNK-0007 completion

- Completed: 2026-07-19
- Model: sol-5.6-medium
- Result: BR-0013, BR-0014
- Assigned scope checked: all 260 assigned lines in `android/app/src/main/cpp/extract/cue_parser.c` and `cue_parser.h`
- Context checked: frozen additions and complete file histories; D1 and D2 Android build inclusion; JNI parsing and Kotlin import flows; command-line extraction and fingerprinting; CD preview and runtime Redbook playback; ISO and HFS consumers; CUE filename ordering; parser fixtures; and all CUE parser edge-case tests
- Commands or validation: `git diff`, `git show`, `git log --follow --full-history`, `git grep`, `rg`, line-numbered source-to-consumer traces, boundary analysis against the declared structure layouts, and `android/tests/test_cue_iso.ps1`, whose seven native suites all passed
- No-finding areas: null input and output handling, result initialization, bounded filename and title copies, ordinary single-file and multi-file sector accounting, CRLF and case-insensitive directives, D1/D2 use of the same parser source, caller cleanup on normal parse failures, and existing extraction and fingerprint fixtures were checked without another actionable root cause; malformed MSF and missing-index acceptance can produce poor diagnostics, but the reviewed callers generally fail later and the evidence did not justify a separate finding from the admitted capacity defects

### R1-CHUNK-0008 completion

- Completed: 2026-07-19
- Model: sol-5.6-medium
- Result: BR-0015, BR-0016
- Assigned scope checked: all 570 assigned lines in `android/app/src/main/cpp/jni_cd_preview.c` and `jni_disc_import.c`
- Context checked: frozen additions and complete histories; D1 and D2 CMake source lists; the superseding `extract/jni_disc_import.c`; JNI symbol sets and Kotlin declarations; CD preview path and descriptor entry points; native preview global state, BIN ownership, render thread, and OpenSL lifecycle; Compose playback controls; setup command receiver threading; and the redbook integration script
- Commands or validation: `git diff`, `git show`, `git log --follow --full-history`, `git grep`, `rg`, line-numbered JNI-to-Kotlin and JNI-to-native traces, frozen-blob comparison of both disc-import implementations, and mechanical comparison of their exported JNI symbol sets and build inclusion
- No-finding areas: path and descriptor start variants release JNI strings and arrays on ordinary success and failure paths; multi-path local references are deleted; descriptor preview duplicates each fd before Kotlin closes its `ParcelFileDescriptor`; preview state serialization is bounded; progress callbacks in the disc-import bridge are synchronous in the reviewed extractors; and no additional actionable JNI ownership or array-bound issue was admitted beyond the stale implementation and preview lifecycle race

### R1-CHUNK-0009 completion

- Completed: 2026-07-19
- Model: sol-5.6-medium
- Result: BR-0017, BR-0018
- Assigned scope checked: all 470 assigned lines in `android/app/src/main/cpp/extract/stuffit_extract.c` and `stuffit_extract.h`
- Context checked: frozen additions and complete file histories; D1 and D2 build inclusion; the built disc-import JNI bridge and Kotlin declaration; StuffIt staging, storage preflight, and result collection; SIT5 header and entry parsing; nested STi handoff; `sti2_extract_entry` allocation and output behavior; extension filtering, path flattening, temporary-file cleanup, and progress callbacks; corpus, direct, and demo-oracle tests
- Commands or validation: `git diff`, `git show`, `git log --follow --full-history`, `git grep`, `rg`, line-numbered archive-to-parser-to-decompressor-to-caller traces, malformed-offset boundary analysis, duplicate-finding search, and `android/tests/test_cue_iso.ps1`, whose seven native suites including StuffIt corpus and demo-oracle tests all passed
- No-finding areas: archive type and version detection, ordinary entry data-span checks, encryption rejection, separator sanitization and basename-only final outputs, directory ancestry construction for valid archives, extension filtering, ordinary cleanup, D1/D2 use of the same built source, and known-corpus extraction were checked without another distinct actionable root cause; basename collisions and silent path truncation remain worth broader archive-policy review, but the current evidence did not establish behavior distinct enough from the admitted trust-boundary findings

### R1-CHUNK-0010 completion

- Completed: 2026-07-19
- Model: sol-5.6-medium
- Result: BR-0018, BR-0019, BR-0020, BR-0021
- Assigned scope checked: all 586 assigned lines in `android/app/src/main/cpp/extract/jni_gog_import.c` and `mac_hfs_extract.c`
- Context checked: frozen additions and complete file histories; D1 and D2 CMake inclusion; GOG Kotlin declarations and import dialog; InnoSetup and PKG reader progress, extraction, and cleanup contracts; HFS catalog name decoding and path construction; disc-import JNI and Kotlin callers; nested STi extraction; game-file hoisting; real-media HFS, STi2, GOG-fd, and extraction tests
- Commands or validation: `git diff`, `git show`, `git log --follow --full-history`, `git grep`, `rg`, line-numbered JNI-to-reader-to-Kotlin and HFS-catalog-to-output traces, malformed catalog component analysis, duplicate-finding search, and `android/tests/test_cue_iso.ps1`, whose seven native suites all passed
- No-finding areas: JNI path and output strings are released on ordinary paths; fd-based Inno opening duplicates caller-owned descriptors; archive handles close after listing and extraction; file extension filtering and basename-only GOG outputs prevent separator traversal; HFS catalog separators are replaced; normal HFS and nested STi cleanup and callback propagation were checked; both Android game targets compile the same sources; known GOG and Mac media fixtures pass, but they do not exercise cancellation, partial failure, dot components, or extraction budgets

### R1-CHUNK-0011 completion

- Completed: 2026-07-19
- Model: sol-5.6-medium
- Result: BR-0018, BR-0020, BR-0021, BR-0022, BR-0023, BR-0024, BR-0025
- Assigned scope checked: `android/app/src/main/cpp/extract/sow_extract.c` L601-L768 and all 74 lines of `sow_extract.h`
- Context checked: frozen additions and complete file histories; the full ARJ entry parser, bit reader, Huffman table builder, and decompressor; D1 and D2 build inclusion; retained and stale JNI bridges; Kotlin direct, nested-ZIP, and disc post-processing callers; append-mode ordering and stale-target cleanup; CMake test registration; direct SOW smoke executable; and locally available retail and split SOW fixtures
- Commands or validation: `git diff`, `git show`, `git log --follow --full-history`, `git grep`, `rg`, line-numbered compressed-input-to-allocation-to-output and callback-to-caller traces, duplicate-finding search, `android/tests/test_cue_iso.ps1` with all seven registered native suites passing, and manual `test_sow_direct` extraction of `game_data/DESCENT2.SOW`, which produced 34 files totaling 52,935,956 bytes
- No-finding areas: basename flattening blocks separator-based archive traversal; output path construction is bounded; known split archives are sorted before append and stale matching targets are cleared in the nested-ZIP flow; valid known SOW data decompresses successfully; ordinary source and output handles close; both Android game targets compile the same implementation; unsupported methods are detected, but their failure status is covered by BR-0020; extended-header seeks and host `long` limits merit malformed-corpus coverage but did not establish another distinct root cause in this range

### R1-CHUNK-0012 completion

- Completed: 2026-07-20
- Model: sol-5.6-medium
- Result: BR-0026, BR-0027
- Assigned scope checked: `android/app/src/main/cpp/extract/hfs_reader.c` L601-L726 and all 76 lines of `hfs_reader.h`
- Context checked: frozen additions and complete file histories; the full volume loader, extent reader, catalog node scanner, path builder, list and extraction APIs; Mac HFS and nested STi callers; JNI and Kotlin `Dispatchers.IO` import path; D1 and D2 build inclusion; synthetic partition coverage; real-disc catalog and extraction tests; and prior HFS path-traversal and extraction-budget findings
- Commands or validation: `git diff`, `git show`, `git log --follow --full-history`, `git grep`, `rg`, line-numbered malformed-node-to-catalog-to-output traces, duplicate-finding search, NDK r27d AArch64 Clang `-O2 -Wframe-larger-than=65536` compilation, and `android/tests/test_cue_iso.ps1`, whose seven registered native suites all passed
- No-finding areas: track reads reject negative and beyond-track byte ranges; file sizes are checked against the three inline extents before extraction; ordinary read and write failures propagate; output descriptors close on success and failure; zero-length files complete without a read; path separators are replaced, while dot components remain covered by BR-0019; expanded-size and storage budgets remain covered by BR-0018; the passing HFS tests cover valid known media but do not exercise malformed catalog record tables or payload boundaries

### R1-CHUNK-0013 completion

- Completed: 2026-07-20
- Model: sol-5.6-medium
- Result: BR-0016, BR-0028, BR-0029, BR-0030
- Assigned scope checked: `android/app/src/main/cpp/jni_main.c` L1201-L1410 and all 124 lines of `jni_midi_preview.c`
- Context checked: frozen additions and complete file histories; MainActivity engine-thread startup and main-looper overlay providers; video diagnostics polling; coop warp traversal, mutation, packet send, and overlay callbacks; escort-owner reads; host-migration callback and receiver behavior; MIDI JNI ownership; HOG reads and enumeration; Compose playback lifecycle and seek controls; native synth, render-thread, ring-buffer, and OpenSL lifecycle; D1 and D2 build wiring; and MIDI automation scripts
- Commands or validation: `git diff`, `git show`, `git log --follow --full-history`, `git grep`, `rg`, line-numbered UI-to-JNI-to-game-state and UI-to-JNI-to-render-thread traces, duplicate-finding search, and review of the MIDI automation timing and assertions; no runtime race, failure-injection, seek, or coop-warp test exists in the frozen suite
- No-finding areas: assigned JNI byte-array data is copied or converted before release; successful HOG reads transfer ownership to Java and free the native buffer; enumeration JSON is freed after `NewStringUTF`; state strings fit their fixed buffer; host migration attaches and detaches native-origin threads and its Java callback performs a thread-safe broadcast; escort indices are range-checked before callsign access in stable state, while unsynchronized transitions are covered by BR-0029; malformed MIDI and HOG allocation behavior remains in the later full implementation chunks

### R1-CHUNK-0014 completion

- Completed: 2026-07-20
- Model: sol-5.6-medium
- Result: BR-0018, BR-0031, BR-0032, BR-0033
- Assigned scope checked: `android/app/src/main/cpp/extract/sti2_extract.c` L1801-L2117 and all 61 lines of `sti2_extract.h`
- Context checked: frozen additions and complete file history; archive signature and header scanning; path construction and name sanitization; methods 13, 14, and 15 bit readers and decoder completion; entry allocation and output writes; nested StuffIt and HFS callers; JNI progress propagation; D1 and D2 build inclusion; STi2 real-media tests and StuffIt corpus hash tests; and the referenced XAD StuffIt parser and Arsenic checksum behavior
- Commands or validation: `git diff`, `git show`, `git log --follow --full-history`, `git grep`, `rg`, line-numbered header-to-entry-to-decoder-to-output traces, duplicate-finding search, comparison with the referenced upstream XADMaster implementation, and `android/tests/test_cue_iso.ps1`, whose seven registered native suites including STi2, StuffIt corpus, and demo-oracle tests all passed
- No-finding areas: archive magic reads are size-guarded; header CRCs are calculated before header fields are accepted; entry compressed spans are checked subtractively before access; supported valid corpus outputs match exact hashes; separator replacement and basename flattening prevent ordinary slash traversal; write and callback failures propagate and descriptors and heap buffers are released on ordinary error paths; cancellation stops before the next file, while broader cancellation semantics remain covered by BR-0021; output allocation and aggregate limits remain covered by BR-0018; basename collision policy and direct-write rollback remain broader archive concerns without a distinct new root cause in this range

### R1-CHUNK-0015 completion

- Completed: 2026-07-20
- Model: sol-5.6-medium
- Result: BR-0018, BR-0020, BR-0021, BR-0034, BR-0035, BR-0036, BR-0037
- Assigned scope checked: `android/app/src/main/cpp/extract/inno_reader.c` L1801-L2202 and all 148 lines of `inno_reader.h`
- Context checked: frozen additions and complete file history; archive version detection, file and data-entry parsing, stored, zlib, LZMA1, and LZMA2 buffered and streaming decoders; GOG Galaxy inner-zlib filtering; checksum metadata; file-range and output-size validation; progress callbacks and cleanup; JNI and Kotlin import callers; D1 and D2 build inclusion; native GOG-fd coverage and Android GOG automation; and the referenced upstream innoextract versioned data layout and checksum-filter behavior
- Commands or validation: `git diff`, `git show`, `git log --follow --full-history`, `git grep`, `rg`, line-numbered parser-to-decoder-to-output and callback-to-caller traces, checked-arithmetic and malformed-range analysis, duplicate-finding search, comparison with the referenced upstream innoextract implementation, and `android/tests/test_cue_iso.ps1`, whose seven registered native suites all passed
- No-finding areas: archive and output descriptors close on ordinary success and failure; streaming regular and Galaxy paths check writes and close errors and remove incomplete output; the buffered path checks short writes; invalid file indices, missing data locations, and unsupported instruction filters fail before output; compressed streaming decoders reject decoder errors and no-progress states; the valid GOG-fd fixture lists expected D1 and D2 entries, but it does not extract payloads or exercise malformed ranges, checksums, cancellation, resource limits, legacy 5.3 layouts, or injected output failures

### R1-CHUNK-0016 completion

- Completed: 2026-07-20
- Model: sol-5.6-medium
- Result: BR-0038, BR-0039, BR-0040, BR-0041
- Assigned scope checked: all 356 lines of `android/app/src/main/cpp/extract/fingerprint_match.c`, all 67 lines of `game_file_extensions.c`, and all 27 lines of `game_file_extensions.h`
- Context checked: frozen additions and complete file histories; Chromaprint decoding and runtime database matching; the canonical fingerprint configuration; the album-to-disc merger and its generated flat input; all frozen track and album fingerprint metadata; current database scale and encoded-fingerprint lengths; GOG, PKG, ISO, HFS, StuffIt, and JNI extension consumers; Kotlin `GameFileFormats` and its compatibility wrapper; CMake build wiring; and native and Kotlin test registration
- Commands or validation: `git diff`, `git show`, `git log --follow --full-history`, `git grep`, `rg`, line-numbered matcher-to-merger and extension-table-to-extractor traces, duplicate-finding search, mechanical extension-set and fingerprint-size comparisons, execution of `fingerprint_match.exe` against the locally available 761-entry merged input at thresholds 0.40 and 0.65, and `android/tests/test_cue_iso.ps1`, whose seven registered native suites including `fingerprint_tests` all passed; a direct `ctest` command was unavailable on `PATH`, but the wrapper resolved and ran the repository CTest binary successfully
- No-finding areas: XOR-popcount arithmetic, offset bounds, minimum overlap, successful Chromaprint allocation ownership, final entry cleanup, pair enumeration, JSON delimiter escaping for ordinary generated identifiers, case-insensitive native extension comparison, leading-dot conventions at each caller, null-terminated extension tables, Windows and POSIX comparator selection, and current shared game-import and generic disc-extract extension values were checked without another actionable issue; the current merged fingerprint input remains below both hard capacities, and existing fingerprint tests validate generation but do not invoke the duplicate matcher or its parser, thresholds, limits, or duration boundaries

### R1-CHUNK-0017 completion

- Completed: 2026-07-20
- Model: sol-5.6-medium
- Result: BR-0020, BR-0024, BR-0042, BR-0043
- Assigned scope checked: `android/app/src/main/cpp/extract/extract_cd.c` L601-L647, all 205 lines of `extract_gog.c`, and all 188 lines of `fingerprint_audio.c`
- Context checked: frozen additions and complete file histories; full CD extraction control flow and recursive SOW scanning; Inno and PKG listing and extraction contracts; GOG batch extraction and hashing; shared game-extension policy; PCM decoding and Chromaprint ownership; music-pack and mission-archive fingerprint generators; fpcalc comparison coverage; Windows and POSIX directory enumeration; output JSON consumers; and CMake and CTest registration
- Commands or validation: `git diff`, `git show`, `git log --follow --full-history`, `git grep`, `rg`, line-numbered CLI-to-reader-to-script and directory-to-fingerprint-to-JSON traces, duplicate-finding search, direct execution of `fingerprint_audio.exe` against a nonexistent directory confirming exit 0 with `[]`, and `android/tests/test_cue_iso.ps1`, whose seven registered native suites all passed
- No-finding areas: format selection, ordinary archive closure, extension filtering, Inno per-file error counting in the standalone CLI, fingerprint allocation cleanup, case-insensitive audio suffix checks, bounded path joining, deterministic filename sorting, comma placement for successful ordinary JSON records, and nonzero exit after a reported decode or path-length failure were checked without another distinct issue; current music directories are far below 4,096 entries and use ordinary names, real GOG automation exercises the production JNI path rather than this standalone CLI, and the passing fingerprint suite tests decoding and generation but not directory-open failure, enumeration capacity, or filename serialization

### R1-CHUNK-0018 completion

- Completed: 2026-07-20
- Model: sol-5.6-medium
- Result: BR-0044, BR-0045
- Assigned scope checked: `android/app/src/main/cpp/jni_resume_save.cpp` L601-L952, all 92 lines of `jni_saf.c`, all 61 lines of `shared/android_jni_overlay.c`, and all 18 lines of `shared/android_jni_overlay.h`
- Context checked: frozen additions and complete file histories; recursive save discovery, metadata and legacy-header parsing, explorer JSON, deletion authorization, and thumbnail reads; Kotlin Save Explorer conversion, deletion, and launch callers and unit tests; SAF manifest parsing, PhysFS open flow, Kotlin ContentResolver bridge, manifest ownership, and adb direct-path test mode; overlay track, level, and rewind producers; MainActivity callback methods and UI-thread handoff; and the long-running `startGame` JNI frame and activity global-reference lifecycle
- Commands or validation: `git diff`, `git show`, `git log --follow --full-history`, `git grep`, `rg`, line-numbered save-file-to-explorer-to-launch, manifest-to-PhysFS-to-JNI, and engine-to-JNI-to-UI traces, duplicate-finding search, and `android/tests/test_cue_iso.ps1`, whose seven registered native suites all passed
- No-finding areas: assigned resume-save JNI strings are released on ordinary paths; metadata-backed candidates retain their validator; deletion checks root prefixes, slot identity, and a timestamp before removal, and normal Kotlin callers pass a native-enumerated slot; thumbnail arrays validate metadata dimensions before allocation and copy; the SAF bridge releases local references, clears callback exceptions, and detaches native-origin threads; ordinary SAF entries are non-null after manifest parsing; the production-compiled absolute-path SAF branch is documented and exercised only by an adb-injected private manifest in the located test, so no normal-app attacker path was established; overlay activity ownership spans the native game call, and Kotlin marshals presentation work to the UI thread; the passing native suites do not exercise Save Explorer legacy validation, JNI local-reference pressure, attach failure, or Java callback exceptions

### R1-CHUNK-0019 completion

- Completed: 2026-07-20
- Model: sol-5.6-medium
- Result: BR-0013, BR-0014, BR-0020, BR-0046, BR-0047
- Assigned scope checked: `android/app/src/main/cpp/extract/extract_cd.c` L1-L600
- Context checked: frozen addition and complete file history; duplicated SHA-1 and file helpers; CUE parsing and its file and track capacities; raw-track hashing; standalone ISO and BIN/CUE listing and extraction; HFS fallback; output status and JSON contracts; SOW post-processing context; ISO reader return semantics and failure accounting; `extract_all_cds.ps1`, track-hash promotion, regression-spec generation, three committed Mac track manifests, native CMake and CTest registration, and the parallel `fingerprint_cd` implementation
- Commands or validation: `git diff`, `git show`, `git log --follow --full-history`, `git grep`, `rg`, line-numbered CUE-to-BIN-to-hash and ISO-or-HFS-to-output traces, duplicate-finding search, inspection of all committed HFS error records and their consumers, and `android/tests/test_cue_iso.ps1`, whose seven registered native suites all passed
- No-finding areas: standard SHA-1 rounds, ordinary full-file ISO hashing, successful descriptor closure, known-good single- and multi-file CUE iteration, per-track file-index use, static track JSON escaping, standalone ISO status lines, ordinary HFS descriptor reuse, and data and audio type selection were checked without another distinct issue; parser capacity defects remain BR-0013 and BR-0014, SOW partial success remains BR-0020, SOW filename serialization remains BR-0042, and the passing suite exercises the shared readers but does not invoke the `extract_cd` CLI, simulate source mutation or short reads, inject output failures, or validate HFS CLI JSON semantics

## Findings

Append findings here in numeric order using the exact template in the process document

### BR-0001: P1 - Remove runtime logs, database state, and scratch output before publication

- [ ] OPEN
- Type: defect
- Confidence: high
- Category: pr-hygiene/privacy
- Found by: R1-PREFLIGHT-001
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/test_output.txt:L1` and `.gitignore:L25-L31`
- Related: `android/build_output.txt:L1`, `game_data_to_copy_to_emulator/debuglog_20260520_211753.txt:L1`, `dxx_matchmaking.db`, `dxx_matchmaking.db-shm`, `dxx_matchmaking.db-wal`, `.tmp`, `test.txt:L1`, and `d1_d2_ogl_diff.txt:L1`
- Evidence: All listed paths are tracked and are not ignored. `android/test_output.txt` contains the local username and home path, Gradle daemon UUIDs and PIDs, local SDK paths, and build-machine details. The texture debug log is 18,644,764 bytes and 54,701 lines. The SQLite WAL is 222,512 bytes and contains schemas for player IDs, callsigns, friends, match history, connection events, admin logs, bans, and keypair identities
- Trigger: Publish this branch or open it directly as a pull request against the public upstream repository
- Impact: Local identity and machine details become public, mutable server state may disclose test or real player data, and large non-reproducible outputs permanently bloat review and repository history
- Expected: Version control contains source, intentional sanitized fixtures, and reproducible metadata only; runtime state and command output remain under ignored scratch directories
- Suggested fix: Remove the tracked outputs and SQLite files from the upstream series, add ignore rules for the actual output and database extensions, route future captures under `temp/`, and replace any indispensable diagnostic fixture with a minimal sanitized deterministic sample. If the database or prior logs ever held real credentials or player data, purge the affected history and rotate credentials before publication
- Validation: Confirm the paths are absent from the proposed PR, `git check-ignore -v` covers their future locations or names, a clean build and test run does not dirty the worktree, and a full-history secret and privacy scan reports no remaining sensitive blobs
- Resolution: Pending

### BR-0002: P1 - Build an allowlisted upstream patch series instead of opening this branch directly

- [ ] OPEN
- Type: design
- Confidence: high
- Category: pr-hygiene
- Found by: R1-PREFLIGHT-001
- Location: `fb555eec75e1ed12c8348805ab335afb4c721b06..c01d8fe4686c63d931b1e543a6305bbafaa944a9`
- Related: `android/ai tool plans/`, `game_data/mission_files/`, `android/test_fixtures/`, `android/tests/fixtures/`, `game_data_to_copy_to_emulator/`, `server/`, `d1/`, and `d2/`
- Evidence: The proposed range contains 1,076 commits, 2,638 paths, and +672,291/-4,047 lines. Historical plans, generated fixtures, artifacts, dependency locks, and miscellaneous data account for 1,286 paths and 366,348 additions, or 48.7 percent of paths and 54.5 percent of added lines. Historical AI plans alone contribute 1,122 files and 88,521 lines, while generated mission metadata contributes 117 files and 180,314 lines. Commit subjects include repeated generic labels such as `test fixes`, `more`, `cleanup`, and merge commits, so history does not provide a reviewable feature sequence
- Trigger: Use `cmake` directly as the head of one upstream pull request
- Impact: Reviewers cannot isolate product behavior from local process history and generated corpora, important native and protocol changes are hidden in noise, and the resulting change is difficult to test, revert, bisect, or merge incrementally
- Expected: Upstream receives a deliberate series whose commits and files each support a named Android-port capability, with only required generated fixtures and minimal paired D1/D2 edits
- Suggested fix: Create a clean integration branch from the current upstream target, define a path and feature allowlist, and transplant reviewed changes as coherent commits or PRs. Separate at least build foundations, Android launcher and native bridge, minimal D1/D2 hooks, matchmaking server, and optional test-data or metadata corpora. Keep AI plans, local logs, databases, and superseded studies out of the upstream series
- Validation: Produce a path-level inclusion manifest mapping every retained file to a feature or required test, confirm excluded paths are absent, run the full host and Android build and test matrix on the clean series, and verify each commit can be reviewed and reverted without unrelated behavior
- Resolution: Pending

### BR-0003: P2 - State the standalone matchmaking server license explicitly

- [ ] OPEN
- Type: documentation
- Confidence: high
- Category: documentation/license
- Found by: R1-PREFLIGHT-001
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:server/Cargo.toml:L1`
- Related: `server/` and repository-root `COPYING.txt`
- Evidence: The branch adds a complete Rust server but no `server/LICENSE`, `server/README.md`, copyright header, SPDX marker, or Cargo `license` or `license-file` field. The only visible repository license is the D1X-Rebirth license, which describes game-derived code and restricts revenue-bearing use, while repository instructions describe the server as separately publishable open-source code
- Trigger: Publish, package, deploy, or later extract `server/` independently from the game tree
- Impact: Contributors and downstream operators cannot determine whether the server is intentionally covered by the game-specific root license or another license, which blocks reliable reuse and distribution decisions
- Expected: The server has an explicit maintainer-approved license and provenance statement that remains valid when the directory is packaged or extracted
- Suggested fix: Decide the server license with the maintainers, add the corresponding license or notice in `server/`, set Cargo package metadata such as `license` or `license-file`, document its relationship to the root game license, and record any third-party source attribution requirements
- Validation: Inspect `cargo metadata` or `cargo package --list` output to confirm the license metadata and notice ship with the server, and have the upstream maintainer approve the stated terms
- Resolution: Pending

### INV-0001: Run a dedicated full-history secret scan before making the branch public

- [ ] OPEN
- Risk if confirmed: P0
- Found by: R1-PREFLIGHT-001
- Location: `fb555eec75e1ed12c8348805ab335afb4c721b06..c01d8fe4686c63d931b1e543a6305bbafaa944a9`
- Evidence known: The range has 1,076 commits and includes Play Store service-account parsing, OAuth configuration, signing workflows, server client-secret support, debug logs, and local runtime state. Targeted `git grep` and `git log -G` scans found only placeholder private-key markers and credential-processing code, not a real credential. No dedicated secret scanner is installed in the current environment
- Evidence missing: Targeted regular expressions do not cover entropy-based findings, deleted or renamed secret formats, credentials embedded in binary blobs, or provider-specific tokens outside the selected patterns
- Next check: Run a maintained secret scanner across every commit and blob in the proposed upstream range, manually classify findings without printing secret values, remove sensitive history before publication, and rotate every credential that may have been exposed
- Resolution: Pending

### BR-0004: P1 - Bridge runtime game state out of the separate game process

- [ ] OPEN
- Type: defect
- Confidence: high
- Category: correctness
- Found by: R1-PREFLIGHT-002
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/app/src/main/java/com/dxxredux/app/multiplayer/MatchmakingService.kt:L453` in `startGameStateUpdates` and `pollAndSendGameState`
- Related: `android/app/src/main/AndroidManifest.xml:L52`, `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt:L1717`, `android/app/src/main/java/com/dxxredux/app/MainActivity.kt:L702`, `server/src/ws_handler.rs:L1379`, `server/src/lobby.rs:L165`, and `server/src/stats.rs:L125`
- Evidence: `MainActivity` runs in `:game`, while the WebSocket-owning `MatchmakingService` singleton and its `activityRef` remain in the default process with `SetupActivity`. The host starts the update job at `MatchmakingService.kt:L1109`, but every poll returns at L478-L480 because that process's activity is not `MainActivity`. The separate game-process singleton receives `MainActivity` at L702 but has no WebSocket and never starts the job. On the server, only `UPDATE_GAME_STATE` transitions `Starting` to `InGame`, populates the current level, and refreshes `last_state_update`; absent an update, the lobby is not late-joinable and stale cleanup removes it five minutes after creation. The same process split also makes the in-game proxy and connection-info providers at `MainActivity.kt:L1437-L1443` read empty game-process singletons
- Trigger: Start an online matchmaking game as host and remain in the native game process for the first three-second state poll and subsequent 30-second polls
- Impact: The lobby remains `Starting`, mid-game joining never becomes available, live level and player counts are never published, in-game network diagnostics omit proxy and connection data, and the server eventually expires the active lobby with `LOBBY_EXPIRED`
- Expected: The default-process matchmaking owner receives authoritative runtime state from `:game` and keeps the server lobby and diagnostic snapshot current for the lifetime of the multiplayer game
- Suggested fix: Define one explicit same-UID IPC boundary. Prefer a bound service or narrowly scoped non-exported messages that let `:game` publish native state and request snapshots from the default-process matchmaking and proxy owner. Keep the WebSocket and UDP proxy under one owner, remove process-local reads that pretend the singleton is shared, and make service restart or process death produce an explicit disconnected state
- Validation: Run an instrumented two-process host session for longer than five minutes and assert that the server observes `Starting -> InGame`, fresh updates at the configured interval, correct current level and player count, a late join succeeds, proxy diagnostics are non-empty when proxying, and stopping either process cleans up without a stale lobby
- Resolution: Pending

### BR-0005: P1 - Keep launcher command broadcasts private in release builds

- [ ] OPEN
- Type: defect
- Confidence: high
- Category: security
- Found by: R1-PREFLIGHT-002
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/app/src/main/java/com/dxxredux/app/SetupActivity.kt:L1728` in the runtime receiver registrations
- Related: `SetupActivity.kt:L453-L951`, `SetupActivity.kt:L978-L1214`, `MainActivity.kt:L3691-L3720`, and `android/app/src/main/AndroidManifest.xml:L1-L112`
- Evidence: `SETUP_INTROSPECT`, `SETUP_COMMAND`, `MP_COMMAND`, and `HOST_MIGRATION` are registered whenever the launcher is created. On Android 13 and later each is explicitly `RECEIVER_EXPORTED`; on earlier versions the registration supplies no sender permission. Only `SETUP_AUTOMATE` is registration-guarded by `BuildConfig.DEBUG`, and the manifest declares no app-specific permission. The exported `SETUP_COMMAND` surface includes unauthenticated operations that delete save and pilot files, clear file sets and audio sources, patch pilot preferences, import caller-selected paths, and launch the game. `MP_COMMAND` can connect or disconnect matchmaking, send chat, create or join lobbies, and launch multiplayer
- Trigger: While `SetupActivity` is alive in a release build, another installed app sends an explicit or package-targeted broadcast with one of the public action strings and chosen extras
- Impact: An untrusted local app can modify or delete private game data, alter configuration, initiate network and lobby actions under the user's game identity, and force UI or game transitions without user consent
- Expected: Production cross-process commands accept broadcasts only from this app's UID; ADB automation and introspection exposure exists only in debug builds with an explicit test boundary
- Suggested fix: Register production same-app actions such as host migration with `RECEIVER_NOT_EXPORTED`, move ADB-only command, multiplayer-control, and introspection receivers behind `BuildConfig.DEBUG`, and use debug-only exported registration only where the shell test harness requires it. If any production external integration is intentional, protect a narrowly scoped receiver with a signature permission and validate every command separately
- Validation: From a separately signed test app, attempt every release action and assert no receiver invocation, file mutation, activity launch, or network change; confirm same-app `:game` to launcher notifications still work; then verify the documented ADB commands remain available only in a debug APK
- Resolution: Pending

### BR-0006: P2 - Refresh operational documentation after tool and source migrations

- [ ] OPEN
- Type: documentation
- Confidence: high
- Category: documentation/maintainability
- Found by: R1-PREFLIGHT-003
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:.github/copilot-instructions.md:L114` and `.github/copilot-instructions.md:L222-L225`
- Related: `android/Run-Emulator.ps1:L1`, `android/app/src/main/cpp/shared/game_introspect.cpp:L1-L22`, `android/app/src/main/assets/DISC_HASHING.md:L4`, and `android/app/src/main/java/com/dxxredux/app/AudioSourceManager.kt:L134`
- Evidence: The mandatory instructions tell bash users to run `android/run_emulator.sh`, but commit `1cae3952` deleted that script and the frozen head contains only the PowerShell launcher. They also direct introspection changes to `d2/introspect/game_introspect.c` using `jb_*` helpers. Commit `a9683796` replaced that C file and hand-written serializer with a C++ nlohmann/json implementation, and commit `68935e7c` moved it into the Android shared source used by both D1 and D2. Neither old source path exists at the frozen head. The disc hashing guide and an audio-source comment likewise retain the deleted `known_discs.json` name after the database moved to `known_discs.json5`
- Trigger: Follow the repository-mandated setup guidance on a bash host, or use its extension instructions to add an introspection field or update disc-identification behavior
- Impact: The emulator command fails immediately, a contributor or AI agent searches for or recreates a superseded D2-only implementation, serializer guidance describes an API that no longer exists, and disc-database edits can target the wrong filename
- Expected: Operational instructions name current, supported entry points and describe the shared C++ serialization design and current JSON5 database consistently
- Suggested fix: Remove the unsupported bash command or add and test a maintained bash wrapper; point introspection work to `android/app/src/main/cpp/shared/game_introspect.cpp`, describe nlohmann/json assignments and the shared D1/D2 build, and replace remaining `known_discs.json` references with `known_discs.json5`. Add a lightweight documentation check that validates repository-relative paths in mandatory operational instructions
- Validation: Resolve every command and repository-relative path in `.github/copilot-instructions.md`, execute the emulator startup path on each claimed host platform, add a harmless debug introspection field in a test branch and confirm both D1 and D2 debug builds expose it, and verify all disc-database documentation names the tracked JSON5 asset
- Resolution: Pending

### BR-0007: P2 - Make the Play Store credential sample match the supported key format

- [ ] OPEN
- Type: defect
- Confidence: high
- Category: correctness/deployment
- Found by: R1-CHUNK-0001
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/play-store-credentials.sample.json:L11`
- Related: `android/helpers/playstore-auth.ps1:L34-L70` and `android/helpers/playstore-auth.ps1:L94-L110`
- Evidence: The sample wraps `private_key` in `BEGIN RSA PRIVATE KEY`, which denotes a PKCS#1 key. The signer removes either PEM label but always calls `ImportPkcs8PrivateKey`; its Windows PowerShell fallback also parses only a PKCS#8 `PrivateKeyInfo`. An in-memory test using the helper with one generated RSA key showed the PKCS#1 encoding fail the modern importer with `ASN1 corrupted data` and fail the fallback by reading past the stream, while the PKCS#8 encoding passed both import paths
- Trigger: Create `play-store-credentials.json` from the checked-in sample and supply a valid PKCS#1 RSA private key as its advertised envelope requests
- Impact: Both Play Store upload entry points fail during JWT signing before they can authenticate or upload a release, despite the credential matching the documented sample format
- Expected: Every key format advertised by the credential sample is accepted by the shared signer on supported PowerShell versions
- Suggested fix: Prefer changing the sample envelope to `BEGIN PRIVATE KEY` and directing users to preserve the PKCS#8 `private_key` value from the downloaded service-account JSON. If PKCS#1 is intentionally supported, detect its envelope before stripping it, call `ImportRSAPrivateKey` on modern runtimes, and add a correct PKCS#1 fallback for Windows PowerShell
- Validation: Add an offline authentication-helper test that generates one RSA key, imports its documented encoding through the same branch used by `Get-PlayStoreAccessToken`, signs a JWT payload, and verifies the signature on PowerShell 7 and Windows PowerShell 5.1; also confirm a minimally redacted downloaded service-account JSON follows the documented setup path
- Resolution: Pending

### BR-0008: P2 - Enforce recorded source hashes before extraction regression tests

- [ ] OPEN
- Type: defect
- Confidence: high
- Category: test-gap/data-integrity
- Found by: R1-CHUNK-0002
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:game_data/CD images/d1 mac 2nd bin+cue/extract_regression.json5:L7-L15`
- Related: the other assigned `extract_regression.json5` files, `game_data/generate_regression_specs.ps1:L234-L249`, `android/tests/validate_extract_regression_specs.ps1:L44-L113`, and `android/tests/test_extract.ps1:L694-L699`
- Evidence: The generator records a SHA-256 for every CUE, BIN, and ISO source, but the validator checks only file existence, CUE references, and import mode. The test runner reads source names and stages those files without reading or comparing `sha256`; the only test-side uses preserve the field while rewriting JSON. A direct review-time hash pass found all 23 assigned files currently match, demonstrating that the oracle data is usable but not enforced
- Trigger: A source image is corrupted, replaced by another release, or incompletely copied while retaining the filename listed in its regression spec
- Impact: The suite runs a named-disc oracle against an unidentified input, so it can produce misleading extraction failures or pass against a different image while the recorded provenance claim remains unchanged
- Expected: The source identity recorded in each spec is verified before extraction or device staging, and a mismatch fails with the affected filename and expected and actual digest
- Suggested fix: Add a shared source-file integrity check to the regression-spec validator and invoke the same check from `test_extract.ps1` before staging. Validate lowercase 64-hex digest syntax, stream each file once, and fail before emulator work on any mismatch
- Validation: Add a fast fixture with a recorded digest, prove the validator and single-spec runner accept it unchanged, alter one byte without renaming it, and assert both reject it before import; retain a controlled full-corpus hash validation for locally available disc images
- Resolution: Pending

### BR-0009: P2 - Fail extraction regressions when the loaded level is wrong

- [ ] OPEN
- Type: defect
- Confidence: high
- Category: test-gap/false-pass
- Found by: R1-CHUNK-0002
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/tests/test_extract.ps1:L1430-L1437`
- Related: assigned `extract_regression.json5` fields `expected_mission` and `expected_level1`, plus `android/tests/test_extract.ps1:L1481-L1488`
- Evidence: After automation reports `in_game`, the runner compares `current_level_name` with `expected_level1`. The matching branch prints PASS, but the mismatch branch also prints `PASS (level mismatch)` and execution continues to the unconditional successful `Exit-Test` at the end. That call writes a `pass` result even though the principal runtime oracle was false
- Trigger: A mission-selection, level-routing, metadata, or automation regression launches any in-game level other than the spec's expected first level
- Impact: The extraction regression suite reports success and records a passing result for the wrong level, allowing the launcher or game-routing regression to merge undetected and weakening every assigned full-test oracle
- Expected: A full regression passes only when the game reaches the exact expected mission and level; an intentional rename or oracle correction is reviewed as a spec change rather than accepted dynamically
- Suggested fix: Treat `levelMatch == false` as a failed assertion with the actual and expected names in diagnostics. If aliases are required, represent an explicit reviewed set in the spec and compare against it rather than accepting every in-game level
- Validation: Unit-test the result decision with matching and mismatching introspection payloads, then run one emulator spec normally and a temporary copy with an intentionally wrong `expected_level1`; require the latter to exit nonzero and persist `status: fail`
- Resolution: Pending

### BR-0010: P2 - Separate incomplete-import policy from the previous test result

- [ ] OPEN
- Type: design
- Confidence: high
- Category: test-gap/false-pass
- Found by: R1-CHUNK-0002
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/tests/test_extract.ps1:L700-L704`
- Related: `android/tests/test_extract.ps1:L1123-L1129`, `android/tests/test_extract.ps1:L1203-L1209`, `game_data/CD images/d2 mac/extract_regression.json5:L31-L37`, and `game_data/CD images/Descent - Test Flight (USA)/extract_regression.json5:L28-L34`
- Evidence: `last_test_result` is written as historical output, but the next run reads its status as policy: every direct-import spec whose prior status is not `pass` sets `allowIncompleteDirectImportSkip`. When required files remain missing and the visible file set stabilizes, both the CD and ISO paths exit 0 with `status: skip`. The two assigned specs already checked in as `skip` therefore authorize the same incomplete behavior indefinitely, while an otherwise identical spec previously marked `pass` fails it
- Trigger: Run either assigned skipped spec after the importer settles without producing all expected launch files, including after a new regression removes a file that happened to be present in an earlier partial result
- Impact: Current behavior is judged by mutable historical output rather than a reviewed expectation; incomplete imports can keep the suite green as skips and never establish whether the disc is intentionally unsupported or the implementation is broken
- Expected: Re-running the same input and build produces the same pass or fail decision regardless of the prior result record, with any intentional unsupported case represented by explicit stable policy and reason
- Suggested fix: Make `last_test_result` output-only. Fail missing required files by default, and if temporary unsupported discs must be retained, add a separately named reviewed field or allowlist with a reason and expiry; let strict CI reject unexpected or expired skips
- Validation: Run the decision logic twice with identical current files but prior statuses `pass` and `skip` and assert identical outcomes; remove one required fixture file and assert failure unless an explicit supported skip policy applies; verify the D2 Mac and Test Flight dispositions are visible in the suite summary
- Resolution: Pending

### BR-0011: P2 - Use European disc identities in European regression specs

- [ ] OPEN
- Type: defect
- Confidence: high
- Category: api-data-format/test-gap
- Found by: R1-CHUNK-0004, R1-CHUNK-0005
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:game_data/CD images/Descent I and II - The Definitive Collection (Europe) (Disc 2)/extract_regression.json5:L6`, `game_data/CD images/Descent I and II - The Definitive Collection (Europe) (Disc 3)/extract_regression.json5:L6`, and `game_data/CD images/Descent II (Europe) (v1.1)/extract_regression.json5:L6`
- Related: `android/app/src/main/assets/known_discs.json5:L143`, `android/app/src/main/assets/known_discs.json5:L161`, `android/app/src/main/assets/known_discs.json5:L365`, the three European directories' `track_hashes.json`, `android/tests/validate_extract_regression_specs.ps1:L44-L113`, and `android/tests/test_extract.ps1:L1369`
- Evidence: The collection specs name the USA Disc 2 and Disc 3 IDs, and Europe v1.1 names Infinite Abyss, even though `known_discs.json5` contains dedicated European records for all three sources. Each local European `track_hashes.json` exactly matches its European database record. Each wrong target shares the data-track SHA-1 with the European source, but all eight Disc 2, seven Disc 3, and eight Europe v1.1 audio SHA-1 values differ. The validator checks source existence, CUE references, and import mode only, while the test runner uses `disc_id` only for a separate preview-disc special case, so all 34 specs validate despite these incorrect identities
- Trigger: Treat `disc_id` as the expected identification result, join regression results to disc metadata by that field, or regress runtime identification from a full European match to a one-track USA or Infinite Abyss partial match
- Impact: Results for three distinct European releases are attributed to other records, and the extraction suite provides no assertion that the app selected the intended known-disc identity; identification regressions can therefore pass while the checked-in oracle remains misleading
- Expected: Each spec names the known-disc record whose complete track manifest matches its source, and regression validation rejects a `disc_id` whose stored tracks do not match the adjacent manifest
- Suggested fix: Change the values to `descent-i-and-ii-the-definitive-collection-europe-disc-2`, `descent-i-and-ii-the-definitive-collection-europe-disc-3`, and `descent-ii-europe-v11`. Extend validation to compare adjacent track hashes with the named database record, and expose and assert the runtime identified disc ID during direct-import tests
- Validation: Run the corpus validator and all three European imports after the corrections; require exact manifest-to-database matches and assert the runtime European IDs, then temporarily substitute one of the current wrong IDs and prove validation or the test fails
- Resolution: Pending

### BR-0012: P2 - Do not mark unobserved classifications as confirmed

- [ ] OPEN
- Type: defect
- Confidence: high
- Category: test-gap/false-oracle
- Found by: R1-CHUNK-0005
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/tests/test_extract.ps1:L1353-L1374`
- Related: `android/tests/test_extract.ps1:L1413-L1424`, `android/tests/test_extract.ps1:L1466-L1488`, `android/app/src/main/java/com/dxxredux/app/SetupAutomationApi.kt:L761-L773`, and `game_data/CD images/Descent II (USA) (3-Level Interactive Preview)/extract_regression.json5:L34`
- Evidence: The setup introspection reports readiness and file names but no detected classification. The runner never compares an observed value with `spec.classification`; it only uses that expected value for messages and to choose D1 versus D2 readiness. Nevertheless it passes `ClassConfirmed $true` on non-launchable and `-SkipLaunch` passes, the preview-disc skip, automation failures, menu timeout, and the final success path. The assigned preview spec therefore records `classification_confirmed: true` after its hard-coded launch exclusion without any classification assertion
- Trigger: Run a file-only, skipped, failed-after-import, or full extraction test when the importer selected the wrong source class or when the expected classification string itself is stale
- Impact: `last_test_result` states that classification was confirmed when no such observation occurred, and the regression suite cannot detect a classification-routing regression even though the checked-in result appears to provide that coverage
- Expected: `classification_confirmed` becomes true only after comparing an independently observed importer classification with the expected class, or the field is removed or renamed if successful file and launch behavior is the intended proxy
- Suggested fix: Expose the importer's detected classification or equivalent stable type through setup introspection, compare it to `spec.classification`, and fail or record false on mismatch. Do not pass true from generic success, skip, or failure branches without that comparison
- Validation: Add decision tests with matching, mismatching, and absent observed classifications; require only the exact match to record true, and run the preview spec to confirm its skip result no longer claims classification coverage unless the importer reported `d2_demo`
- Resolution: Pending

### BR-0013: P1 - Check track capacity before writing the parsed record

- [ ] OPEN
- Type: defect
- Confidence: high
- Category: security/memory-safety
- Found by: R1-CHUNK-0007
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/app/src/main/cpp/extract/cue_parser.c:L102-L111` in `cue_parse`
- Related: `android/app/src/main/cpp/extract/cue_parser.h:L45-L51`, `android/app/src/main/cpp/extract/jni_disc_import.c:L121-L137`, `android/app/src/main/cpp/shared/rbaudio_bin.c:L537-L582`, and `android/app/src/main/cpp/extract/test_cue_iso.c:L1447-L1563`
- Evidence: For every accepted `TRACK`, the parser sets `idx = out->num_tracks` and writes `out->tracks[idx]` at L105-L109 before checking the count at L110. After 100 accepted directives, `idx` is 100 although the declared array has indices 0 through 99. The first write overlaps the following `num_tracks` field and the remaining writes continue beyond `cue_disc_t`. Track numbers need not be unique, so repeating an in-range directive reaches this state. The parser is production-linked into both Android game libraries and directly processes user-selected CUE content in the launcher, preview, extraction, fingerprint, and playback paths
- Trigger: Select or load a CUE file containing one `FILE` directive followed by 101 syntactically accepted directives such as repeated `TRACK 01 AUDIO` entries
- Impact: Parsing writes outside the native result object and can corrupt stack state or crash the launcher or game process before any caller can reject the result
- Expected: The parser checks record capacity before deriving an index or writing any track field and rejects over-capacity input without returning a partial, corrupted structure
- Suggested fix: Test `out->num_tracks >= CUE_MAX_TRACKS` before all writes, return an explicit parse failure for excess tracks, and validate that track numbers are unique and ordered if that is required by downstream identity and playback logic
- Validation: Add exact-boundary tests showing 100 records remain in bounds and the 101st is rejected, run them with AddressSanitizer or equivalent bounds instrumentation, and exercise both JNI parsing and runtime CUE loading with the oversized fixture
- Resolution: Pending

### BR-0014: P2 - Reject excess FILE directives instead of reusing the previous file

- [ ] OPEN
- Type: defect
- Confidence: high
- Category: correctness/compatibility
- Found by: R1-CHUNK-0007
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/app/src/main/cpp/extract/cue_parser.c:L82-L112` in `cue_parse`
- Related: `android/app/src/main/cpp/extract/cue_parser.h:L17-L20`, `android/app/src/main/cpp/extract/test_cue_iso.c:L1522-L1540`, `android/app/src/main/java/com/dxxredux/app/SetupDiscImport.kt:L335-L390`, and `android/app/src/main/cpp/shared/rbaudio_bin.c:L537-L598`
- Evidence: Once `num_files` reaches 50, L85 skips an additional `FILE` without changing `cur_file` or failing the parse. The following in-range `TRACK` therefore inherits file index 49 and is treated as another track in the 50th image. The existing boundary test constructs exactly this 51st FILE/TRACK pair but asserts only that `num_files` remains capped, so it passes without detecting the false association. This also conflicts with the interface's 100-track capacity and its stated support for layouts with one BIN per track
- Trigger: Import or play a multi-file CUE containing 51 FILE/TRACK pairs, or any over-limit FILE followed by a track
- Impact: The parser silently reports the later track as belonging to the previous BIN, computes its length from the wrong file size, and causes extraction, hashing, identification, preview, or playback to read the 50th image while ignoring the selected 51st image
- Expected: Every returned track maps to the FILE directive that owns it; unsupported file counts fail parsing instead of producing a plausible but incorrect disc map
- Suggested fix: Either size the file table for every supported per-track layout, likely using the same ceiling as tracks, or return failure immediately when the FILE limit is exceeded. In either case, never retain the prior `cur_file` after rejecting a FILE directive
- Validation: Extend the boundary test to assert correct file indices through the supported maximum and prompt failure one past it, add a 51-file consumer fixture when such layouts are supported, and confirm JNI ordering, fingerprinting, preview, and Redbook playback all open the intended image for the last track
- Resolution: Pending

### BR-0015: P2 - Remove the superseded disc-import JNI implementation

- [ ] OPEN
- Type: design
- Confidence: high
- Category: maintainability/partial-migration
- Found by: R1-CHUNK-0008
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/app/src/main/cpp/jni_disc_import.c:L1-L408`
- Related: `android/app/src/main/cpp/extract/jni_disc_import.c:L1-L547` and `android/app/src/main/cpp/CMakeLists.txt:L529-L536,L671-L678`
- Evidence: Both files define the same six `DiscImportBridge` JNI symbols, but both Android targets compile only `extract/jni_disc_import.c`. The root file stopped evolving after the March migration while the built copy received later ISO-image, HFS, StuffIt, append-mode, callback, and shared-extension changes and now exports four additional methods. The stale file still logs `sow` after `ReleaseStringUTFChars` and implements the old three-argument SOW extraction contract, while the built copy moved the log before release and accepts Kotlin's fourth `appendExisting` argument
- Trigger: Review, debug, or modify the prominently named root implementation instead of the compiled copy, or later add it to a source list without first discovering the duplicate definitions
- Impact: A plausible fix can compile nowhere and have no runtime effect; copying behavior from the stale file can reintroduce a JNI use-after-release or omit current import features; compiling both files produces duplicate native symbols
- Expected: Each JNI surface has one clearly built source of truth, and repository search cannot lead maintainers to an obsolete full implementation
- Suggested fix: Delete the root `jni_disc_import.c`, retain the implementation under `extract/`, and update any historical or operational references that still point to the old path. If a forwarding artifact is required, make it non-compilable documentation rather than a second implementation
- Validation: Confirm each `DiscImportBridge` native symbol has exactly one source definition, both D1 and D2 CMake targets compile that file, a clean Android build succeeds, and ISO, BIN/CUE, Mac, SOW, and StuffIt import tests still invoke the retained implementation
- Resolution: Pending

### BR-0016: P1 - Serialize launcher preview start and stop commands

- [ ] OPEN
- Type: defect
- Confidence: high
- Category: concurrency/resource-lifetime
- Found by: R1-CHUNK-0008, R1-CHUNK-0013
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/app/src/main/java/com/dxxredux/app/SetupActivity.kt:L773-L875` in `commandReceiver`
- Related: `android/app/src/main/java/com/dxxredux/app/MidiBytesPreviewDialog.kt:L58-L100,L141-L168`, `android/app/src/main/cpp/jni_cd_preview.c:L16-L125`, `android/app/src/main/cpp/jni_midi_preview.c:L21-L68`, `android/app/src/main/cpp/shared/cd_preview.c:L443-L660`, `android/app/src/main/cpp/shared/midi_preview.c:L531-L680`, `android/game_scripts/test_midi_preview_hmp_unified.json5:L27-L36`, `android/game_scripts/test_gog_installer_redbook_unified.json5:L99-L123`, and BR-0005
- Evidence: Both `music_midi_play` and `music_cd_play` launch untracked raw `Thread` instances and return from the broadcast immediately, while their stop commands call native global singletons synchronously on the receiver thread. The MIDI dialog likewise starts a blocking HOG read and native playback in an IO coroutine, but disposal calls stop without waiting; coroutine cancellation does not interrupt those non-suspending native calls or prevent the subsequent start. There is no future, generation token, executor, or mutex ordering these operations. Every native start first stops global playback and then repopulates parser, OpenSL, and render-thread state; concurrent stop joins and destroys those resources. Both integration scripts avoid the race by waiting four seconds after play before issuing stop
- Trigger: Send either preview play command followed immediately by stop, issue two play commands close together, or dismiss the MIDI dialog while its background HOG read or native start is still running
- Impact: Stop can complete before an older background start reaches native code, so playback begins after a reported stop or closed dialog; overlapping native start and stop can free parser buffers, close BIN streams, or destroy OpenSL objects while another thread uses or initializes them, causing undefined native behavior, crashes, leaks, or stale playback state
- Expected: Preview lifecycle commands execute in issue order, a completed stop prevents every older pending start from publishing playback, and native global resources cannot be initialized and destroyed concurrently
- Suggested fix: Route play, stop, and replacement-play operations through one owned serialized executor or coroutine scope with a cancellation or generation token, and add native lifecycle locking if calls can still arrive from multiple threads. Tie pending work to activity or preview ownership so teardown cancels and joins it before returning
- Validation: Add zero-delay MIDI and CD automation stress tests that repeatedly send play-stop and play-play-stop, plus a dialog test that dismisses during a delayed HOG read; assert the final state remains stopped with no later restart and run under native race or memory instrumentation while checking buffer, descriptor, render-thread, and OpenSL cleanup
- Resolution: Pending

### BR-0017: P1 - Validate a complete SIT5 entry header before reading its parent offset

- [ ] OPEN
- Type: defect
- Confidence: high
- Category: security/memory-safety
- Found by: R1-CHUNK-0009
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/app/src/main/cpp/extract/stuffit_extract.c:L331-L334` in `sit5_parse_entries`
- Related: `android/app/src/main/cpp/extract/stuffit_extract.c:L178-L190,L354-L369`, `android/app/src/main/cpp/extract/jni_disc_import.c:L481-L502`, and `android/app/src/main/java/com/dxxredux/app/SetupFileImport.kt:L378-L464`
- Evidence: The loop rejects only `current_offset >= archive_size`, then immediately reads four bytes from `archive_data + current_offset + 26`. The complete-entry check in `sit5_parse_entry` runs only after that read. The archive header controls `first_offset`, and `sit5_list_entries` accepts every value below `archive_size`; therefore an offset in the final 29 bytes passes the first check but makes the parent-offset read cross the heap buffer boundary
- Trigger: Import a 100-byte SIT5-looking file with a nonzero root count and `first_offset` set near the end of the file, such as 99
- Impact: A user-selected malformed archive causes an out-of-bounds native heap read before parsing can reject it, which can crash the launcher process and may expose adjacent-memory behavior to further parser exploitation
- Expected: No entry field is read until the parser has established that the entire minimum fixed header is present, using overflow-safe bounds arithmetic
- Suggested fix: Before reading `parent_offset` or calling the entry parser, require `current_offset <= archive_size - 48` after first proving `archive_size >= 48`. Centralize fixed-header validation so all field reads share the same checked span, and use subtractive checks for subsequent variable-length sections
- Validation: Add malformed fixtures for every offset from `archive_size - 1` through the first valid 48-byte boundary, assert clean rejection, and run the parser tests under AddressSanitizer or an equivalent native bounds checker
- Resolution: Pending

### BR-0018: P1 - Enforce extraction budgets for untrusted archive entries

- [ ] OPEN
- Type: defect
- Confidence: high
- Category: security/resource-exhaustion
- Found by: R1-CHUNK-0009, R1-CHUNK-0010, R1-CHUNK-0011, R1-CHUNK-0015
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/app/src/main/cpp/extract/stuffit_extract.c:L373-L396` in `maybe_extract_nested_sti`, `android/app/src/main/cpp/extract/mac_hfs_extract.c:L103-L130` in `extract_sti2_from_hfs`, `android/app/src/main/cpp/extract/sow_extract.c:L699-L720` in `sow_extract_impl`, and `android/app/src/main/cpp/extract/inno_reader.c:L1074-L1301` in `decompress_chunk`
- Related: `android/app/src/main/cpp/extract/stuffit_extract.c:L400-L450`, `android/app/src/main/cpp/extract/sti2_extract.c:L1879-L1935,L2040-L2069`, `android/app/src/main/cpp/extract/sow_extract.c:L500-L524,L647-L663`, `android/app/src/main/cpp/extract/inno_reader.c:L1750-L2010,L2087-L2123`, `android/app/src/main/java/com/dxxredux/app/SetupFileImport.kt:L220-L330,L410-L444`, and `android/app/src/main/java/com/dxxredux/app/SetupDialogs.kt:L941-L1014,L1220-L1290`
- Evidence: Every nonmatching, nondirectory SIT5 entry of at least 22 declared bytes is decompressed merely to determine whether it contains a nested STi archive. `extract_entry_data` trusts the archive's 32-bit uncompressed size for a single `malloc`, the result is written to a predictable temporary file, and `read_file_to_buffer` allocates another full-size buffer before inspecting it. The Mac HFS path similarly writes the complete `Install Descent` fork to disk, allocates another buffer equal to that file, and passes it to the same unbudgeted STi extractor. SOW extraction trusts archive-controlled 32-bit compressed and original sizes, allocates both complete buffers, and its bit reader pads exhausted input with zero bytes while decoding toward the declared original size. Inno's buffered path reads the complete compressed chunk, derives an initial output allocation from unchecked archive offsets and sizes, and repeatedly doubles it without an expanded-size or overflow ceiling; its streaming paths bound working memory but still decode toward attacker-controlled expanded offsets with no output-byte, ratio, or work budget. There is no shared per-entry expanded-size limit, compression-ratio limit, aggregate output budget, or native free-space recheck. Kotlin preflights use source, ZIP-entry, or listed sizes, which do not reliably bound nested or compressed output
- Trigger: Select a crafted StuffIt, SOW, or Inno archive, or a Mac HFS disc image, whose declared or valid decompressed entry sizes approach available memory or app storage
- Impact: Import can allocate gigabytes, fill app storage, terminate the launcher through memory pressure, and leave partial output or a large temporary file if the process is killed; repeating the import provides a local denial of service against the app
- Expected: Untrusted archive extraction has explicit per-entry, aggregate-memory, aggregate-output, compression-ratio, and free-space budgets, and nested detection does not require two complete expanded copies
- Suggested fix: Preflight declared expanded sizes with checked 64-bit totals before allocating or writing, reject entries above documented limits, and pass a shared extraction budget through direct, nested, buffered, and streaming decoder operations. Detect nested formats from a bounded streamed prefix, stream approved output instead of buffering it twice, use uniquely owned temporary files, reject allocation-growth overflow, and recheck usable space while writing
- Validation: Add StuffIt, SOW, HFS, and Inno fixtures whose single-entry size, aggregate size, compression ratio, offset, and decoder work are just below and above each limit; prove over-budget inputs fail before large allocation or output creation, cancellation removes temporary data, and ordinary demo installers still match their oracle hashes
- Resolution: Pending

### BR-0019: P1 - Reject HFS dot components before constructing output paths

- [ ] OPEN
- Type: defect
- Confidence: high
- Category: security/path-traversal
- Found by: R1-CHUNK-0010
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/app/src/main/cpp/extract/mac_hfs_extract.c:L152-L169` in `extract_hfs_matching_files`
- Related: `android/app/src/main/cpp/extract/hfs_reader.c:L183-L200,L345-L377,L624-L663,L669-L704`, `android/app/src/main/java/com/dxxredux/app/SetupDiscImport.kt:L237-L285`, and `android/app/src/main/java/com/dxxredux/app/SetupDialogs.kt:L1261-L1294`
- Evidence: HFS catalog names are untrusted bytes. `copy_catalog_name` replaces slash and backslash but preserves names equal to `.` or `..`; `build_catalog_path` then joins those components with `/`. The assigned extractor accepts a file when its basename has a game extension and concatenates the complete catalog path directly after `output_dir`. A directory named `..` containing `escape.hog` therefore produces `<output_dir>/../escape.hog`, which `mkdirs_for_file` and `open` resolve outside the selected set directory
- Trigger: Import a crafted HFS data track whose catalog contains a directory named `..` and a child file such as `escape.hog` with valid extents
- Impact: The archive can create or truncate extension-matching files in parent directories outside the chosen file set, corrupt another set or other app-private game data, and evade the later hoisting and cleanup that walk only within the selected set
- Expected: Every extracted destination is derived from normalized safe components and is proven to remain beneath the intended output root before any directory creation or file open
- Suggested fix: Reject empty, `.`, and `..` catalog components during decoding, build destinations through one checked path helper, and verify the normalized destination has the canonical output directory as its parent prefix. Prefer flattening approved game basenames when directory structure is not required
- Validation: Add a synthetic HFS fixture with `.`, `..`, separator, and ordinary nested components; assert malicious entries are rejected without creating anything outside a temporary output root, and run the test on Windows and a POSIX host to cover both path implementations
- Resolution: Pending

### BR-0020: P2 - Fail import when any requested archive file fails extraction

- [ ] OPEN
- Type: defect
- Confidence: high
- Category: correctness
- Found by: R1-CHUNK-0010, R1-CHUNK-0011, R1-CHUNK-0015, R1-CHUNK-0017, R1-CHUNK-0019
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/app/src/main/cpp/extract/jni_gog_import.c:L256-L298` in `extract_inno_archive`, `android/app/src/main/cpp/extract/sow_extract.c:L670-L750` in `sow_extract_impl`, `android/app/src/main/cpp/extract/inno_reader.c:L1666-L1686,L2057-L2188`, `android/app/src/main/cpp/extract/pkg_reader.c:L450-L529` in `pkg_extract_all`, and `android/app/src/main/cpp/extract/iso9660_reader.c:L413-L507` in `iso_extract_files_from_source`
- Related: `android/app/src/main/cpp/extract/extract_gog.c:L98-L188`, `android/app/src/main/cpp/extract/extract_cd.c:L338-L393,L533-L550,L608-L644`, `game_data/extract_all_gog.ps1:L121-L170`, `game_data/extract_all_cds.ps1:L81-L95,L119-L153`, `android/app/src/main/cpp/extract/jni_disc_import.c:L439-L477`, `android/app/src/main/java/com/dxxredux/app/GogImportBridge.kt:L70-L92`, `android/app/src/main/java/com/dxxredux/app/SetupDialogs.kt:L684-L819,L941-L1035`, and `android/app/src/main/java/com/dxxredux/app/SetupFileImport.kt:L280-L323`
- Evidence: The Inno loop counts each failure but returns `-1` only when no file succeeded; one successful file followed by any number of failed requested files returns a positive count. The Inno reader itself logs a GOG Galaxy external-size mismatch but returns success, and its buffered output path ignores `fclose`, so a truncated filtered file or delayed persistence failure can also be counted as extracted. The SOW loop similarly converts parser errors to end-of-input, continues after allocation, read, decompression, unsupported-method, and open failures, and returns the number of earlier successes. Its write path does not check `fwrite` or `fclose` at all and increments `extracted` even when the requested bytes were not persisted; the CD CLI treats every nonnegative SOW result, including zero or a partial count, as success. PKG extraction skips an entry whose destination cannot be opened, ignores `fclose`, and returns the count of other files; if every open fails it returns zero, which `extract_gog` reports as successful completion. The ISO reader likewise breaks an individual file's transfer on a sector read or output write error, leaves the truncated destination in place, increments `extracted`, and proceeds; destination-open failures are skipped. `extract_cd` reports any positive count as successful BIN/CUE extraction, treats every nonnegative standalone-ISO result including zero as success, and does not increment its error count when a listed BIN/CUE track extracts zero files. The batch scripts write the result manifests before checking exit status and skip existing output directories and manifests on later runs, so residue from a failed run can later be treated as complete. Kotlin likewise treats positive native counts as successful imports and does not compare every output with the analyzed or archive entry list
- Trigger: Import an Inno, SOW, PKG, or ISO source where an early file succeeds and a later requested file fails, use an unwritable output directory, extract a Galaxy entry whose filtered size differs from `external_size`, or inject a source read, destination write, or close failure
- Impact: The launcher reports a completed import and activates follow-on setup with an incomplete or inconsistent game-data set, while the only indication of missing files is a native log line
- Expected: Success means every selected file completed and passed its extraction checks; partial output is either rolled back or returned as an explicit failure with a machine-readable list of failed files
- Suggested fix: Stop on the first requested-file failure and return an error distinct from zero matches, stage outputs transactionally before replacing the set, and propagate the failed filename and reason to Kotlin. If partial imports are intentionally supported, represent that state explicitly and require caller validation before enabling Done
- Validation: Add two-file Inno, SOW, PKG, and ISO fixtures whose first file succeeds and second file fails, all-open-fail destinations, a Galaxy external-size mismatch, and readers with injected short-read, short-write, and close failures; assert no positive success status, successful CLI exit, enabled completion, or reusable partial directory, verify rollback, and retain full known-installer and disc extraction tests
- Resolution: Pending

### BR-0021: P2 - Honor and propagate documented extraction cancellation

- [ ] OPEN
- Type: defect
- Confidence: high
- Category: correctness/cancellation
- Found by: R1-CHUNK-0010, R1-CHUNK-0011, R1-CHUNK-0015
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/app/src/main/cpp/extract/jni_gog_import.c:L214-L232` in `gog_progress_cb`
- Related: `android/app/src/main/java/com/dxxredux/app/GogImportBridge.kt:L60-L67`, `android/app/src/main/cpp/extract/inno_reader.c:L1074-L1301,L1750-L2185`, `android/app/src/main/cpp/extract/pkg_reader.c:L450-L529`, `android/app/src/main/cpp/extract/sow_extract.h:L30-L35`, and `android/app/src/main/cpp/extract/sow_extract.c:L689-L692,L749-L750`
- Evidence: The Kotlin GOG and native SOW callback contracts say a nonzero return cancels extraction. Every Inno progress result is discarded at initial, decoder-loop, streamed-write, and completion notifications, and PKG likewise discards callback results, so those readers continue all work. SOW checks the callback only between files and breaks, but then returns the ordinary positive extracted count; after one completed file its callers cannot distinguish cancellation from full success and may enable Done or continue post-processing
- Trigger: Supply a progress implementation that returns 1 while extracting an InnoSetup, PKG, or multi-file SOW archive
- Impact: GOG work and callbacks continue after cancellation, while SOW can stop but be reported as successfully complete; either behavior violates caller control and can consume storage or publish a partial file set while a UI or lifecycle owner is closing
- Expected: A nonzero callback result promptly stops all extraction, closes reader and output resources, removes incomplete output, and returns a distinct canceled status without starting another file
- Suggested fix: Check and propagate every progress return through streaming and buffered decoder loops, use a distinct cancellation result instead of a generic extraction error, stop the outer Inno loop immediately, and make the JNI bridge preserve that status for Kotlin
- Validation: Add Inno, PKG, and SOW tests whose callbacks cancel at the initial, mid-file where supported, and between-file notifications; assert bounded additional work, no later callbacks or files, a distinct canceled status, removal of partial output, and clean descriptor and archive closure
- Resolution: Pending

### BR-0022: P1 - Reject invalid Huffman code lengths before table construction

- [ ] OPEN
- Type: defect
- Confidence: high
- Category: security/memory-safety
- Found by: R1-CHUNK-0011
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/app/src/main/cpp/extract/sow_extract.c:L277-L287` in `huff_make_table`
- Related: `android/app/src/main/cpp/extract/sow_extract.c:L368-L407,L409-L455,L500-L524` and `android/app/src/main/cpp/extract/jni_disc_import.c:L451-L477`
- Evidence: `huff_make_table` increments `count[bitlen[i]]` where `count` has indices 0 through 16, but it never validates the archive-derived bit length. `read_pt_len` starts at 7 and increments once for every leading set bit in a 13-bit mask, allowing a value as high as 20 before passing the array to the table builder. A value above 16 therefore writes beyond the stack array before the later table-consistency checks can reject anything; subsequent `start[len]` and `weight[len]` accesses are also out of range
- Trigger: Import a method 1 through 3 SOW entry whose compressed Huffman header encodes a nonzero table with a variable-length code above 16, such as an all-ones unary extension
- Impact: A user-selected archive performs an out-of-bounds native stack write during decompression and can crash or corrupt the launcher process before the extractor reports failure
- Expected: Every decoded code length is validated against the table format maximum before it indexes any array, and a malformed table aborts decompression without using partially initialized state
- Suggested fix: Make the length readers and table builder return checked errors, reject values outside 0 through 16 and symbol counts outside their destination arrays, propagate `huff_make_table` failures through `arj_decode_block`, and stop padding malformed input into usable decoder state
- Validation: Add boundary fixtures for lengths 0, 16, 17, and the maximum unary value, fuzz compressed table headers, and run the SOW parser under AddressSanitizer and UndefinedBehaviorSanitizer while asserting all malformed cases fail without output
- Resolution: Pending

### BR-0023: P2 - Verify ARJ header and payload CRCs before accepting output

- [ ] OPEN
- Type: defect
- Confidence: high
- Category: correctness/data-integrity
- Found by: R1-CHUNK-0011
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/app/src/main/cpp/extract/sow_extract.c:L567-L585,L699-L746` in `arj_read_entry` and `sow_extract_impl`
- Related: `android/app/src/main/cpp/extract/sow_extract.c:L211-L262,L500-L524`, `android/app/src/main/java/com/dxxredux/app/SetupDialogs.kt:L941-L1035`, and `android/app/src/main/java/com/dxxredux/app/SetupFileImport.kt:L280-L323`
- Evidence: The parser reads the four bytes following each basic header but never compares the header CRC, does not retain or verify the original-file CRC stored in the fixed file header, and accepts output solely when decoding reaches the declared size. The bit reader substitutes zero bytes after compressed input is exhausted, so a truncated stream can continue decoding rather than necessarily failing at its physical boundary. Stored entries are copied without any integrity check as well
- Trigger: Import a SOW archive with a flipped header, stored payload byte, or compressed payload byte that still permits the simplified parser or decoder to produce the declared number of bytes
- Impact: Corrupted or attacker-modified game assets are counted and presented as successfully imported, leading to later load failures, crashes, or nondeterministic game behavior with no extraction-time integrity error
- Expected: Both ARJ header metadata and every extracted payload are accepted only after their specified CRCs match, and compressed input exhaustion is a hard decode error
- Suggested fix: Parse and verify the basic and extended header CRCs before trusting sizes or names, stream the specified original-file CRC over produced bytes before committing output, track bit-reader exhaustion explicitly, and remove temporary output on any mismatch
- Validation: Add valid stored and compressed fixtures, flip one bit independently in each header and payload region, truncate the compressed stream, and assert every corrupted case fails without a committed file while known SOW fixtures retain stable hashes
- Resolution: Pending

### BR-0024: P2 - Register assertion-based SOW extraction tests with CTest

- [ ] OPEN
- Type: test-gap
- Confidence: high
- Category: test-gap
- Found by: R1-CHUNK-0011, R1-CHUNK-0017
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/app/src/main/cpp/extract/CMakeLists.txt:L92-L95,L122-L146`
- Related: `android/app/src/main/cpp/extract/test_sow_direct.c:L1-L22`, `android/app/src/main/cpp/extract/sow_extract.c:L38-L121,L181-L524`, `android/app/src/main/cpp/extract/extract_cd.c:L608-L644`, and `android/tests/test_cue_iso.ps1:L19-L34`
- Evidence: CMake builds `test_sow_direct` but does not register it with `add_test`; the seven-test native suite therefore never executes any SOW extraction. The binary itself has no content assertions and exits successfully for every nonnegative result, including zero extracted files. The CD tool's post-ISO recursive scan and extraction path is also absent from the suite. This leaves the branch's custom 768-line native ARJ parser, Huffman decompressor, scan capacity and error handling, and caller status accounting without automated success, corruption, boundary, cancellation, append-mode, or post-ISO coverage
- Trigger: Regress the decoder so a known SOW yields zero or corrupted files, break split-archive append ordering, or introduce malformed-input memory behavior, then run the documented native test script
- Impact: The required build and test pass remains green while a supported disc and demo import path is broken or unsafe, delaying detection until manual import or gameplay
- Expected: The standard native suite runs deterministic SOW tests with exact counts and content hashes for stored, compressed, retail, and split archives plus malformed security boundaries
- Suggested fix: Replace or supplement the CLI smoke tool with an assertion-based test target, use small committed synthetic fixtures for boundary behavior and available real-media oracle hashes for integration coverage, register it in CTest, and make fixture absence an explicit skip rather than a silent zero-success pass
- Validation: Confirm `ctest -N` lists the SOW suite, run it through `android/tests/test_cue_iso.ps1`, deliberately corrupt an expected hash and decoder result to prove nonzero failure, retain an append-order oracle for the three-part preview archive, and exercise the `extract_cd` scan path with none, one, excess, unreadable, and failed SOW inputs
- Resolution: Pending

### BR-0025: P3 - Correct the SOW header's obsolete libarchive description

- [ ] OPEN
- Type: documentation
- Confidence: high
- Category: documentation
- Found by: R1-CHUNK-0011
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/app/src/main/cpp/extract/sow_extract.h:L1-L6`
- Related: `android/app/src/main/cpp/extract/sow_extract.c:L1-L16,L181-L524` and `android/app/src/main/cpp/extract/CMakeLists.txt:L92-L95`
- Evidence: The public header says the module wraps libarchive, while the implementation explicitly has no external library dependency and contains its own ARJ bit reader, Huffman table builder, LZSS dictionary, header parser, and decoder. The build target links no libarchive library
- Trigger: A maintainer evaluates decoder provenance, security ownership, dependency updates, or a future format fix using the public header as the subsystem summary
- Impact: Reviewers can incorrectly assume mature third-party validation and maintenance cover untrusted SOW parsing, miss the custom decoder's testing and hardening burden, or waste time searching for a dependency that is not used
- Expected: The interface documentation accurately names the self-contained implementation, supported ARJ methods and quirks, provenance, limitations, and responsibility for validation
- Suggested fix: Replace the libarchive statement with the concise implementation and provenance summary from `sow_extract.c`, and keep supported-method and integrity limitations synchronized with the code
- Validation: Review the header against source and link dependencies, then ensure generated API documentation and repository searches contain no remaining claim that SOW extraction uses libarchive
- Resolution: Pending

### BR-0026: P1 - Validate HFS catalog record bounds before decoding

- [ ] OPEN
- Type: defect
- Confidence: high
- Category: security/memory-safety
- Found by: R1-CHUNK-0012
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/app/src/main/cpp/extract/hfs_reader.c:L546-L592` in `scan_catalog`
- Related: `android/app/src/main/cpp/extract/hfs_reader.c:L624-L720`, `android/app/src/main/cpp/extract/test_hfs.c:L361-L425,L529-L580`, and `android/app/src/main/cpp/extract/jni_disc_import.c:L509-L546`
- Evidence: The leaf node's untrusted 16-bit record count directly indexes `node + 512 - 2 * (rec_index + 1)` without proving the footer offset table fits the 512-byte node. At record index 256 the unsigned subtraction wraps and `be16` reads far outside the stack buffer. After locating a record, the only payload check is `data_off + 2 <= 512`, but a directory record reads through `data_off + 9` and a file record reads fixed fields and extents through `data_off + 97`. A type 1 or type 2 byte near the node end therefore causes additional out-of-bounds stack reads even with a small record count
- Trigger: Import a crafted HFS track containing a catalog leaf with at least 257 declared records, or a record whose aligned data offset is near byte 512 and whose first payload byte selects directory or file parsing
- Impact: A user-selected disc image can make native catalog listing read outside its stack node, crashing the launcher process or consuming adjacent stack data as file IDs, sizes, and extents before extraction begins
- Expected: The node descriptor, complete offset table, each record interval, key, and type-specific payload all fit within the node and do not overlap the offset table before any field is decoded
- Suggested fix: Parse catalog nodes with checked subtractive bounds, cap the record count by the space available after the node descriptor including the terminal offset, derive each record's start and end from the validated offset table, and require the complete directory or file payload size before calling `be32` or `parse_extent_record`. Reject the node on structural inconsistency instead of continuing with partially trusted records
- Validation: Add synthetic leaf nodes at the maximum valid record count and one above it, records ending before every fixed directory and file field, overlapping or nonmonotonic offsets, and valid boundary records; assert clean rejection and run the HFS suite plus a malformed corpus under AddressSanitizer and UndefinedBehaviorSanitizer
- Resolution: Pending

### BR-0027: P2 - Reuse one heap-backed HFS catalog during extraction

- [ ] OPEN
- Type: defect
- Confidence: high
- Category: performance/resource-lifetime
- Found by: R1-CHUNK-0012
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/app/src/main/cpp/extract/hfs_reader.c:L624-L720` in `hfs_list_files` and `hfs_extract_file`
- Related: `android/app/src/main/cpp/extract/hfs_reader.h:L15-L41`, `android/app/src/main/cpp/extract/mac_hfs_extract.c:L133-L172`, and `android/app/src/main/java/com/dxxredux/app/SetupDialogs.kt:L1234-L1289`
- Evidence: `hfs_file_list_t` embeds 1,024 324-byte entries, so every caller materializes a 331,780-byte value. The fallback Mac extractor keeps that list live while calling `hfs_extract_file` for every matching file; each call allocates another complete list, loads the volume, and scans the whole catalog again, while `hfs_list_files` adds a 1,024-entry internal catalog array. NDK r27d AArch64 Clang at `-O2` reports stack frames of 332,960 bytes for the inlined Mac fallback, 397,584 bytes for `hfs_extract_file`, and 103,328 bytes for `hfs_list_files`, for roughly 834 KiB of nested native frames before JNI, Kotlin, and runtime frames. Extracting K matching files also changes one catalog scan into K additional full scans and volume loads
- Trigger: Use the supported loose-file fallback on a Mac HFS image without a usable `Install Descent` archive and with several extension-matching game files, especially from the Kotlin `Dispatchers.IO` JNI path
- Impact: Import performs avoidable repeated I/O and parsing for every selected file and consumes most of a MiB of native stack at peak, increasing latency and leaving little stack headroom on mobile or host worker threads even for ordinary valid input
- Expected: One validated catalog and volume context is allocated with explicit ownership, reused for selection and extraction, and released after the import; per-file extraction uses the already selected entry and only a bounded transfer buffer
- Suggested fix: Replace the fixed-array value API with an opaque or caller-owned heap-backed HFS context, scan once, pass a validated entry to an extraction helper, and make file counts and allocation failure explicit. Keep the transfer buffer bounded or heap-backed and avoid loading the volume again inside the per-entry path
- Validation: Compile Android ABIs with a scoped frame-size warning, assert the HFS call chain has no large fixed catalog frames, instrument a synthetic many-file fallback to prove one catalog scan regardless of selected-file count, and retain exact listing and extraction checks for both known Mac discs
- Resolution: Pending

### BR-0028: P1 - Serialize MIDI seek with rendering

- [ ] OPEN
- Type: defect
- Confidence: high
- Category: concurrency/resource-lifetime
- Found by: R1-CHUNK-0013
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/app/src/main/cpp/jni_midi_preview.c:L63-L78` in `nativeSeek` and `nativeGetState`
- Related: `android/app/src/main/cpp/shared/midi_preview.c:L287-L379,L636-L694` and `android/app/src/main/java/com/dxxredux/app/MidiBytesPreviewDialog.kt:L63-L75,L155-L168`
- Evidence: The Compose slider calls `nativeSeek` on the main thread while the MIDI render thread continuously runs `render_midi_frames`. Seek resets and reconfigures the non-thread-safe TinySoundFont instance, rewinds and advances the shared linked-list cursor, rewrites the shared playback double, and resets ring positions while rendering simultaneously mutates the same synth, cursor, time, and ring. No mutex, command queue, pause handshake, or atomic ownership protects these operations. The 100 ms state poll also reads the concurrently written `double s_playback_msec` and related plain integers without synchronization
- Trigger: Start a MIDI or HMP preview and release the position slider while playback is active, especially while the render thread is dispatching notes; repeated slider releases widen the race window
- Impact: Concurrent synth and cursor mutation is undefined native behavior that can corrupt preview state or heap internals, crash the launcher, return torn or nonsensical positions, and produce stale or corrupted audio
- Expected: The render thread has exclusive ownership of synth, message cursor, playback time, and ring reset; UI controls submit ordered commands and state queries consume a synchronized snapshot
- Suggested fix: Route seek, pause, resume, and stop through a render-thread command queue or guard all shared playback state with one lifecycle mutex and stop rendering during seek. Publish position, duration, and state through atomics or a locked snapshot, and define callback ordering during ring reset
- Validation: Add a preview test that performs rapid seeks while playing and while paused, verifies bounded monotonic state after each completed seek, and stress it under ThreadSanitizer where available plus AddressSanitizer on an Android or host OpenSL test shim
- Resolution: Pending

### BR-0029: P1 - Marshal overlay game-state access through the engine thread

- [ ] OPEN
- Type: defect
- Confidence: high
- Category: concurrency
- Found by: R1-CHUNK-0013
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/app/src/main/cpp/jni_main.c:L1193-L1255,L1272-L1314,L1352-L1410`
- Related: `android/app/src/main/java/com/dxxredux/app/MainActivity.kt:L860-L940,L1025-L1124,L1472-L1573,L1684-L1725`, `android/app/src/main/java/com/dxxredux/app/VideoInfoOverlay.kt:L223-L300`, `android/app/src/main/java/com/dxxredux/app/WarpButtonOverlay.kt:L73-L94`, and `android/app/src/main/cpp/shared/coop/coop_warp.c:L41-L169,L187-L291`
- Evidence: MainActivity launches the engine in a dedicated raw thread, but main-looper View handlers call the assigned JNI methods directly. The 500 ms warp poll runs a BFS over mutable `Segments` and `Walls` and dereferences live `Players` and `Objects`; touch callbacks call `coop_warp_execute` and `coop_warp_cycle_target` on the UI thread, directly moving and relinking the player object, advancing shared RNG, writing the shared multiplayer packet buffer, sending network data, and posting HUD messages while simulation and networking continue on the engine thread. Video polling concurrently walks mutable bitmap and texture records, and escort providers read owner and player state during multiplayer transitions. None of these paths uses a command queue, lock, atomic snapshot, or engine-thread assertion
- Trigger: Display the video, warp, or Guide-Bot overlay during active gameplay or a level or player transition, tap warp or cycle while a simulation or network frame is updating the same state, or disconnect an escort owner during an overlay callsign query
- Impact: The UI and engine threads race on native world, renderer, RNG, HUD, and network state; a warp can corrupt object-to-segment links or the shared packet buffer, and polling can observe invalid transitional indices or inconsistent texture data, causing crashes, desynchronization, malformed packets, or misleading controls
- Expected: Only the engine thread traverses or mutates engine-owned state. UI commands are queued into the game event loop, and overlays read immutable snapshots published at a defined frame boundary
- Suggested fix: Add one Android game-command bridge for warp and cycle actions, process it from the engine loop, and publish compact video, warp, and escort snapshots with explicit synchronization. Reuse that boundary for existing overlay providers and add debug assertions that world mutation runs on the engine thread
- Validation: Add an integration test that polls every assigned overlay while repeatedly loading levels and disconnecting players, queues warp and cycle during active multiplayer frames, and verifies engine-thread assertions, valid object segment links, packet contents, and stable UI snapshots under ThreadSanitizer or race instrumentation
- Resolution: Pending

### BR-0030: P1 - Check MIDI audio initialization results before use

- [ ] OPEN
- Type: defect
- Confidence: high
- Category: resource-lifetime
- Found by: R1-CHUNK-0013
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/app/src/main/cpp/jni_midi_preview.c:L29-L39` in `nativeStart`
- Related: `android/app/src/main/cpp/shared/midi_preview.c:L382-L399,L419-L497,L531-L602`
- Evidence: The JNI bridge reports the native start result, but `osl_init` checks only object-creation calls. It ignores every `Realize`, `GetInterface`, callback registration, play-state, and initial `Enqueue` result, then dereferences the returned engine, play, and buffer-queue interfaces unconditionally. If realization or interface acquisition fails, the next function-table call can use a null or unusable interface. `render_thread_start` also returns no status: a failed `pthread_create` is logged while `midi_preview_start` still returns success with OpenSL running and no producer
- Trigger: Start a preview while OpenSL cannot realize an engine or player, cannot provide the required buffer-queue interface, rejects a queue operation, or the process cannot create another native thread because audio or thread resources are exhausted
- Impact: A recoverable audio initialization failure can become a native null-interface crash; thread-creation failure is falsely reported as playing and leaves an initialized silent player until a later stop
- Expected: Every fallible OpenSL and pthread step is checked before its result is used, partial objects are destroyed in reverse order, and start returns false with the preview fully stopped
- Suggested fix: Make `osl_init` and `render_thread_start` use a single checked cleanup path, verify each required interface and operation, set global objects only after successful initialization, and propagate a distinct failure to JNI. Keep stop idempotent for every partial state
- Validation: Wrap OpenSL and pthread entry points in a testable adapter, inject failure at every initialization step, and assert no null call, success result, live thread, queued buffer, or retained engine, mix, player, MIDI, or ring state remains; retain a real-device successful playback test
- Resolution: Pending

### BR-0031: P2 - Reject encrypted STi2 entries before extraction

- [ ] OPEN
- Type: defect
- Confidence: high
- Category: correctness/compatibility
- Found by: R1-CHUNK-0014
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/app/src/main/cpp/extract/sti2_extract.c:L323-L345,L1999-L2010` in `is_valid_method` and `sti2_list_entries`
- Related: `android/app/src/main/cpp/extract/sti2_extract.c:L44-L49,L1879-L1942` and `android/app/src/main/cpp/extract/sti2_extract.h:L19-L41`
- Evidence: The parser defines the StuffIt encrypted flag, accepts a method byte after examining only its low compression bits, then stores only `method & 0x0f` in the public entry. It does not retain an encrypted bit, padding, or entry key. Extraction therefore treats encrypted method `0x80` as stored method 0 and copies encrypted bytes as successful output, or sends encrypted bytes to methods 13 through 15 as if they were compressed plaintext. The referenced XAD parser instead marks these entries encrypted and handles their 16-byte key and padding metadata
- Trigger: Import a valid password-protected legacy StuffIt or STi archive containing a data fork whose filename matches a requested game extension
- Impact: The importer can report a successfully extracted game file that is ciphertext or decoder garbage, or fail with no indication that encryption rather than archive corruption is unsupported
- Expected: Unsupported encrypted entries are identified during listing and rejected before allocation or output creation; any future password support preserves and consumes all required encryption metadata
- Suggested fix: Add an explicit encrypted field to the internal and public entry metadata, preserve the full method flags, reject encrypted matching entries with a distinct unsupported-encryption result, and only add decryption after the key, padding, and password contract is implemented
- Validation: Build a valid-header fixture for stored and compressed entries with `STI2_ENCRYPTED_FLAG`, including compressed sizes below and above the 16-byte key requirement, and assert listing records encryption while extraction creates no output and returns the documented unsupported status
- Resolution: Pending

### BR-0032: P2 - Parse STi2 entries in archive order and enforce the declared structure

- [ ] OPEN
- Type: defect
- Confidence: high
- Category: correctness/data-integrity
- Found by: R1-CHUNK-0014
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/app/src/main/cpp/extract/sti2_extract.c:L471-L512` in `scan_headers`
- Related: `android/app/src/main/cpp/extract/sti2_extract.c:L1964-L2021` and `android/app/src/main/cpp/extract/test_sti2.c:L313-L430`
- Evidence: `scan_headers` tests every byte position in the declared archive span for a 112-byte sequence with plausible methods, a printable name, and a valid header CRC. It does not start at the first file header and advance by `112 + resource_compressed_size + data_compressed_size`, and `sti2_list_entries` records but never reconciles the declared file count. An invalid expected header is silently skipped, while a CRC-valid header-shaped sequence embedded in compressed data is accepted as another entry. The referenced XAD parser reads the next header at the current structural offset, raises on its CRC failure, and seeks over the two declared compressed forks before reading the next header
- Trigger: Flip a bit in one expected file header while leaving other entries intact, make a compressed fork contain a crafted valid header sequence, or declare a file count inconsistent with the structurally reachable headers
- Impact: Listing and extraction can return positive success after silently omitting a damaged requested file, or expose a false entry sourced from payload bytes; callers then publish a partial or misleading import instead of rejecting the malformed archive
- Expected: Every header is reached through checked archive structure, its complete fork spans fit, invalid expected headers fail the archive, and the parsed entry and folder structure is consistent with the declared count
- Suggested fix: Begin at `STI2_ARCHIVE_HEADER_SIZE`, parse one validated header at a time, use checked additions for both compressed fork lengths, advance exactly to the next header, validate folder nesting, and reject count or end-offset mismatches. If known self-extracting variants require a prefix, detect and bound that prefix explicitly rather than scanning payload bytes
- Validation: Add fixtures for a corrupted middle header, a fake valid header inside stored and compressed payloads, truncated fork spans, overflowing length sums, unbalanced folders, too few and too many declared entries, and a valid known archive; assert malformed cases fail without any committed output
- Resolution: Pending

### BR-0033: P2 - Verify STi2 payload checksums and compressed input exhaustion

- [ ] OPEN
- Type: defect
- Confidence: high
- Category: correctness/data-integrity
- Found by: R1-CHUNK-0014
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/app/src/main/cpp/extract/sti2_extract.c:L1879-L1942` in `extract_entry_data`
- Related: `android/app/src/main/cpp/extract/sti2_extract.c:L59-L63,L699-L733,L1493-L1522,L1759-L1768,L1814-L1876,L2040-L2069` and `android/app/src/main/cpp/extract/test_sti2.c:L433-L508`
- Evidence: StuffIt file headers carry resource and data fork CRC16 values at offsets 100 and 102, but the parser does not read either value and accepts stored or decompressed output solely by declared size. Both bit readers synthesize zero bits after physical compressed input ends, so truncated method 13, 14, or 15 input can continue decoding. Method 15 additionally reads its terminal 32-bit checksum into `ignored_crc` and never compares it with the produced bytes. A flipped byte in a stored entry is therefore always written and reported as success, and some damaged compressed streams can also reach the requested output length without an integrity failure
- Trigger: Corrupt a stored payload byte, truncate or alter a compressed fork while preserving its declared sizes and header CRC, or alter the terminal method 15 checksum
- Impact: Corrupted or attacker-modified game assets can be counted and presented as successfully imported, causing later load failures, crashes, or nondeterministic gameplay with no extraction-time integrity signal
- Expected: Decoders fail on any read beyond the compressed span and an entry is committed only when its format checksum matches the complete produced payload
- Suggested fix: Preserve the data and resource CRC16 fields in entry metadata, make both bit readers carry a hard exhaustion state, compare method 15's terminal CRC32, verify the header CRC16 over every completed data fork, and write through a temporary file that is removed on any decode, checksum, write, or close failure
- Validation: Add stored and methods 13, 14, and 15 fixtures, flip payload and checksum bits independently, truncate at each decoder boundary, and assert every corrupted case fails without output while the real-media and corpus oracle hashes remain unchanged; run the malformed corpus under AddressSanitizer and UndefinedBehaviorSanitizer
- Resolution: Pending

### BR-0034: P2 - Verify Inno file checksums before committing output

- [ ] OPEN
- Type: defect
- Confidence: high
- Category: correctness/data-integrity
- Found by: R1-CHUNK-0015
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/app/src/main/cpp/extract/inno_reader.c:L2057-L2188` in `inno_extract_file`
- Related: `android/app/src/main/cpp/extract/inno_reader.c:L1013-L1017,L1074-L1301,L1666-L2054`, `android/app/src/main/cpp/extract/inno_reader.h:L85-L104`, and `android/app/src/main/cpp/extract/test_gog_fd.c:L1-L159`
- Evidence: The data-entry parser preserves each Inno 5.3.9-or-newer file's 20-byte SHA-1 value, but no extraction path ever reads that field. Stored, zlib, LZMA1, LZMA2, and GOG Galaxy outputs are accepted using decoder status and declared size only. The referenced innoextract implementation applies the per-file checksum filter during extraction; this reader omits the equivalent integrity decision, and the native GOG test only lists installer entries without extracting or corrupting payloads
- Trigger: Flip a stored payload byte, or alter compressed input so decoding still produces the declared byte count, while leaving the file's recorded checksum unchanged
- Impact: Corrupted or attacker-modified game files can be reported and published as successfully imported, leading to delayed load failures, crashes, or nondeterministic behavior without identifying the damaged installer
- Expected: Every extracted file is committed only after its version-appropriate checksum is calculated over the format-defined byte stream and matches the data entry
- Suggested fix: Preserve the checksum algorithm with each data entry, stream MD5 or SHA-1 calculation through every output path, mirror the format's filter ordering for GOG Galaxy entries, compare before final publication, and write to a temporary file that is removed on mismatch
- Validation: Add stored, zlib, LZMA1, LZMA2, and Galaxy fixtures with known checksums; flip payload and checksum bits and truncate inputs independently; assert every mismatch fails with no final output while valid real-installer hashes remain unchanged
- Resolution: Pending

### BR-0035: P2 - Bound stored Inno file ranges to their declared chunk

- [ ] OPEN
- Type: defect
- Confidence: high
- Category: security/data-boundary
- Found by: R1-CHUNK-0015
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/app/src/main/cpp/extract/inno_reader.c:L1750-L1806` in `stream_chunk_file_range`
- Related: `android/app/src/main/cpp/extract/inno_reader.c:L132-L146,L2025-L2054,L2087-L2117`
- Evidence: The stored streaming branch calculates the physical start from unchecked `data_offset + chunk_offset + 4 + file_offset` and copies exactly `file_size` bytes from the installer without proving `file_offset + file_size` fits `chunk_compressed_size`. It then assigns `outer_pos = range_end` and tests `outer_pos < range_end`, a condition that is necessarily false. The lower-level read helper checks only whether the requested physical bytes exist, so bytes after the declared chunk can satisfy the copy
- Trigger: Craft a stored data entry whose file range extends beyond its declared chunk while placing readable bytes, such as another chunk or installer metadata, immediately afterward; overflowing offset sums provide additional malformed cases
- Impact: Extraction can copy unrelated following installer data into a requested game file and report success, violating archive boundaries and potentially exposing embedded metadata or producing attacker-composed output
- Expected: All offset additions are overflow-checked and a stored entry is rejected unless its complete range is contained within the declared chunk and physical archive span
- Suggested fix: Use checked additions for `chunk_pos`, `range_end`, and `copy_start`; require `file_offset <= chunk_compressed_size` and `file_size <= chunk_compressed_size - file_offset` before reading; validate the resulting physical span against the archive; and remove the tautological post-copy check
- Validation: Add exact-boundary, one-byte-over, adjacent-sentinel, and near-`UINT64_MAX` fixtures for offsets and sizes; assert malformed entries fail without output and never read sentinel bytes
- Resolution: Pending

### BR-0036: P2 - Consume the MD5 field in pre-5.3.9 Inno data entries

- [ ] OPEN
- Type: defect
- Confidence: high
- Category: compatibility/parser-correctness
- Found by: R1-CHUNK-0015
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/app/src/main/cpp/extract/inno_reader.c:L1013-L1024` in `parse_data_entries` and `android/app/src/main/cpp/extract/inno_reader.h:L8-L10`
- Related: `android/app/src/main/cpp/extract/inno_reader.c:L505-L570,L922-L1034` and the public `inno_open` API in `android/app/src/main/cpp/extract/inno_reader.h:L107-L120`
- Evidence: The public header claims support for all Inno 5.3.x Unicode installers. The parser consumes a 20-byte SHA-1 only at version 5.3.9 or newer and consumes no checksum for earlier claimed versions, then immediately reads timestamp and version fields. The referenced innoextract version layout shows that installers from 4.2.0 through 5.3.8 carry a 16-byte MD5 field there. For 5.3.0 through 5.3.8 this parser therefore remains 16 bytes behind and interprets checksum bytes as subsequent fields, misaligning the rest of the entry table
- Trigger: Open a valid Unicode Inno 5.3.0 through 5.3.8 installer containing one or more data entries
- Impact: A format version explicitly advertised as supported can fail to list or extract, or can produce corrupted offsets, sizes, flags, and later entries from the misaligned table
- Expected: Each accepted installer version consumes its exact data-entry layout, or unsupported versions are rejected before any table is parsed
- Suggested fix: Represent checksum kind and bytes explicitly, consume MD5 for the applicable pre-5.3.9 versions and SHA-1 afterward using one version-layout helper, and reject versions whose complete layout is not implemented. Pair this with BR-0034 so parsed checksums are also enforced
- Validation: Add valid multi-entry installers or faithful synthetic tables at 5.3.0, 5.3.8, and the 5.3.9 transition; verify subsequent timestamps, flags, offsets, listing, extraction, and checksum decisions, plus clean rejection immediately outside the supported matrix
- Resolution: Pending

### BR-0037: P3 - Synchronize the Inno reader capability documentation with the implementation

- [ ] OPEN
- Type: maintainability
- Confidence: high
- Category: documentation
- Found by: R1-CHUNK-0015
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/app/src/main/cpp/extract/inno_reader.h:L1-L13`
- Related: `android/app/src/main/cpp/extract/inno_reader.c:L8-L16,L1237-L1300,L1943-L2010` and `android/app/src/main/cpp/extract/inno_reader.h:L85-L120`
- Evidence: The public header says only LZMA1 and zlib are implemented and that LZMA2 returns an error, while the source contains both buffered and streaming LZMA2 decoders and has done so since the file was introduced. The same preamble broadly claims 5.3.x support without documenting the checksum-layout gap in BR-0036, uses the label `AI-slop` instead of a maintainable provenance statement, and does not state the callback's cancellation contract
- Trigger: A maintainer uses the public header to decide which installer fixtures, decompressor paths, or callback behaviors require review and regression coverage
- Impact: Reviewers and callers receive a false support matrix, can omit security and compatibility coverage for live LZMA2 code, and cannot reliably distinguish implemented, unsupported, and unverified format features
- Expected: The public API states the tested version and compression matrix, unsupported filters or encryption, callback semantics, and accurate upstream-derived provenance in neutral project language
- Suggested fix: Replace the stale preamble with a concise maintained support table or linked design note, document LZMA2 and callback cancellation accurately, narrow version claims until BR-0036 is fixed, and describe the implementation's relationship to innoextract without informal or ambiguous authorship language
- Validation: Review the documentation against the compiled method switch, parser version gates, callback tests, and a fixture matrix; add a lightweight checklist so capability changes update documentation and coverage together
- Resolution: Pending

### BR-0038: P2 - Use the canonical fingerprint threshold in the duplicate matcher

- [ ] OPEN
- Type: defect
- Confidence: high
- Category: correctness
- Found by: R1-CHUNK-0016
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/app/src/main/cpp/extract/fingerprint_match.c:L288-L301` in `main`
- Related: `android/app/src/main/assets/fingerprint_config.json5:L1-L15`, `game_data/update_known_discs_albums.ps1:L24-L35,L158-L174`, and `android/app/src/main/cpp/shared/chromaprint_db.c:L24-L37`
- Evidence: The command documents its threshold argument as optional and hardcodes 0.40 when it is omitted. The repository's declared single source of truth sets 0.65 and explains that random-noise comparisons peak near 0.50, so the tool's default lies in the false-match region. The merger normally passes the configured value, but it also falls back to 0.40 when configuration loading fails. On the locally available 761-entry merged input, the matcher reported 51,571 pairs at 0.40 versus 1,577 at 0.65. Command-line parsing uses `atof`, so a value such as `nan` also passes both range comparisons and silently makes every `score >= threshold` test false
- Trigger: Invoke the documented command without a threshold, let the merger fall back after a missing or malformed configuration, or pass a non-finite or partially numeric threshold
- Impact: Database regeneration can classify tens of thousands of unrelated tracks as duplicates or, for NaN, classify none, producing incorrect exclusions and unstable album-to-disc mappings while the command exits successfully
- Expected: Every matching surface consumes one validated finite threshold from the canonical configuration, and invalid or unavailable configuration fails instead of selecting a known-unsafe fallback
- Suggested fix: Require the caller to pass the canonical value or add an explicit config-file argument, remove the independent 0.40 defaults, and parse with `strtof` plus complete-consumption, range, and `isfinite` checks. Keep runtime, scripts, and the audit tool on one loaded configuration contract
- Validation: Add matcher integration cases at 0.40, 0.65, omitted, malformed, trailing-junk, infinity, and NaN inputs; assert invalid configuration fails nonzero and the repository merger uses exactly the configured threshold
- Resolution: Pending

### BR-0039: P2 - Fail duplicate analysis when any fingerprint entry is not loaded

- [ ] OPEN
- Type: defect
- Confidence: high
- Category: correctness/test-tool
- Found by: R1-CHUNK-0016
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/app/src/main/cpp/extract/fingerprint_match.c:L190-L271` in `load_db`
- Related: `android/app/src/main/cpp/extract/fingerprint_match.c:L23-L27,L109-L137,L304-L325` and `game_data/update_known_discs_albums.ps1:L95-L174`
- Evidence: The loader stops successfully at `MAX_ENTRIES` without checking for remaining objects, silently truncates each encoded fingerprint into an 8,192-byte stack buffer, and silently omits entries whose truncated or malformed fingerprint does not decode or whose allocation fails. The file reader also ignores all seek and read return values and passes the declared size after a short read. The merger knows how many objects it wrote but checks only the process exit code, so a partial load is indistinguishable from complete duplicate analysis. Current data fits at 761 merged entries with a 3,854-character maximum fingerprint, but neither public command contract states the limits nor prevents future growth or a single bad generated record from creating false negatives
- Trigger: Supply more than 4,096 entries, an encoded fingerprint longer than 8,191 bytes, a malformed fingerprint among valid records, an allocation failure, or a short database read
- Impact: Some tracks receive no duplicate comparison, yet the successful result is used to regenerate `known_discs.json5`; duplicate album tracks can be published and later compete with canonical CD identities without any failing build or diagnostic tied to the omitted record
- Expected: Success proves every input object was parsed, decoded, and compared exactly once, with explicit bounded-input errors rather than silent omission
- Suggested fix: Use a strict JSON parser or shared validated loader with dynamically sized fingerprint strings and entries, check file size and every seek/read operation, fail on any malformed or undecodable required record, and report a machine-readable loaded count that the merger compares with its input count. If caps remain, reject at the first excess item
- Validation: Add exact-limit and one-over databases, an overlength fingerprint, malformed middle and final entries, forced allocation failure, and injected short reads; assert nonzero exit and no consumable result for every incomplete load, plus equality between written, loaded, and compared counts on valid input
- Resolution: Pending

### BR-0040: P2 - Make fingerprint duration tolerance independent of entry order

- [ ] OPEN
- Type: defect
- Confidence: high
- Category: correctness
- Found by: R1-CHUNK-0016
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/app/src/main/cpp/extract/fingerprint_match.c:L84-L97` in `best_similarity`
- Related: `android/app/src/main/assets/fingerprint_config.json5:L10-L15`, `android/app/src/main/cpp/shared/chromaprint_db.c:L311-L340`, and `game_data/update_known_discs_albums.ps1:L64-L150`
- Evidence: Pair similarity is intended to be symmetric, but the duration prefilter divides `a->duration_ms` by `b->duration_ms` and applies fixed bounds of 0.90 through 1.10. Durations 90,000 and 100,000 therefore pass in that order, while the same pair reversed yields 1.111 and is rejected before fingerprint comparison. The merger's construction order places CD entries before albums and sorts albums, so whether an otherwise identical boundary pair is considered can depend on which source happens to be longer and on input ordering. The runtime database matcher repeats the same one-direction ratio against its query
- Trigger: Compare identical or qualifying fingerprints whose positive durations lie in the asymmetric part of the tolerance boundary, such as 90,000 and 100,000 milliseconds, then swap their order
- Impact: Duplicate discovery and runtime identification can disagree or change after harmless database reordering, leaving a duplicate album track unexcluded or rejecting a valid query depending only on which duration is used as the denominator
- Expected: The configured duration tolerance defines one documented symmetric comparison, and swapping two entries cannot change eligibility or score
- Suggested fix: Centralize a checked duration-compatibility helper using an explicitly symmetric denominator, such as the larger duration, or reciprocal bounds derived from the chosen relative-error definition; use it in both audit and runtime matchers
- Validation: Add below, exact, and above-boundary duration pairs in both orders with identical fingerprints, assert identical decisions, and run the same vectors through `fingerprint_match` and `chromaprint_db_match`
- Resolution: Pending

### BR-0041: P3 - Establish one owner for native and Kotlin extension policy

- [ ] OPEN
- Type: maintainability
- Confidence: high
- Category: maintainability
- Found by: R1-CHUNK-0016
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/app/src/main/cpp/extract/game_file_extensions.h:L8-L17`
- Related: `android/app/src/main/cpp/extract/game_file_extensions.c:L13-L47`, `android/app/src/main/java/com/dxxredux/app/AndroidGameFileExtensions.kt:L3-L16`, `android/app/src/main/java/com/dxxredux/app/GameFileFormats.kt:L41-L224,L310-L316`, and `android/app/src/test/java/com/dxxredux/app/AndroidGameFileExtensionsTest.kt:L7-L21`
- Evidence: The C header calls its tables the single source of truth and says to align them with Kotlin, while the Kotlin wrapper says format knowledge lives in `GameFileFormats` and must be aligned with C. Both sides therefore claim ownership while duplicating game-import, disc-extract, and GOG-audio policy in different representations, and the Mac native list adds a third specialized set. The shared game and generic disc values currently agree, but the only focused test asserts Kotlin behavior and cannot detect native drift. History already shows the native disc table being removed, specialized, and re-added independently as import support changed
- Trigger: Add or reclassify a game format in `GameFileFormats` or one native table while following that file's source-of-truth comment, without discovering and updating every mirror
- Impact: Kotlin can classify and offer a file that native GOG, PKG, ISO, HFS, or StuffIt extraction silently omits, or native code can extract a format that launcher validation does not recognize; contradictory ownership makes the omission likely to survive review
- Expected: One artifact owns extension roles, every language consumes generated or mechanically verified values, and platform-specific exceptions are explicit rather than implicit list drift
- Suggested fix: Define the role flags in one neutral manifest and generate the compact C tables and Kotlin map, or designate one side as authoritative and add a cross-language parity test that parses the other. Rewrite both comments to state the actual ownership and document why the Mac set differs
- Validation: Add a test that compares every shared role and case-insensitive lookup across native and Kotlin representations, deliberately change one side to prove the test fails, and retain focused cases for GOG audio, generic disc extraction, and Mac-only exceptions
- Resolution: Pending

### BR-0042: P2 - Encode every extractor filename as valid JSON

- [ ] OPEN
- Type: defect
- Confidence: high
- Category: api-data-format
- Found by: R1-CHUNK-0017
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/app/src/main/cpp/extract/extract_gog.c:L116-L130,L169-L176`
- Related: `android/app/src/main/cpp/extract/extract_cd.c:L619-L639`, `android/app/src/main/cpp/extract/fingerprint_audio.c:L121-L129,L154-L184`, `game_data/extract_all_gog.ps1:L140-L153`, `game_data/fingerprint_music_packs.ps1:L308-L329`, and `game_data/fingerprint_mission_zip_music.ps1:L719-L742`
- Evidence: `extract_gog` interpolates archive-controlled Inno destinations and CPIO names directly inside JSON strings without escaping quotes, backslashes, control bytes, or encoding errors. The CD tool normalizes backslashes in a SOW path but otherwise emits it raw. `fingerprint_audio` escapes only quote and backslash, leaving newline, tab, carriage return, and other control bytes from a legal POSIX filename unencoded. These commands advertise JSON output, and the fingerprint workflows pass stdout directly to `ConvertFrom-Json`; one such name makes the complete otherwise-successful result syntactically invalid while the native command can exit zero
- Trigger: List an installer entry or fingerprint an audio file whose name contains a quote, backslash, newline, tab, another byte below 0x20, or a filename encoding that is not valid UTF-8 for the consumer
- Impact: Extraction reports and generated fingerprint metadata become unparsable or can be structurally misrepresented, aborting batch regeneration after expensive decoding and preventing callers from reliably associating results with source files
- Expected: Every command emits valid UTF-8 JSON for every filename accepted by its platform, with complete JSON string escaping and an explicit policy for unrepresentable native names
- Suggested fix: Replace the three ad hoc emitters with one small tested JSON-string writer or a lightweight existing serializer that escapes all required control characters and validates or converts platform filenames to UTF-8; keep diagnostic text exclusively on stderr
- Validation: Add CLI integration fixtures with quote, backslash, tab, newline, control-byte, non-ASCII, and invalid-encoding names where the platform permits them; parse stdout with strict JSON readers on Windows and POSIX and verify exact round trips or documented rejection
- Resolution: Pending

### BR-0043: P2 - Fail audio fingerprinting when directory enumeration is incomplete

- [ ] OPEN
- Type: defect
- Confidence: high
- Category: correctness/test-tool
- Found by: R1-CHUNK-0017
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/app/src/main/cpp/extract/fingerprint_audio.c:L79-L118` in `collect_audio_files`
- Related: `android/app/src/main/cpp/extract/fingerprint_audio.c:L30-L36,L132-L187`, `game_data/fingerprint_music_packs.ps1:L308-L329`, `game_data/fingerprint_mission_zip_music.ps1:L700-L742`, and `android/tests/test_fpcalc_and_acoustid.ps1:L121-L155`
- Evidence: Both `FindFirstFileA` and `opendir` failure return the same zero count as an empty directory, and `main` prints `[]` and exits zero. Direct execution against a nonexistent directory confirmed that behavior. Enumeration also stops silently at `MAX_FILES` and does not distinguish `FindNextFileA` or `readdir` errors from end-of-directory. The fingerprint generators accept a successful nonempty subset without comparing it to the extracted or directory audio count, so capacity or mid-scan omissions are publishable; the ANSI Windows API additionally makes unrepresentable paths another fail-open enumeration case
- Trigger: Pass a missing, unreadable, or non-ANSI Windows directory, inject a directory-read error, or fingerprint a directory containing more than 4,096 matching files
- Impact: The tool can report successful empty or partial analysis, causing music packs or mission archives to lose track fingerprints and later database matches without identifying which source files were never examined
- Expected: Success proves complete enumeration of the requested directory and every eligible file is either represented in output or causes a nonzero result with an explicit failure record
- Suggested fix: Return distinct empty, error, and capacity-exceeded states; use checked dynamic storage or reject on the first excess file; inspect terminal enumeration errors; use wide-character Windows enumeration with UTF-8 conversion; and have callers compare enumerated, fingerprinted, and expected counts before publishing metadata
- Validation: Test empty, nonexistent, unreadable, non-ASCII, exact-limit, one-over, and injected mid-enumeration-error directories on Windows and POSIX; assert only a genuinely empty readable directory succeeds with `[]` and every valid audio filename appears exactly once
- Resolution: Pending

### BR-0044: P1 - Make repeated overlay JNI callbacks resource- and exception-safe

- [ ] OPEN
- Type: defect
- Confidence: high
- Category: stability/jni-lifecycle
- Found by: R1-CHUNK-0018
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/app/src/main/cpp/shared/android_jni_overlay.c:L13-L30` in `call_java_string_method`
- Related: `android/app/src/main/cpp/jni_main.c:L120-L148,L261-L281`, `android/app/src/main/cpp/shared/android_rewind.c:L74-L81`, `android/app/src/main/cpp/shared/track_names.c:L328-L406`, and `android/app/src/main/java/com/dxxredux/app/MainActivity.kt:L3663-L3696`
- Evidence: Every overlay callback obtains an activity class local reference but never deletes it. During normal play these calls run on the thread already executing the long-lived `MainActivity.startGame` native method, so the references belong to one JNI local frame and are not reclaimed until the game exits. Track, level, and rewind notifications therefore grow that frame for the life of the session and can eventually overflow ART's local-reference table. The same helper marks itself attached without checking whether `AttachCurrentThread` succeeded, dereferences `env` and allocation results unconditionally, and neither detects nor clears an exception from method lookup, string conversion, or the Java callback, leaving later JNI work to run with a pending exception
- Trigger: Generate enough track, level, or rewind overlay notifications in one game process to exhaust local-reference capacity, or encounter an attachment, allocation, modified-UTF-8 conversion, method lookup, or Java callback failure
- Impact: A long-running game can abort from local-reference-table overflow; rarer callback failures can dereference an invalid JNI environment or poison subsequent native-to-Java calls instead of degrading only the optional overlay
- Expected: Each callback releases every local reference on every path, validates attachment and JNI results, and handles callback exceptions according to an explicit nonfatal overlay policy
- Suggested fix: Check the exact `GetEnv` result and the return value from `AttachCurrentThread`; reject null text or define its conversion; check class, method, and string creation; delete both `jstr` and `cls` through one cleanup path; detect, log, and clear Java exceptions when the overlay is intentionally best-effort; and detach only after a successful attachment. A safely cached global class and method ID is also reasonable if lifecycle ownership remains explicit
- Validation: Under CheckJNI, stress thousands of track, level, and rewind callbacks during one `startGame` invocation and assert local-reference usage stays bounded; inject attachment, string-allocation, lookup, and Java-callback failures and verify no crash, pending exception, leaked reference, or incorrect detach
- Resolution: Pending

### BR-0045: P2 - Validate legacy save headers before offering Resume

- [ ] OPEN
- Type: defect
- Confidence: high
- Category: correctness/input-validation
- Found by: R1-CHUNK-0018
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/app/src/main/cpp/jni_resume_save.cpp:L646-L652` in `save_explorer_slot_json`
- Related: `android/app/src/main/cpp/jni_resume_save.cpp:L156-L207,L378-L399,L611-L719`, `android/app/src/main/java/com/dxxredux/app/SaveExplorerBridge.kt:L40-L65,L108-L177`, `android/app/src/main/java/com/dxxredux/app/SetupSaveExplorer.kt:L793-L823,L882-L911`, and `android/app/src/test/java/com/dxxredux/app/SaveExplorerTest.kt:L154-L169`
- Evidence: For a metadata-less single-player file under a recognized D1 or D2 save tree, the explorer sets `loadable = true` solely from the scope, game path, and a non-sentinel pilot name. It has already run `read_save_header_preview`, which checks the `DGSS` magic, compatible version, required header reads, and mission field, but does not require that check to succeed. Consequently a zero-length, truncated, wrong-magic, or arbitrary file named like `pilot.sg0` is serialized as loadable with fabricated fallback fields. Kotlin's `toResumeCandidate` trusts the native `loadable` flag and exposes it as a resumable candidate; the focused tests construct trusted slot DTOs and do not cover native malformed-file classification
- Trigger: Leave or import a malformed or unrelated metadata-less file with a save-slot suffix such as `.sg0` beneath a recognized single-player save-set path and open Save Explorer
- Impact: The launcher offers Resume for a file it has not established is a compatible save, producing a misleading candidate and deferring rejection or unsafe malformed-input handling to the game loader
- Expected: A metadata-less legacy candidate is loadable only after the native save parser proves at least the complete compatibility header and required structure are valid for the selected game
- Suggested fix: Require `have_preview` for the legacy fallback at minimum, and preferably call one shared engine-owned validation routine that checks the complete load preconditions rather than maintaining a weaker launcher parser. Preserve invalid files as explicit orphan entries with a precise reason instead of synthesizing resume metadata
- Validation: Add native-to-Kotlin explorer fixtures for valid legacy D1 and D2 saves and for empty, truncated, wrong-magic, old-version, missing-mission, cross-game, and random-byte `.sgN` files; assert only valid fixtures become resume candidates and invalid files remain inspectable but not loadable
- Resolution: Pending

### BR-0046: P2 - Reject incomplete CD reads instead of publishing partial hashes

- [ ] OPEN
- Type: defect
- Confidence: high
- Category: correctness/data-integrity
- Found by: R1-CHUNK-0019
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/app/src/main/cpp/extract/extract_cd.c:L231-L248,L498-L515`
- Related: `android/app/src/main/cpp/extract/extract_cd.c:L455-L472`, `android/app/src/main/cpp/extract/fingerprint_cd.c:L228-L245,L320-L375`, `android/app/src/main/cpp/extract/cue_parser.c:L135-L164`, `game_data/extract_all_cds.ps1:L119-L153`, and `game_data/hash_disc_tracks.ps1:L76-L95`
- Evidence: The CUE reader ignores both seeks, accepts a negative `ftell`, and ignores `fread`. If length discovery fails, `len` can be `-1`, making the allocation size zero, the requested read size enormous after unsigned conversion, and `buf[len]` a write before the allocation. A short read of a regular file instead leaves uninitialized bytes for the parser to consume. After parsing, per-track hashing ignores the seek result; when any sector read is short or fails, it only prints a warning, finalizes the SHA-1 over the prefix, emits that digest as an ordinary successful track record, and leaves `errors` unchanged. The parallel fingerprint CLI duplicates both patterns. The extraction script saves all emitted records as `track_hashes.json`, and the database generator promotes any track with a number, type, and SHA-1 without knowing whether the declared sector range was fully read
- Trigger: Pass a non-seekable CUE stream, encounter a CUE short read, truncate or replace a BIN after its size pass, inject a sector read error, or make the computed track seek fail
- Impact: The host tool can corrupt memory while reading the CUE or publish a stable-looking SHA-1 for an empty or partial track and exit successfully; that digest can enter checked-in track manifests and the canonical known-disc database, causing false or irreproducible disc identities
- Expected: CUE loading succeeds only after an exact bounded regular-file read, and a track hash is emitted only after a validated seek and every declared sector has been read exactly once
- Suggested fix: Replace the duplicated seek-and-`ftell` readers with one checked bounded file loader that rejects non-seekable, oversized, changed, or short inputs; validate every track has a positive in-file byte range; check `lseek`; and convert any short sector read into a nonzero command result without emitting a success SHA-1. Share the exact hashing helper between extraction and fingerprint tools
- Validation: Add seek-failure, negative-length, CUE short-read, BIN truncation-between-stat-and-read, mid-track read-error, zero-sector, and complete valid-track cases for both CLIs; assert failures emit no promotable digest, exit nonzero without memory errors under sanitizers, and leave canonical manifests unchanged
- Resolution: Pending

### BR-0047: P2 - Report successful HFS extraction as success

- [ ] OPEN
- Type: defect
- Confidence: high
- Category: api-data-format
- Found by: R1-CHUNK-0019
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/app/src/main/cpp/extract/extract_cd.c:L558-L586`
- Related: `game_data/extract_all_cds.ps1:L129-L153`, `game_data/hash_disc_tracks.ps1:L76-L95`, `game_data/CD images/Descent - Mac macplay/track_hashes.json:L2`, `game_data/CD images/d1 mac 2nd bin+cue/track_hashes.json:L2`, `game_data/CD images/d2 mac/track_hashes.json:L2`, and `android/ai tool plans/CD, installer parsing/on_device_mac_extract.md:L53,L94,L160,L470`
- Evidence: After detecting HFS and extracting a positive file count, the tool increments both success counters and exits successfully but deliberately emits `"error": "ISO listing failed"` to avoid changing old fixtures. All three checked-in Mac track manifests therefore describe their successfully extracted data track as an error. Repository planning claims both that HFS is reported instead of a false ISO error and, elsewhere, that the false error is correct. Current promotion scripts silently discard the error while accepting the SHA-1, so the field is neither a truthful status nor an enforced compatibility contract
- Trigger: Successfully process any supported HFS BIN/CUE image through `extract_cd` and save its JSON lines with `extract_all_cds.ps1`
- Impact: Machine-readable output contradicts the actual result and exit status, checked-in provenance permanently claims a failure, and any caller that correctly treats an `error` field as failure will reject supported Mac extraction while existing callers hide genuine status mistakes by ignoring it
- Expected: A successful HFS record contains no error and explicitly identifies the filesystem and extracted count; an error record is emitted only when HFS detection or extraction actually fails
- Suggested fix: Restore a truthful success schema such as `filesystem: "hfs"` plus `files_extracted` and optional partition metadata, update the three generated manifests and contradictory planning text in the publication series, and make the batch script validate that successful records lack `error` while failed records cause a nonzero run
- Validation: Run the CLI against the three known Mac discs, parse stdout strictly, assert the HFS data record has the exact SHA-1, filesystem, and positive extracted count with no error, verify failed ISO and HFS fixtures still emit errors and exit nonzero, and regenerate consumers without changing disc identity
- Resolution: Pending

## Disposition log

Append a dated entry whenever a finding becomes fixed, dismissed, deferred, or a duplicate. Include evidence and the deciding person or call

## Campaign closure

- [ ] Every queue item is `DONE` or has an approved `SKIP` disposition
- [ ] No queue item remains `ACTIVE` or `BLOCKED`
- [ ] Every P0 and P1 finding received an independent verification call
- [ ] Every finding has a final disposition or an explicitly accepted deferral owner
- [ ] Required build, lint, unit, integration, emulator, and server validations are recorded
- [ ] `git diff c01d8fe4686c63d931b1e543a6305bbafaa944a9..HEAD` was reviewed through a delta campaign or proven empty
- [ ] A human maintainer reviewed open risks and AI-generated dispositions
- [ ] The closing summary records counts by severity, category, and disposition
