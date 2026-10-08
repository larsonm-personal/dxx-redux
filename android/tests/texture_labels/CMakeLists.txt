cmake_minimum_required(VERSION 3.22)
project(android_texture_labels LANGUAGES CXX)
include(../native_engine_fixture.cmake)
target_compile_definitions(${FIXTURE_TARGET} PRIVATE OGL OGLES OGL_MERGE)
target_link_libraries(${FIXTURE_TARGET} PRIVATE EGL GLESv3)

target_include_directories(${FIXTURE_TARGET} PRIVATE "${REPO_ROOT}/${GAME}/xmodel")
