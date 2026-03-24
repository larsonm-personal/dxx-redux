# CMakeLists.txt — Desktop test builds for CD/SOW/GOG extraction code.
#
# Usage:
#   cmake -S android/app/src/main/cpp/extract -B android/tests/build
#   cmake --build android/tests/build
#
# Produces: test_cue_iso, extract_cd, test_sow_direct, extract_gog
# Output location: android/tests/build/

cmake_minimum_required(VERSION 3.16)
project(dxx-extract-tests C CXX)

include(FetchContent)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# On MSVC, prevent windows.h min/max macros and enable M_PI
if(MSVC)
    add_compile_definitions(NOMINMAX _USE_MATH_DEFINES)
endif()

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
    ${CMAKE_CURRENT_SOURCE_DIR}/pkg_reader.c
)
target_include_directories(extract_gog PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(extract_gog PRIVATE lzma_sdk_c ZLIB::ZLIB)

# ── Chromaprint (audio fingerprinting, uses bundled KissFFT) ─────────
FetchContent_Declare(chromaprint
    URL https://github.com/acoustid/chromaprint/archive/refs/tags/v1.5.1.tar.gz
)
FetchContent_GetProperties(chromaprint)
if(NOT chromaprint_POPULATED)
    FetchContent_Populate(chromaprint)
endif()

set(_CP_SRC ${chromaprint_SOURCE_DIR}/src)
add_library(chromaprint STATIC
    ${_CP_SRC}/chromaprint.cpp
    ${_CP_SRC}/audio_processor.cpp
    ${_CP_SRC}/chroma.cpp
    ${_CP_SRC}/chroma_filter.cpp
    ${_CP_SRC}/chroma_resampler.cpp
    ${_CP_SRC}/fft.cpp
    ${_CP_SRC}/fft_lib_kissfft.cpp
    ${_CP_SRC}/fingerprint_calculator.cpp
    ${_CP_SRC}/fingerprint_compressor.cpp
    ${_CP_SRC}/fingerprint_decompressor.cpp
    ${_CP_SRC}/fingerprint_matcher.cpp
    ${_CP_SRC}/fingerprinter.cpp
    ${_CP_SRC}/fingerprinter_configuration.cpp
    ${_CP_SRC}/image_builder.cpp
    ${_CP_SRC}/silence_remover.cpp
    ${_CP_SRC}/simhash.cpp
    ${_CP_SRC}/spectrum.cpp
    ${_CP_SRC}/utils/base64.cpp
    ${chromaprint_SOURCE_DIR}/src/3rdparty/kissfft/kiss_fft.c
    ${chromaprint_SOURCE_DIR}/src/3rdparty/kissfft/tools/kiss_fftr.c
    ${chromaprint_SOURCE_DIR}/src/avresample/resample2.c
)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/chromaprint_config/config.h"
    "#ifndef CHROMAPRINT_CONFIG_H\n"
    "#define CHROMAPRINT_CONFIG_H\n"
    "#define HAVE_ROUND 1\n"
    "#define USE_KISSFFT 1\n"
    "#endif\n"
)
target_include_directories(chromaprint
    PUBLIC  ${chromaprint_SOURCE_DIR}/src
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/chromaprint_config
            ${chromaprint_SOURCE_DIR}/src/3rdparty/kissfft
            ${chromaprint_SOURCE_DIR}/src/3rdparty/kissfft/tools
)
target_compile_definitions(chromaprint PRIVATE HAVE_CONFIG_H HAVE_LRINTF)
if(MSVC)
    target_compile_options(chromaprint PRIVATE /w)
else()
    target_compile_options(chromaprint PRIVATE -w)
endif()

# ── Single-file PCM decoders (minimp3, stb_vorbis, dr_flac) ─────────
set(_DECODERS_DIR "${CMAKE_CURRENT_BINARY_DIR}/pcm_decoders")
file(MAKE_DIRECTORY "${_DECODERS_DIR}")

if(NOT EXISTS "${_DECODERS_DIR}/minimp3.h")
    file(DOWNLOAD
        "https://raw.githubusercontent.com/lieff/minimp3/afb604c06bc8beb145fecd42c0ceb5bda8795144/minimp3.h"
        "${_DECODERS_DIR}/minimp3.h")
endif()
if(NOT EXISTS "${_DECODERS_DIR}/minimp3_ex.h")
    file(DOWNLOAD
        "https://raw.githubusercontent.com/lieff/minimp3/afb604c06bc8beb145fecd42c0ceb5bda8795144/minimp3_ex.h"
        "${_DECODERS_DIR}/minimp3_ex.h")
endif()
set(_stb_vorbis_need_dl TRUE)
if(EXISTS "${_DECODERS_DIR}/stb_vorbis.c")
    file(SIZE "${_DECODERS_DIR}/stb_vorbis.c" _stb_vorbis_size)
    if(_stb_vorbis_size GREATER 0)
        set(_stb_vorbis_need_dl FALSE)
    endif()
endif()
if(_stb_vorbis_need_dl)
    file(DOWNLOAD
        "https://raw.githubusercontent.com/nothings/stb/1ee679ca2ef753a528db5ba6801e1067b40481b8/stb_vorbis.c"
        "${_DECODERS_DIR}/stb_vorbis.c"
        STATUS _stb_dl_status)
    list(GET _stb_dl_status 0 _stb_dl_code)
    if(NOT _stb_dl_code EQUAL 0)
        message(FATAL_ERROR "Failed to download stb_vorbis.c: ${_stb_dl_status}")
    endif()
endif()
if(NOT EXISTS "${_DECODERS_DIR}/dr_flac.h")
    file(DOWNLOAD
        "https://raw.githubusercontent.com/mackron/dr_libs/da35f9d6c7374a95353fd1df1d394d44ab66cf01/dr_flac.h"
        "${_DECODERS_DIR}/dr_flac.h")
endif()

set(SHARED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../shared")

# ── fingerprint_cd ───────────────────────────────────────────────────
add_executable(fingerprint_cd
    ${CMAKE_CURRENT_SOURCE_DIR}/fingerprint_cd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/cue_parser.c
    ${SHARED_DIR}/pcm_decoders.c
    ${SHARED_DIR}/fingerprint_gen.c
    ${_DECODERS_DIR}/stb_vorbis.c
)
target_include_directories(fingerprint_cd PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SHARED_DIR}
    ${_DECODERS_DIR}
)
target_compile_definitions(fingerprint_cd PRIVATE CHROMAPRINT_NODLL)
target_link_libraries(fingerprint_cd PRIVATE chromaprint)
if(MSVC)
    target_compile_options(fingerprint_cd PRIVATE /W3)
else()
    target_compile_options(fingerprint_cd PRIVATE -Wall)
endif()

# ── fingerprint_audio (loose MP3/OGG/FLAC files) ────────────────────
add_executable(fingerprint_audio
    ${CMAKE_CURRENT_SOURCE_DIR}/fingerprint_audio.c
    ${SHARED_DIR}/pcm_decoders.c
    ${SHARED_DIR}/fingerprint_gen.c
    ${_DECODERS_DIR}/stb_vorbis.c
)
target_include_directories(fingerprint_audio PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SHARED_DIR}
    ${_DECODERS_DIR}
)
target_compile_definitions(fingerprint_audio PRIVATE CHROMAPRINT_NODLL)
target_link_libraries(fingerprint_audio PRIVATE chromaprint)
if(MSVC)
    target_compile_options(fingerprint_audio PRIVATE /W3)
else()
    target_compile_options(fingerprint_audio PRIVATE -Wall)
endif()

# ── fingerprint_match (find duplicate fingerprints in a JSON DB) ─────
add_executable(fingerprint_match
    ${CMAKE_CURRENT_SOURCE_DIR}/fingerprint_match.c
)
target_include_directories(fingerprint_match PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_definitions(fingerprint_match PRIVATE CHROMAPRINT_NODLL)
target_link_libraries(fingerprint_match PRIVATE chromaprint)
if(MSVC)
    target_compile_options(fingerprint_match PRIVATE /W3)
else()
    target_compile_options(fingerprint_match PRIVATE -Wall)
endif()

# ── test_fingerprint (unit tests for fingerprint_gen) ────────────────
add_executable(test_fingerprint
    ${CMAKE_CURRENT_SOURCE_DIR}/test_fingerprint.c
    ${SHARED_DIR}/pcm_decoders.c
    ${SHARED_DIR}/fingerprint_gen.c
    ${_DECODERS_DIR}/stb_vorbis.c
)
target_include_directories(test_fingerprint PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SHARED_DIR}
    ${_DECODERS_DIR}
)
target_compile_definitions(test_fingerprint PRIVATE CHROMAPRINT_NODLL)
target_link_libraries(test_fingerprint PRIVATE chromaprint)
if(MSVC)
    target_compile_options(test_fingerprint PRIVATE /W3)
else()
    target_compile_options(test_fingerprint PRIVATE -Wall)
endif()

# Chromaprint test data lives in the fetched source tree
set(_CP_TEST_DATA "${chromaprint_SOURCE_DIR}/tests/data")
add_test(NAME fingerprint_tests
    COMMAND test_fingerprint "${_CP_TEST_DATA}")
set_tests_properties(fingerprint_tests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
