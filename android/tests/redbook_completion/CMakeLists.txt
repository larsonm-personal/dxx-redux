cmake_minimum_required(VERSION 3.22)
project(android_redbook_completion LANGUAGES C CXX)
include(../native_engine_fixture.cmake)
target_sources(${FIXTURE_TARGET} PRIVATE production.c)
target_compile_options(${FIXTURE_TARGET} PRIVATE -ffunction-sections -fdata-sections
                                                 -fvisibility=hidden)
target_compile_definitions(${FIXTURE_TARGET} PRIVATE SHAREPATH=".")
target_include_directories(${FIXTURE_TARGET} PRIVATE "${ENGINE_BUILD_DIR}/_deps/sdl_mixer12-src")
target_link_options(${FIXTURE_TARGET} PRIVATE -Wl,--gc-sections)
target_link_libraries(${FIXTURE_TARGET} PRIVATE "${ENGINE_LIB_DIR}/libSDL12.so"
                                                "${ENGINE_LIB_DIR}/libSDL_mixer.so" log m)
