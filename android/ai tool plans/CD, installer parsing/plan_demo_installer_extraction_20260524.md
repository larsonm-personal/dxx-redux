# Demo Installer Extraction Plan

## Goal
Make demo installer archives a first-class import source for Android and PC-side tooling. The Android launcher should accept known Descent 1 and Descent 2 demo ZIP or EXE packages, extract the contained game data files, record hashes in the existing asset manifest, and identify demo file versions through the normal hash/version display path.

## Phases
- [x] Inventory `game_data/demo installers` payloads and document installer contents
- [x] Locate existing PC-side extraction helpers and reuse or align with them where practical
- [x] Define a shared demo package model for known filenames, package hashes, output files, and game identity
- [x] Add Android extraction support for demo ZIP and EXE packages using existing archive readers where possible
- [x] Add hash/version recognition for extracted demo game data files
- [x] Connect demo package import to the Setup UI without depending on the broken download URL
- [x] Add focused unit or integration coverage for package detection and ZIP extraction
- [x] Run formatting and relevant tests, then mark completed work in this plan

## Follow-up: Mac StuffIt Demo Installers
- [x] Inventory `Descent II Preview.sit` and `Descent Shareware.sit` contents and extracted hashes
- [x] Add known package entries for the Mac StuffIt installers and route `.sit` imports through StuffIt extraction
- [x] Update docs and tests for the new package hashes and extraction modes
- [x] Run formatting and focused validation for the updated import paths

## Follow-up: StuffIt Attribution, Hashes, and Oracles
- [x] Label `stuffit_extract.c` with the main open source references used for the SIT5 parser
- [x] Audit README extracted-file hash rows against `known_versions.json5` and document any shared-version hashes
- [x] Add repeatable oracle validation for the two real Mac `.sit` installers, using PC-side extraction hashes as the expected output
- [x] Plan the PC helper transition from external StuffIt tools to the in-tree extractor
- [x] Run formatting and focused native/helper validation

## Follow-up: Demo Runtime Issues
- [x] Investigate D2 DOS demo crash at briefing sound start in `mixdigi_convert_sound`
- [x] Add intelligible version recognition for the `desc14sw.exe` `descent.pig` hash
- [x] Fix D1 Mac demo mission detection so menus and level range use demo behavior
- [x] Fix D1 Mac demo missing briefing/level resource paths or fail safely with the right mission data
- [ ] Add focused validation for the runtime fixes

## Follow-up: Additional BinHex and Classic SIT Demo Packages
- [x] Hash and inventory the newly downloaded `.hqx` and classic `.sit` demo packages
- [x] Extract the new packages and record final game data hashes in the README
- [x] Extend Android import routing for BinHex-wrapped StuffIt packages
- [x] Extend native StuffIt extraction for the classic `SIT!` package variant
- [x] Add focused tests or oracle coverage for the new package IDs and extraction paths
- [x] Review the previous runtime bug fixes for correctness risks

## Notes
- Keep game-engine source changes out of this tranche unless needed for hash recognition
- Do not fix the existing demo download URL in this pass
- Prefer one extraction path for PC and Android semantics even if the host scripts and Android implementation use different archive backends
- Added `game_data/demo installers/README.md` with package hashes, contents, reference extracted files, and extraction flow
- Android now routes known demo ZIP and self-extracting EXE packages through ZIP/SOW extraction instead of the GOG installer dialog, using package hashes when filenames are renamed
- Nested demo `.sow` chunks are extracted in sorted append mode so multi-part installer files like D2 demo HAM/PIG reconstruct to the DOSBox reference hashes
- Existing hash recognition in `known_versions.json5` already covers D1 Demo v1.4, D1 Demo (Mac), D2 Demo v1.0, and `d2demo.dem`; D2 demo setup now tracks `d2demo.dem` as optional
- Validation completed with focused JVM tests and scoped `run-code-quality.ps1 -Fix` on touched Kotlin files. A full unscoped quality pass still reports unrelated pre-existing PowerShell formatting in `android/tools/decode_object_packets.ps1`
- Follow-up remains for actual game-engine runtime issues when launching/playing from demo data
- Mac StuffIt support now covers direct D1 game data and the nested D2 STi installer path, including StuffIt methods 14 and 15 in the native extractor
- Mac helper validation and native `test_stuffit_direct` both reproduce the documented D1 and D2 Mac demo output hashes
- The existing StuffIt corpus test was updated to treat methods 14 and 15 as supported and passes against the committed manifests
- `stuffit_extract.c` is labeled as derived primarily from XADMaster's `XADStuffIt5Parser`, with corpus/oracle cross-check references
- `mac_stuffit_oracles.json` is produced by `extract_mac_demos.ps1 -WriteOracle`; `stuffit_demo_oracle_tests` requires the in-tree extractor to match those sizes and SHA-256 hashes
- `hash_assets.ps1` now scans `game_data/demo installers/*_extracted` and can reproduce the D2 Mac demo `descent2.s11` version alias from the shared D2 v1.2 hash
- Initial crash evidence: D2 DOS demo tombstone enters `mixdigi_convert_sound` from `new_briefing_screen`; D1 Mac demo crash files report missing `missions/level01.rdl` and missing `pluto01.pcx`
- Host validation in this tranche: `run-windows-build.ps1 -Target d1`, `run-windows-build.ps1 -Target d2`, and extractor `stuffit_corpus_tests` + `stuffit_demo_oracle_tests` pass in `temp/extract_build`
- Added support for `descent2preview.sit`, `descent2preview.sit_.hqx`, `Descent_demo.HQX`, and `descent_demo.sit_.hqx`; BinHex wrappers are decoded to their data fork before native StuffIt extraction
- Native StuffIt extraction now accepts classic `SIT!` archives through the existing StuffIt-family header scanner, while SIT5 support remains unchanged
- The new D2 preview classic SIT and its BinHex wrapper extract to the same D2 Demo (Mac) game data hashes as the existing Mac preview support; the two D1 BinHex downloads extract to the two already-recognized D1 Demo (Mac) HOG variants
- Focused validation in this tranche: `:app:testDebugUnitTest --tests com.dxxredux.app.BinHexDecoderTest --tests com.dxxredux.app.DemoInstallerPackagesTest`, `stuffit_corpus_tests`, `stuffit_demo_oracle_tests`, `sti2_tests`, `run-windows-build.ps1 -Target d1`, and `run-windows-build.ps1 -Target d2`
- Review of the previous runtime fixes found one follow-up guard issue: invalid or oversized audio samples could be rejected by conversion but still handed to SDL_mixer as an empty chunk. D1 and D2 now return before `Mix_PlayChannel` if conversion did not produce a valid chunk. Remaining risk is runtime behavior on device for the D1 Mac demo and D2 DOS demo paths, so the focused runtime-validation item remains open
