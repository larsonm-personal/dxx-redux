cmake_minimum_required(VERSION 3.22)
project(android_d1_mod_patches LANGUAGES CXX)
include(../native_engine_fixture.cmake)
target_compile_definitions(${FIXTURE_TARGET} PRIVATE OGL)
