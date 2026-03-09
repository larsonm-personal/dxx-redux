# CMakeLists.txt — Desktop test builds for CD/SOW/GOG extraction code.
#
# Usage:
#   cmake -S android/app/src/main/cpp/extract -B android/tests/build
#   cmake --build android/tests/build
#
# Produces: test_cue_iso, extract_cd, test_sow_direct, extract_gog
# Output location: android/tests/build/

cmake_minimum_required(VERSION 3.16)
project(dxx-extract-tests C)

include(FetchContent)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# ── LZMA SDK (for InnoSetup decompression) ───────────────────────────
FetchContent_Declare(
    lzma_sdk
    GIT_REPOSITORY https://github.com/jljusten/LZMA-SDK.git
    GIT_TAG        781863cdf592da3e97420f50de5dac056ad352a5  # 18.05
    GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(lzma_sdk)
if(NOT lzma_sdk_POPULATED)
    FetchContent_Populate(lzma_sdk)
endif()

# Build the LZMA SDK C files we need as a static library
add_library(lzma_sdk_c STATIC
    ${lzma_sdk_SOURCE_DIR}/C/LzmaDec.c
    ${lzma_sdk_SOURCE_DIR}/C/Lzma2Dec.c
    ${lzma_sdk_SOURCE_DIR}/C/7zCrc.c
    ${lzma_sdk_SOURCE_DIR}/C/7zCrcOpt.c
)
target_include_directories(lzma_sdk_c PUBLIC ${lzma_sdk_SOURCE_DIR}/C)
target_compile_definitions(lzma_sdk_c PRIVATE _7ZIP_ST)  # single-threaded

# ── zlib (for InnoSetup decompression) ────────────────────────────────
FetchContent_Declare(
    zlib_dep
    GIT_REPOSITORY https://github.com/madler/zlib.git
    GIT_TAG        v1.3.1
    GIT_SHALLOW    TRUE
)
set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(zlib_dep)

# Create ZLIB::ZLIB alias matching find_package(ZLIB) conventions
if(NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB ALIAS zlibstatic)
    # Make zlib headers available
    target_include_directories(zlibstatic PUBLIC
        ${zlib_dep_SOURCE_DIR} ${zlib_dep_BINARY_DIR})
endif()

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

# ── extract_gog ─────────────────────────────────────────────────────
add_executable(extract_gog
    ${CMAKE_CURRENT_SOURCE_DIR}/extract_gog.c
    ${CMAKE_CURRENT_SOURCE_DIR}/inno_reader.c
)
target_include_directories(extract_gog PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(extract_gog PRIVATE lzma_sdk_c ZLIB::ZLIB)
