# Extract Code Reorganization Plan

## Goal
1. Move extraction code into `android/app/src/main/cpp/extract/` subfolder
2. Remove .obj files from git history (if any exist)
3. Create CMake-based centralized test build system
4. Move useful test files from temp/ to proper test directory

## Files to Move → `extract/`

Source files (currently in `android/app/src/main/cpp/`):
- `cue_parser.c` → `extract/cue_parser.c`
- `cue_parser.h` → `extract/cue_parser.h`
- `iso9660_reader.c` → `extract/iso9660_reader.c`
- `iso9660_reader.h` → `extract/iso9660_reader.h`
- `sow_extract.c` → `extract/sow_extract.c`
- `sow_extract.h` → `extract/sow_extract.h`
- `extract_cd.c` → `extract/extract_cd.c`
- `jni_disc_import.c` → `extract/jni_disc_import.c`
- `test_cue_iso.c` → `extract/test_cue_iso.c`

## Include Path Updates
- All `#include "cue_parser.h"` etc. use bare names → keep working via `-I extract/`
- CMakeLists.txt: update source paths from `${CMAKE_CURRENT_SOURCE_DIR}/X.c` to `${CMAKE_CURRENT_SOURCE_DIR}/extract/X.c`
- CMakeLists.txt: add `extract/` to include dirs

## Test Infrastructure
- Create `android/app/src/main/cpp/extract/CMakeLists.txt` that:
  - Defines extract library sources (for inclusion by parent)
  - When built standalone (TEST_STANDALONE), builds test executables
- Create `android/tests/CMakeLists.txt` — top-level test build
  - Builds: test_cue_iso, extract_cd, test_sow_direct
  - Output dir: `android/tests/build/` (gitignored)
- Move test_sow_direct.c from temp/ → `android/app/src/main/cpp/extract/test_sow_direct.c`
- Drop ref_arj_test.c and test_sow_partial.c (debugging artifacts, not regression tests)

## Script Updates
- `android/run_cue_iso_tests.ps1` → use cmake build
- `android/run_cue_iso_tests.sh` → use cmake build
- `game_data/extract_all_cds.ps1` → use cmake build
- Remove embedded build instructions from .c file comments

## .obj Cleanup
- No .obj files found in git history — just ensure .gitignore covers them
