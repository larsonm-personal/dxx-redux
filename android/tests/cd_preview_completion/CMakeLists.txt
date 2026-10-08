cmake_minimum_required(VERSION 3.22)
project(android_cd_preview_completion LANGUAGES C CXX)
include(../native_engine_fixture.cmake)
target_sources(${FIXTURE_TARGET}
               PRIVATE production.c "${REPO_ROOT}/android/app/src/main/cpp/extract/cue_parser.c")
target_compile_options(${FIXTURE_TARGET} PRIVATE -ffunction-sections -fdata-sections
                                                 -fvisibility=hidden)
target_link_options(${FIXTURE_TARGET} PRIVATE -Wl,--gc-sections)
target_link_libraries(${FIXTURE_TARGET} PRIVATE log OpenSLES m)
