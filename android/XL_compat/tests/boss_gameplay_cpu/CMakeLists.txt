cmake_minimum_required(VERSION 3.21)
project(xl_boss_gameplay_cpu LANGUAGES C CXX)

include(CTest)

set(repo_root ${CMAKE_CURRENT_LIST_DIR}/../../../..)
set(xl ${repo_root}/android/XL_compat)
set(shared ${repo_root}/android/app/src/main/cpp/shared)

add_executable(
    xl_boss_gameplay_consumer
    boss_gameplay_consumer_test.cpp
    ${repo_root}/d2/main/collide.c
    ${repo_root}/d2/main/robot.c
    ${repo_root}/d2/main/d1_in_d2/d1_in_d2.c
    ${repo_root}/d2/main/d1_in_d2/d1_in_d2_ai.c
    ${repo_root}/d2/main/d1_in_d2/d1_in_d2_semantics.c
    ${repo_root}/d2/maths/vecmat.c
    ${repo_root}/d2/maths/fixc.c
    ${repo_root}/d2/maths/rand.c
    ${xl}/runtime/object_trigger.cpp
    ${xl}/runtime/object_birth.cpp
    ${xl}/runtime/boss_spawn.cpp
    ${xl}/runtime/robot_generator.cpp
    ${xl}/runtime/teleport.cpp
    ${shared}/boss_health_shared.c)
target_compile_features(xl_boss_gameplay_consumer PRIVATE cxx_std_17)
target_compile_definitions(xl_boss_gameplay_consumer PRIVATE DXX_BUILD_DESCENT_II DXX_XL_RUNTIME)
target_include_directories(
    xl_boss_gameplay_consumer PRIVATE ${repo_root}/d2/include ${repo_root}/d2/main
                                      ${repo_root}/d2/arch/include ${xl} ${xl}/runtime ${shared})
find_path(PHYSFS_INCLUDE_DIR physfs.h REQUIRED)
find_path(SDL_INCLUDE_DIR SDL.h PATH_SUFFIXES SDL REQUIRED)
find_path(NLOHMANN_JSON_INCLUDE_DIR nlohmann/json_fwd.hpp REQUIRED)
target_include_directories(
    xl_boss_gameplay_consumer PRIVATE ${PHYSFS_INCLUDE_DIR} ${SDL_INCLUDE_DIR}
                                      ${NLOHMANN_JSON_INCLUDE_DIR})
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(xl_boss_gameplay_consumer PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(xl_boss_gameplay_consumer PRIVATE -Wl,--gc-sections)
endif()
add_test(NAME xl_boss_gameplay_consumer COMMAND xl_boss_gameplay_consumer)
