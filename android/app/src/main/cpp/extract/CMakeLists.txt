# CMakeLists.txt — Desktop test builds for CD/SOW/GOG extraction code.
#
# Usage: cmake -S android/app/src/main/cpp/extract -B android/tests/build cmake --build
# android/tests/build
#
# Produces: test_cue_iso, extract_cd, test_sow_direct, test_hfs, test_sti2, test_stuffit_corpus,
# extract_gog Output location: android/tests/build/

cmake_minimum_required(VERSION 3.16)
project(dxx-extract-tests C CXX)

include(${CMAKE_CURRENT_LIST_DIR}/../../../../../../cmake/dxx-verified-dependencies.cmake)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set(DXX_DEPENDENCY_MANIFEST
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../get_deps/tool_versions.conf")

# On MSVC, prevent windows.h min/max macros and enable M_PI
if(MSVC)
    add_compile_definitions(NOMINMAX _USE_MATH_DEFINES)
endif()

# ── LZMA SDK (for InnoSetup decompression) ───────────────────────────
dxx_verified_fetchcontent_declare(lzma_sdk LZMA_SDK)
FetchContent_GetProperties(lzma_sdk)
if(NOT lzma_sdk_POPULATED)
    FetchContent_Populate(lzma_sdk)
endif()

# Build the LZMA SDK C files we need as a static library
add_library(
    lzma_sdk_c STATIC ${lzma_sdk_SOURCE_DIR}/C/LzmaDec.c ${lzma_sdk_SOURCE_DIR}/C/Lzma2Dec.c
                      ${lzma_sdk_SOURCE_DIR}/C/7zCrc.c ${lzma_sdk_SOURCE_DIR}/C/7zCrcOpt.c)
target_include_directories(lzma_sdk_c PUBLIC ${lzma_sdk_SOURCE_DIR}/C)
target_compile_definitions(lzma_sdk_c PRIVATE _7ZIP_ST) # single-threaded

# ── zlib (for InnoSetup decompression) ────────────────────────────────
dxx_verified_fetchcontent_declare(zlib_dep ZLIB)
set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(zlib_dep)

# Create ZLIB::ZLIB alias matching find_package(ZLIB) conventions
if(NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB ALIAS zlibstatic)
    # Make zlib headers available
    target_include_directories(zlibstatic PUBLIC ${zlib_dep_SOURCE_DIR} ${zlib_dep_BINARY_DIR})
endif()

# ── nlohmann/json (header-only for manifest-driven desktop tests) ─────
if(NOT TARGET nlohmann_json::nlohmann_json)
    dxx_verified_fetchcontent_declare(nlohmann_json NLOHMANN_JSON)
    set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
    set(JSON_Install OFF CACHE BOOL "" FORCE)
    set(JSON_MultipleHeaders OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# The rewind wrapper test supplies controlled PHYSFS function implementations, but it must compile
# against the same public declarations as the application.
dxx_verified_fetchcontent_declare(physfs_test_headers PHYSFS)
FetchContent_GetProperties(physfs_test_headers)
if(NOT physfs_test_headers_POPULATED)
    FetchContent_Populate(physfs_test_headers)
endif()

# ── StuffIt corpus (desktop regression only) ─────────────────────────
dxx_verified_fetchcontent_declare(stuffit_test_files STUFFIT_TEST_FILES)
FetchContent_GetProperties(stuffit_test_files)
if(NOT stuffit_test_files_POPULATED)
    FetchContent_Populate(stuffit_test_files)
endif()
file(TO_CMAKE_PATH "${stuffit_test_files_SOURCE_DIR}" STUFFIT_TEST_FILES_DIR)

# ── Library sources (shared by all test executables) ─────────────────
set(EXTRACT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/cd_read_contract.c
    ${CMAKE_CURRENT_SOURCE_DIR}/cue_parser.c
    ${CMAKE_CURRENT_SOURCE_DIR}/extract_attempt_budget.c
    ${CMAKE_CURRENT_SOURCE_DIR}/game_file_extensions.c
    ${CMAKE_CURRENT_SOURCE_DIR}/hfs_reader.c
    ${CMAKE_CURRENT_SOURCE_DIR}/iso9660_reader.c
    ${CMAKE_CURRENT_SOURCE_DIR}/physical_output_file.c
    ${CMAKE_CURRENT_SOURCE_DIR}/json_writer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/mac_hfs_extract.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sow_extract.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stuffit_extract.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sti2_extract.c)

# ── test_cue_iso ─────────────────────────────────────────────────────
add_executable(
    test_cue_iso
    ${CMAKE_CURRENT_SOURCE_DIR}/test_cue_iso.c ${CMAKE_CURRENT_SOURCE_DIR}/cue_parser.c
    ${CMAKE_CURRENT_SOURCE_DIR}/extract_attempt_budget.c
    ${CMAKE_CURRENT_SOURCE_DIR}/iso9660_reader.c ${CMAKE_CURRENT_SOURCE_DIR}/physical_output_file.c)
target_include_directories(test_cue_iso PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(test_cue_iso PRIVATE ISO9660_READER_TESTING)
if(MSVC)
    target_link_options(test_cue_iso PRIVATE /STACK:16777216)
endif()

# ── extract_cd ───────────────────────────────────────────────────────
add_executable(extract_cd ${CMAKE_CURRENT_SOURCE_DIR}/extract_cd.c ${EXTRACT_SOURCES})
target_include_directories(extract_cd PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# ── test_sow_direct ─────────────────────────────────────────────────
add_executable(
    test_sow_direct
    ${CMAKE_CURRENT_SOURCE_DIR}/test_sow_direct.c
    ${CMAKE_CURRENT_SOURCE_DIR}/extract_attempt_budget.c ${CMAKE_CURRENT_SOURCE_DIR}/sow_extract.c)
target_include_directories(test_sow_direct PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(
    test_sow_huffman
    ${CMAKE_CURRENT_SOURCE_DIR}/test_sow_huffman.c
    ${CMAKE_CURRENT_SOURCE_DIR}/extract_attempt_budget.c ${CMAKE_CURRENT_SOURCE_DIR}/sow_extract.c)
target_include_directories(test_sow_huffman PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(test_sow_huffman PRIVATE SOW_EXTRACT_TESTING)

add_executable(
    test_sow_integrity
    ${CMAKE_CURRENT_SOURCE_DIR}/test_sow_integrity.c
    ${CMAKE_CURRENT_SOURCE_DIR}/extract_attempt_budget.c ${CMAKE_CURRENT_SOURCE_DIR}/sow_extract.c)
target_include_directories(test_sow_integrity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# ── test_hfs ───────────────────────────────────────────────────────
add_executable(
    test_hfs ${CMAKE_CURRENT_SOURCE_DIR}/test_hfs.c ${CMAKE_CURRENT_SOURCE_DIR}/cue_parser.c
             ${CMAKE_CURRENT_SOURCE_DIR}/hfs_reader.c)
target_include_directories(test_hfs PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(test_hfs PRIVATE HFS_READER_TESTING)

# ── test_sti2 ──────────────────────────────────────────────────────
add_executable(
    test_sti2
    ${CMAKE_CURRENT_SOURCE_DIR}/test_sti2.c ${CMAKE_CURRENT_SOURCE_DIR}/cue_parser.c
    ${CMAKE_CURRENT_SOURCE_DIR}/extract_attempt_budget.c ${CMAKE_CURRENT_SOURCE_DIR}/hfs_reader.c
    ${CMAKE_CURRENT_SOURCE_DIR}/mac_hfs_extract.c ${CMAKE_CURRENT_SOURCE_DIR}/sti2_extract.c)
target_include_directories(test_sti2 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(test_sti2 PRIVATE STI2_EXTRACT_TESTING HFS_READER_TESTING)

# ── test_stuffit_corpus ────────────────────────────────────────────
add_executable(
    test_stuffit_corpus
    ${CMAKE_CURRENT_SOURCE_DIR}/test_stuffit_corpus.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/extract_attempt_budget.c ${CMAKE_CURRENT_SOURCE_DIR}/sti2_extract.c)
target_include_directories(test_stuffit_corpus PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(test_stuffit_corpus PRIVATE cxx_std_11)
target_compile_definitions(
    test_stuffit_corpus
    PRIVATE STUFFIT_TEST_FILES_DIR="${STUFFIT_TEST_FILES_DIR}"
            STUFFIT_MANIFEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test/data/stuffit_manifests")
target_link_libraries(test_stuffit_corpus PRIVATE nlohmann_json::nlohmann_json)

# ── CTest integration ───────────────────────────────────────────────
enable_testing()
add_executable(test_json_writer ${CMAKE_CURRENT_SOURCE_DIR}/test_json_writer.cpp
                                ${CMAKE_CURRENT_SOURCE_DIR}/json_writer.c)
target_include_directories(test_json_writer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_json_writer PRIVATE nlohmann_json::nlohmann_json)
add_test(NAME json_writer_tests COMMAND test_json_writer)
add_executable(test_extract_limits ${CMAKE_CURRENT_SOURCE_DIR}/test_extract_limits.c
                                   ${CMAKE_CURRENT_SOURCE_DIR}/extract_attempt_budget.c)
target_include_directories(test_extract_limits PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME extract_limit_tests COMMAND test_extract_limits)
add_executable(test_game_file_extensions ${CMAKE_CURRENT_SOURCE_DIR}/test_game_file_extensions.c
                                         ${CMAKE_CURRENT_SOURCE_DIR}/game_file_extensions.c)
target_include_directories(test_game_file_extensions PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME game_file_extension_tests COMMAND test_game_file_extensions)
add_executable(
    test_graphics_config_transaction
    ${CMAKE_CURRENT_SOURCE_DIR}/test_graphics_config_transaction.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/graphics_config_transaction.c)
target_include_directories(test_graphics_config_transaction
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../shared)
target_compile_definitions(test_graphics_config_transaction
                           PRIVATE GRAPHICS_CONFIG_TRANSACTION_TESTING)
add_test(NAME graphics_config_transaction_tests COMMAND test_graphics_config_transaction)
add_test(NAME sow_huffman_tests COMMAND test_sow_huffman)
add_test(NAME sow_integrity_tests COMMAND test_sow_integrity)
set(SOW_RETAIL_FIXTURE "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../game_data/DESCENT2.SOW")
set(SOW_SPLIT_FIXTURE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../game_data/CD images/Descent II (USA) (3-Level Interactive Preview)/data_tracks/sowstuff"
)
add_test(
    NAME sow_real_media_tests
    COMMAND
        ${CMAKE_COMMAND} -DTEST_SOW_DIRECT=$<TARGET_FILE:test_sow_direct>
        "-DRETAIL_SOW=${SOW_RETAIL_FIXTURE}" "-DSPLIT_SOW_DIR=${SOW_SPLIT_FIXTURE_DIR}"
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/sow_real_media_tests -P
        ${CMAKE_CURRENT_SOURCE_DIR}/test_sow_real_media.cmake)
set_tests_properties(sow_real_media_tests PROPERTIES SKIP_REGULAR_EXPRESSION
                                                     "SKIP: SOW real-media fixtures unavailable")
add_test(NAME cue_iso_tests COMMAND test_cue_iso)
set_tests_properties(cue_iso_tests PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME hfs_tests COMMAND test_hfs)
set_tests_properties(hfs_tests PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME sti2_tests COMMAND test_sti2)
set_tests_properties(sti2_tests PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME stuffit_corpus_tests COMMAND test_stuffit_corpus)
set_tests_properties(stuffit_corpus_tests PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(
    test_stuffit_malformed
    ${CMAKE_CURRENT_SOURCE_DIR}/test_stuffit_malformed.c
    ${CMAKE_CURRENT_SOURCE_DIR}/extract_attempt_budget.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stuffit_extract.c ${CMAKE_CURRENT_SOURCE_DIR}/sti2_extract.c)
target_include_directories(test_stuffit_malformed PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(test_stuffit_malformed PRIVATE STUFFIT_EXTRACT_TESTING)
add_test(NAME stuffit_malformed_tests COMMAND test_stuffit_malformed)
set_tests_properties(stuffit_malformed_tests PROPERTIES WORKING_DIRECTORY
                                                        ${CMAKE_CURRENT_BINARY_DIR})

add_executable(
    test_stuffit_direct
    ${CMAKE_CURRENT_SOURCE_DIR}/test_stuffit_direct.c
    ${CMAKE_CURRENT_SOURCE_DIR}/extract_attempt_budget.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stuffit_extract.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sti2_extract.c
    ${CMAKE_CURRENT_SOURCE_DIR}/game_file_extensions.c)
target_include_directories(test_stuffit_direct PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set(DEMO_INSTALLER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../game_data/demo installers")
set(MAC_STUFFIT_ORACLE_FILE "${DEMO_INSTALLER_DIR}/mac_stuffit_oracles.json")
add_test(
    NAME stuffit_demo_oracle_tests
    COMMAND
        ${CMAKE_COMMAND} -DTEST_STUFFIT_DIRECT=$<TARGET_FILE:test_stuffit_direct>
        "-DDEMO_INSTALLER_DIR=${DEMO_INSTALLER_DIR}" "-DORACLE_FILE=${MAC_STUFFIT_ORACLE_FILE}"
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/stuffit_demo_oracle_tests -P
        ${CMAKE_CURRENT_SOURCE_DIR}/test_stuffit_demo_oracles.cmake)

# ── extract_gog ─────────────────────────────────────────────────────
add_executable(
    extract_gog
    ${CMAKE_CURRENT_SOURCE_DIR}/extract_gog.c ${CMAKE_CURRENT_SOURCE_DIR}/game_file_extensions.c
    ${CMAKE_CURRENT_SOURCE_DIR}/json_writer.c ${CMAKE_CURRENT_SOURCE_DIR}/inno_reader.c
    ${CMAKE_CURRENT_SOURCE_DIR}/pkg_reader.c)
target_include_directories(extract_gog PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(extract_gog PRIVATE lzma_sdk_c ZLIB::ZLIB)

add_executable(
    test_gog_fd
    ${CMAKE_CURRENT_SOURCE_DIR}/test_gog_fd.c ${CMAKE_CURRENT_SOURCE_DIR}/game_file_extensions.c
    ${CMAKE_CURRENT_SOURCE_DIR}/inno_reader.c)
target_include_directories(test_gog_fd PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_gog_fd PRIVATE lzma_sdk_c ZLIB::ZLIB)
target_compile_definitions(test_gog_fd PRIVATE INNO_READER_TESTING=1)

add_executable(
    test_pkg_toc_bounds
    ${CMAKE_CURRENT_SOURCE_DIR}/test_pkg_toc_bounds.c
    ${CMAKE_CURRENT_SOURCE_DIR}/game_file_extensions.c ${CMAKE_CURRENT_SOURCE_DIR}/pkg_reader.c)
target_include_directories(test_pkg_toc_bounds PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_pkg_toc_bounds PRIVATE ZLIB::ZLIB)
target_compile_definitions(test_pkg_toc_bounds PRIVATE PKG_READER_TESTING=1)

set(GOG_D1_INSTALLER
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../game_data/gog installers/setup_descent_1.4a_(16596).exe"
)
set(GOG_D2_INSTALLER
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../game_data/gog installers/setup_descent_2_1.1_(16596).exe"
)
add_test(NAME gog_fd_tests COMMAND test_gog_fd "${GOG_D1_INSTALLER}" "${GOG_D2_INSTALLER}")
add_test(NAME pkg_toc_bound_tests COMMAND test_pkg_toc_bounds)

# ── Chromaprint (audio fingerprinting, uses bundled KissFFT) ─────────
dxx_verified_fetchcontent_declare(chromaprint CHROMAPRINT)
FetchContent_GetProperties(chromaprint)
if(NOT chromaprint_POPULATED)
    FetchContent_Populate(chromaprint)
endif()

set(_CP_SRC ${chromaprint_SOURCE_DIR}/src)
add_library(
    chromaprint STATIC
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
    ${chromaprint_SOURCE_DIR}/src/3rdparty/kissfft/kiss_fftr.c
    ${chromaprint_SOURCE_DIR}/src/avresample/resample2.c)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/chromaprint_config/config.h"
     "#ifndef CHROMAPRINT_CONFIG_H\n" "#define CHROMAPRINT_CONFIG_H\n" "#define HAVE_ROUND 1\n"
     "#define USE_KISSFFT 1\n" "#define USE_INTERNAL_AVRESAMPLE 1\n" "#endif\n")
target_include_directories(
    chromaprint
    PUBLIC ${chromaprint_SOURCE_DIR}/src
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/chromaprint_config
            ${chromaprint_SOURCE_DIR}/src/3rdparty/kissfft
            ${chromaprint_SOURCE_DIR}/src/3rdparty/kissfft/tools)
target_compile_definitions(chromaprint PRIVATE HAVE_CONFIG_H HAVE_LRINTF)
if(MSVC)
    target_compile_options(chromaprint PRIVATE /w)
else()
    target_compile_options(chromaprint PRIVATE -w)
endif()

# ── Single-file PCM decoders (minimp3, stb_vorbis, dr_flac) ─────────
set(_DECODERS_DIR "${CMAKE_CURRENT_BINARY_DIR}/pcm_decoders")
file(MAKE_DIRECTORY "${_DECODERS_DIR}")

dxx_verified_source(MINIMP3 "${_DECODERS_DIR}/minimp3.h")
dxx_verified_source(MINIMP3_EX "${_DECODERS_DIR}/minimp3_ex.h")
dxx_verified_source(STB_VORBIS "${_DECODERS_DIR}/stb_vorbis.c")
dxx_verified_source(DR_FLAC "${_DECODERS_DIR}/dr_flac.h")

set(SHARED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../shared")

# ── fingerprint_cd ───────────────────────────────────────────────────
add_executable(
    fingerprint_cd
    ${CMAKE_CURRENT_SOURCE_DIR}/fingerprint_cd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/cd_read_contract.c
    ${CMAKE_CURRENT_SOURCE_DIR}/cue_parser.c
    ${CMAKE_CURRENT_SOURCE_DIR}/json_writer.c
    ${SHARED_DIR}/pcm_decoders.c
    ${SHARED_DIR}/fingerprint_gen.c
    ${_DECODERS_DIR}/stb_vorbis.c)
target_include_directories(fingerprint_cd PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${SHARED_DIR}
                                                  ${_DECODERS_DIR})
target_compile_definitions(fingerprint_cd PRIVATE CHROMAPRINT_NODLL)
target_link_libraries(fingerprint_cd PRIVATE chromaprint)
if(MSVC)
    target_compile_options(fingerprint_cd PRIVATE /W3)
else()
    target_compile_options(fingerprint_cd PRIVATE -Wall)
endif()

# ── fingerprint_audio (loose MP3/OGG/FLAC files) ────────────────────
set(FINGERPRINT_AUDIO_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/fingerprint_audio.c ${SHARED_DIR}/pcm_decoders.c
    ${SHARED_DIR}/fingerprint_gen.c ${CMAKE_CURRENT_SOURCE_DIR}/json_writer.c
    ${_DECODERS_DIR}/stb_vorbis.c)
add_executable(fingerprint_audio ${FINGERPRINT_AUDIO_SOURCES})
target_include_directories(fingerprint_audio PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${SHARED_DIR}
                                                     ${_DECODERS_DIR})
target_compile_definitions(fingerprint_audio PRIVATE CHROMAPRINT_NODLL
                                                     MUSIC_PCM_DECODED_MAX_BYTES=1073741824u)
target_link_libraries(fingerprint_audio PRIVATE chromaprint)
if(MSVC)
    target_compile_options(fingerprint_audio PRIVATE /W3)
else()
    target_compile_options(fingerprint_audio PRIVATE -Wall)
endif()

add_executable(test_fingerprint_audio_enumeration ${FINGERPRINT_AUDIO_SOURCES})
target_include_directories(test_fingerprint_audio_enumeration
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${SHARED_DIR} ${_DECODERS_DIR})
target_compile_definitions(
    test_fingerprint_audio_enumeration PRIVATE CHROMAPRINT_NODLL FINGERPRINT_AUDIO_TESTING
                                               MUSIC_PCM_DECODED_MAX_BYTES=1073741824u)
target_link_libraries(test_fingerprint_audio_enumeration PRIVATE chromaprint)
if(MSVC)
    target_compile_options(test_fingerprint_audio_enumeration PRIVATE /W3)
else()
    target_compile_options(test_fingerprint_audio_enumeration PRIVATE -Wall)
endif()

# ── fingerprint_match (find duplicate fingerprints in a JSON DB) ─────
add_executable(fingerprint_match ${CMAKE_CURRENT_SOURCE_DIR}/fingerprint_match.c
                                 ${CMAKE_CURRENT_SOURCE_DIR}/json_writer.c)
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/fingerprint_match.c PROPERTIES LANGUAGE CXX)
target_include_directories(fingerprint_match PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(fingerprint_match PRIVATE CHROMAPRINT_NODLL)
target_link_libraries(fingerprint_match PRIVATE chromaprint nlohmann_json::nlohmann_json)
if(MSVC)
    target_compile_options(fingerprint_match PRIVATE /W3)
else()
    target_compile_options(fingerprint_match PRIVATE -Wall)
endif()

# ── test_fingerprint (unit tests for fingerprint_gen) ────────────────
add_executable(
    test_fingerprint ${CMAKE_CURRENT_SOURCE_DIR}/test_fingerprint.c ${SHARED_DIR}/pcm_decoders.c
                     ${SHARED_DIR}/fingerprint_gen.c ${_DECODERS_DIR}/stb_vorbis.c)
target_include_directories(test_fingerprint PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${SHARED_DIR}
                                                    ${_DECODERS_DIR})
target_compile_definitions(test_fingerprint PRIVATE CHROMAPRINT_NODLL)
target_link_libraries(test_fingerprint PRIVATE chromaprint)
if(MSVC)
    target_compile_definitions(test_fingerprint PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(test_fingerprint PRIVATE /W3)
else()
    target_compile_options(test_fingerprint PRIVATE -Wall)
endif()

# Chromaprint test data lives in the fetched source tree
set(_CP_TEST_DATA "${chromaprint_SOURCE_DIR}/tests/data")
add_test(NAME fingerprint_tests COMMAND test_fingerprint "${_CP_TEST_DATA}")

add_executable(test_chromaprint_db_config ${CMAKE_CURRENT_SOURCE_DIR}/test_chromaprint_db_config.c
                                          ${SHARED_DIR}/chromaprint_db.c)
set_source_files_properties(${SHARED_DIR}/chromaprint_db.c PROPERTIES LANGUAGE CXX)
target_include_directories(test_chromaprint_db_config
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/test_android_stubs ${SHARED_DIR})
target_compile_definitions(test_chromaprint_db_config PRIVATE CHROMAPRINT_NODLL)
target_link_libraries(test_chromaprint_db_config PRIVATE chromaprint nlohmann_json::nlohmann_json)
if(MSVC)
    target_compile_options(test_chromaprint_db_config PRIVATE /W4)
else()
    target_compile_options(test_chromaprint_db_config PRIVATE -Wall -Wextra)
endif()
add_test(NAME chromaprint_db_config_tests COMMAND test_chromaprint_db_config)

find_package(Threads REQUIRED)
add_executable(
    test_chromaprint_db_concurrency ${CMAKE_CURRENT_SOURCE_DIR}/test_chromaprint_db_concurrency.cpp
                                    ${SHARED_DIR}/chromaprint_db.c)
target_include_directories(test_chromaprint_db_concurrency
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/test_android_stubs ${SHARED_DIR})
target_compile_definitions(test_chromaprint_db_concurrency PRIVATE CHROMAPRINT_NODLL)
target_link_libraries(test_chromaprint_db_concurrency
                      PRIVATE chromaprint nlohmann_json::nlohmann_json Threads::Threads)
if(MSVC)
    target_compile_options(test_chromaprint_db_concurrency PRIVATE /W4)
else()
    target_compile_options(test_chromaprint_db_concurrency PRIVATE -Wall -Wextra)
endif()
add_test(NAME chromaprint_db_concurrency_tests COMMAND test_chromaprint_db_concurrency)

add_executable(test_gles3_shim_array_sources
               ${CMAKE_CURRENT_SOURCE_DIR}/../shared/test_gles3_shim_array_sources.c)
target_include_directories(test_gles3_shim_array_sources
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../shared)
add_test(NAME gles3_shim_array_source_tests COMMAND test_gles3_shim_array_sources)
add_executable(
    test_ogl_texture_filename ${CMAKE_CURRENT_SOURCE_DIR}/test/test_ogl_texture_filename.c
                              ${CMAKE_CURRENT_SOURCE_DIR}/../shared/ogl_texture_filename.c)
target_include_directories(test_ogl_texture_filename PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../shared)
if(MSVC)
    target_compile_options(test_ogl_texture_filename PRIVATE /W4)
else()
    target_compile_options(test_ogl_texture_filename PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME ogl_texture_filename_tests COMMAND test_ogl_texture_filename)
add_executable(test_rgba8888 ${CMAKE_CURRENT_SOURCE_DIR}/../shared/test_rgba8888.c)
target_include_directories(test_rgba8888 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../shared)
add_test(NAME rgba8888_tests COMMAND test_rgba8888)
add_executable(test_merged_wall_geometry_hit
               ${CMAKE_CURRENT_SOURCE_DIR}/../shared/test_merged_wall_geometry_hit.c)
target_include_directories(test_merged_wall_geometry_hit
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../shared)
add_test(NAME merged_wall_geometry_hit_tests COMMAND test_merged_wall_geometry_hit)
dxx_verified_fetchcontent_declare(tinysoundfont_test TINYSOUNDFONT)
FetchContent_GetProperties(tinysoundfont_test)
if(NOT tinysoundfont_test_POPULATED)
    FetchContent_Populate(tinysoundfont_test)
endif()
add_executable(
    test_midi_seek_timeline
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/test_midi_seek_timeline.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/midi_seek_timeline.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/tsf_impl.c)
target_include_directories(test_midi_seek_timeline PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../shared
                                                           ${tinysoundfont_test_SOURCE_DIR})
if(NOT MSVC)
    target_link_libraries(test_midi_seek_timeline PRIVATE m)
endif()
add_test(NAME midi_seek_timeline_tests COMMAND test_midi_seek_timeline
                                               ${CMAKE_CURRENT_SOURCE_DIR}/../../assets/gm.sf2)
add_executable(
    test_secret_area_scan_budget
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/test_secret_area_scan_budget.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/secret_area_scan.c)
target_include_directories(test_secret_area_scan_budget
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../shared)
add_test(NAME secret_area_scan_budget_tests COMMAND test_secret_area_scan_budget)
add_executable(test_hmp_android_shared ${CMAKE_CURRENT_SOURCE_DIR}/test_hmp_android_shared.c
                                       ${CMAKE_CURRENT_SOURCE_DIR}/../shared/hmp_android_shared.c)
target_include_directories(
    test_hmp_android_shared PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/test_hmp_stubs
                                    ${CMAKE_CURRENT_SOURCE_DIR}/../shared)
if(MSVC)
    target_compile_options(test_hmp_android_shared PRIVATE /W4)
else()
    target_compile_options(test_hmp_android_shared PRIVATE -Wall -Wextra)
endif()
add_test(NAME hmp_android_shared_tests COMMAND test_hmp_android_shared)
add_executable(
    test_midi_metadata
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/test_midi_metadata.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/midi_metadata.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/hmp_android_shared.c)
target_include_directories(
    test_midi_metadata PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/test_hmp_stubs
                               ${CMAKE_CURRENT_SOURCE_DIR}/../shared)
if(MSVC)
    target_compile_options(test_midi_metadata PRIVATE /W4)
else()
    target_compile_options(test_midi_metadata PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME midi_metadata_tests COMMAND test_midi_metadata)
add_executable(test_utf8_codec ${CMAKE_CURRENT_SOURCE_DIR}/test_utf8_codec.c
                               ${CMAKE_CURRENT_SOURCE_DIR}/../shared/utf8_codec.c)
target_include_directories(test_utf8_codec PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../shared)
add_test(NAME utf8_codec_tests COMMAND test_utf8_codec)
add_executable(test_cd_read_contract ${CMAKE_CURRENT_SOURCE_DIR}/test_cd_read_contract.c
                                     ${CMAKE_CURRENT_SOURCE_DIR}/cd_read_contract.c)
target_include_directories(test_cd_read_contract PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(MSVC)
    target_compile_options(test_cd_read_contract PRIVATE /W4)
else()
    target_compile_options(test_cd_read_contract PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME cd_read_contract_tests COMMAND test_cd_read_contract)
add_test(
    NAME cd_incomplete_read_cli_tests
    COMMAND
        ${CMAKE_COMMAND} "-DEXTRACT_CD=$<TARGET_FILE:extract_cd>"
        "-DFINGERPRINT_CD=$<TARGET_FILE:fingerprint_cd>" -P
        ${CMAKE_CURRENT_SOURCE_DIR}/test_cd_incomplete_read.cmake)
add_executable(
    test_saf_manifest_parser ${CMAKE_CURRENT_SOURCE_DIR}/../shared/test_saf_manifest_parser.c
                             ${CMAKE_CURRENT_SOURCE_DIR}/../shared/saf_manifest_parser.c)
target_include_directories(test_saf_manifest_parser PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../shared)
if(MSVC)
    target_compile_definitions(test_saf_manifest_parser PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(test_saf_manifest_parser PRIVATE /W4)
else()
    target_compile_options(test_saf_manifest_parser PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME saf_manifest_parser_tests COMMAND test_saf_manifest_parser)
add_executable(test_saf_io_contract ${CMAKE_CURRENT_SOURCE_DIR}/../shared/test_saf_io_contract.c
                                    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/saf_io_contract.c)
target_include_directories(test_saf_io_contract PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../shared)
if(MSVC)
    target_compile_options(test_saf_io_contract PRIVATE /W4)
else()
    target_compile_options(test_saf_io_contract PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME saf_io_contract_tests COMMAND test_saf_io_contract)
add_executable(test_music_track_json ${CMAKE_CURRENT_SOURCE_DIR}/../shared/test_music_track_json.c
                                     ${CMAKE_CURRENT_SOURCE_DIR}/../shared/music_track_json.c)
target_include_directories(test_music_track_json PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../shared)
if(MSVC)
    target_compile_options(test_music_track_json PRIVATE /W4)
else()
    target_compile_options(test_music_track_json PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME music_track_json_tests COMMAND test_music_track_json)

add_executable(test_music_name_table ${CMAKE_CURRENT_SOURCE_DIR}/../shared/test_music_name_table.cpp
                                     ${CMAKE_CURRENT_SOURCE_DIR}/../shared/music_name_table.cpp)
target_include_directories(test_music_name_table PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../shared)
target_link_libraries(test_music_name_table PRIVATE nlohmann_json::nlohmann_json)
if(MSVC)
    target_compile_options(test_music_name_table PRIVATE /W4)
else()
    target_compile_options(test_music_name_table PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME music_name_table_tests COMMAND test_music_name_table)
add_executable(test_input_demo_limits
               ${CMAKE_CURRENT_SOURCE_DIR}/../shared/test_input_demo_limits.c)
target_include_directories(test_input_demo_limits PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../shared)
if(MSVC)
    target_compile_options(test_input_demo_limits PRIVATE /W4)
else()
    target_compile_options(test_input_demo_limits PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME input_demo_limit_tests COMMAND test_input_demo_limits)
add_executable(test_rewind_file ${CMAKE_CURRENT_SOURCE_DIR}/../shared/test_rewind_file.c)
target_include_directories(test_rewind_file PRIVATE ${physfs_test_headers_SOURCE_DIR}/src
                                                    ${CMAKE_CURRENT_SOURCE_DIR}/../shared)
if(MSVC)
    target_compile_options(test_rewind_file PRIVATE /W4)
else()
    target_compile_options(test_rewind_file PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME rewind_file_tests COMMAND test_rewind_file)
add_executable(test_classic_demo_wall_validation
               ${CMAKE_CURRENT_SOURCE_DIR}/../shared/test_classic_demo_wall_validation.c)
target_include_directories(test_classic_demo_wall_validation
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../shared)
if(MSVC)
    target_compile_options(test_classic_demo_wall_validation PRIVATE /W4)
else()
    target_compile_options(test_classic_demo_wall_validation PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME classic_demo_wall_validation_tests COMMAND test_classic_demo_wall_validation)
add_executable(test_music_decode_limits
               ${CMAKE_CURRENT_SOURCE_DIR}/../shared/test_music_decode_limits.c)
target_include_directories(test_music_decode_limits PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../shared)
if(MSVC)
    target_compile_options(test_music_decode_limits PRIVATE /W4)
else()
    target_compile_options(test_music_decode_limits PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME music_decode_limit_tests COMMAND test_music_decode_limits)
add_executable(test_bounded_music_read
               ${CMAKE_CURRENT_SOURCE_DIR}/../shared/test_bounded_music_read.c)
target_include_directories(test_bounded_music_read PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../shared)
if(MSVC)
    target_compile_options(test_bounded_music_read PRIVATE /W4)
else()
    target_compile_options(test_bounded_music_read PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME bounded_music_read_tests COMMAND test_bounded_music_read)
add_executable(test_hog_midi_catalog ${CMAKE_CURRENT_SOURCE_DIR}/../shared/test_hog_midi_catalog.c)
target_include_directories(test_hog_midi_catalog PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../shared)
if(MSVC)
    target_compile_definitions(test_hog_midi_catalog PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(test_hog_midi_catalog PRIVATE /W4)
else()
    target_compile_options(test_hog_midi_catalog PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME hog_midi_catalog_tests COMMAND test_hog_midi_catalog)
add_executable(test_android_audio_format
               ${CMAKE_CURRENT_SOURCE_DIR}/../shared/test_android_audio_format.c)
target_include_directories(test_android_audio_format PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../shared)
if(MSVC)
    target_compile_options(test_android_audio_format PRIVATE /W4)
else()
    target_compile_options(test_android_audio_format PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME android_audio_format_tests COMMAND test_android_audio_format)
add_executable(
    test_physfsx_android_setup ${CMAKE_CURRENT_SOURCE_DIR}/../shared/test_physfsx_android_setup.c
                               ${CMAKE_CURRENT_SOURCE_DIR}/../shared/physfsx_android_setup.c)
target_include_directories(test_physfsx_android_setup PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../shared)
if(MSVC)
    target_compile_definitions(test_physfsx_android_setup PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(test_physfsx_android_setup PRIVATE /W4)
else()
    target_compile_options(test_physfsx_android_setup PRIVATE -Wall -Wextra)
endif()
add_test(NAME physfsx_android_setup_tests COMMAND test_physfsx_android_setup)
add_executable(
    test_pilot_pref_transaction
    ${CMAKE_CURRENT_SOURCE_DIR}/test_pilot_pref_transaction.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/pilot_pref_transaction.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/playsave_transaction.c)
target_include_directories(test_pilot_pref_transaction
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../shared)
if(MSVC)
    target_compile_definitions(test_pilot_pref_transaction PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(test_pilot_pref_transaction PRIVATE /W4)
else()
    target_compile_options(test_pilot_pref_transaction PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME pilot_pref_transaction_tests COMMAND test_pilot_pref_transaction)
set_tests_properties(fingerprint_tests PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

# These executables use assert for both setup and verification. Keep assertions active when the host
# suite is configured as Release so test setup is not compiled away before CTest runs.
get_property(EXTRACT_DIRECTORY_TARGETS DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(EXTRACT_TARGET IN LISTS EXTRACT_DIRECTORY_TARGETS)
    if(EXTRACT_TARGET MATCHES "^test_")
        if(MSVC)
            target_compile_options(${EXTRACT_TARGET} PRIVATE /UNDEBUG)
        else()
            target_compile_options(${EXTRACT_TARGET} PRIVATE -UNDEBUG)
        endif()
    endif()
endforeach()
