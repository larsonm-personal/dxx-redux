# CMakeLists.txt — Desktop test builds for CD/SOW extraction code.
#
# Usage:
#   cmake -S android/app/src/main/cpp/extract -B android/tests/build
#   cmake --build android/tests/build
#
# Produces: test_cue_iso, extract_cd, test_sow_direct
# Output location: android/tests/build/

cmake_minimum_required(VERSION 3.16)
project(dxx-extract-tests C)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# ── Library sources (shared by all test executables) ─────────────────
set(EXTRACT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/cue_parser.c
    ${CMAKE_CURRENT_SOURCE_DIR}/iso9660_reader.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sow_extract.c
)

# ── test_cue_iso ─────────────────────────────────────────────────────
add_executable(test_cue_iso
    ${CMAKE_CURRENT_SOURCE_DIR}/test_cue_iso.c
    ${CMAKE_CURRENT_SOURCE_DIR}/cue_parser.c
    ${CMAKE_CURRENT_SOURCE_DIR}/iso9660_reader.c
)
target_include_directories(test_cue_iso PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# ── extract_cd ───────────────────────────────────────────────────────
add_executable(extract_cd
    ${CMAKE_CURRENT_SOURCE_DIR}/extract_cd.c
    ${EXTRACT_SOURCES}
)
target_include_directories(extract_cd PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# ── test_sow_direct ─────────────────────────────────────────────────
add_executable(test_sow_direct
    ${CMAKE_CURRENT_SOURCE_DIR}/test_sow_direct.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sow_extract.c
)
target_include_directories(test_sow_direct PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# ── CTest integration ───────────────────────────────────────────────
enable_testing()
add_test(NAME cue_iso_tests COMMAND test_cue_iso)
set_tests_properties(cue_iso_tests PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
