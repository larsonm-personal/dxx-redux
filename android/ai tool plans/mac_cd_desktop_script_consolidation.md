# Mac CD Desktop Extraction Consolidation

Status: Complete

## Goal

Clean up temp/prototype scripts created during Mac CD image investigation.
Consolidate into the existing extraction pipeline so extract_all_cds.ps1 handles
Mac discs automatically.

## Tasks

### 1. Delete superseded temp scripts
- [x] temp/extract_data_track.ps1 -- Mode1 sector stripping (inlined in extract_mac_cd.ps1 Stage 2)
- [x] temp/parse_apm.ps1 -- APM parsing (inlined in Stage 3)
- [x] temp/extract_mac_hfs.py -- HFS extraction (inlined in Stage 5)
- [x] temp/mac_extract_test.txt -- test output log
- [x] temp/mac_extract_test2.txt -- test output log
- [x] temp/scan_sit_output2.txt -- STi2 analysis artifact
- [x] temp/sti2_hexdump.txt -- format analysis artifact
- [x] temp/hash_assets_output.txt -- hash run output log
- [x] temp/install_descent.sit -- extracted STi2 file (can re-extract from CD)
- [x] temp/mac_extracted/ -- leftover partial extraction directory

### 2. Integrate Mac fallback into extract_all_cds.ps1
- [x] After extract_cd.exe fails for a folder, check if the JSON output contains
      "ISO listing failed" (changed from DDR signature check to JSON-based detection)
- [x] If Mac detected, call extract_mac_cd.ps1 -CdFolder <path>
- [x] Count Mac successes alongside normal successes in the report

### 3. Verify
- [x] Run extract_all_cds.ps1 -SkipBuild on Mac macplay (29 PC skipped, Mac extracted OK)
- [x] Confirm data_tracks/ created with 8 game files
- [x] hash_assets.ps1 verification deferred (187 entries confirmed in prior session)

## Files

- game_data/extract_mac_cd.ps1 -- standalone Mac pipeline (exists, tested)
- game_data/extract_all_cds.ps1 -- batch extractor, add Mac fallback
- android/get_deps/get_unar.sh -- unar install script (exists)
- android/get_deps/get_stuffit.sh -- stuffit install script (exists, but
  stuffit CLI can't handle STi2 -- kept for potential SIT 1.x use)
