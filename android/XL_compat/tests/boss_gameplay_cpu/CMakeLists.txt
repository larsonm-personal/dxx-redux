cmake_minimum_required(VERSION 3.21)
project(xl_boss_gameplay_cpu LANGUAGES C CXX)

include(CTest)

set(repo_root ${CMAKE_CURRENT_LIST_DIR}/../../../..)
set(xl ${repo_root}/android/XL_compat)
set(shared ${repo_root}/android/app/src/main/cpp/shared)

# Bind fixture configuration data to the actual production table definitions
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${repo_root}/d2/main/ai.c")
file(READ "${repo_root}/d2/main/ai.c" boss_configuration_source)
set(boss_configuration "// Generated from production d2/main/ai.c\n")
foreach(boss_table IN ITEMS Boss_spew_more Boss_spews_bots_energy Boss_spews_bots_matter
                            Boss_invulnerable_energy Boss_invulnerable_matter Boss_invulnerable_spot)
    string(REGEX MATCH "const ubyte ${boss_table}\\[NUM_D2_BOSSES\\][^\n]*;" definition "${boss_configuration_source}")
    if(NOT definition)
        message(FATAL_ERROR "Missing production boss table: ${boss_table}")
    endif()
    string(APPEND boss_configuration "${definition}\n")
endforeach()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/boss_collision_tables.h" "${boss_configuration}")

# The intact geometry unit retains this production side table through teleport
# link closure. Solid fluid fixtures do not execute classic lighting geometry.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${repo_root}/d2/main/mglobal.c")
file(READ "${repo_root}/d2/main/mglobal.c" geometry_globals)
string(REGEX MATCH "const int Side_to_verts_int[^;]*;" side_table "${geometry_globals}")
if(NOT side_table)
    message(FATAL_ERROR "Missing production side-vertex table")
endif()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/side_table.h"
     "#define encrypt(a,b,c,d) a,b,c,d\n${side_table}\n#undef encrypt\n")

add_executable(
    xl_boss_gameplay_consumer
    boss_gameplay_consumer_test.cpp
    ${repo_root}/d2/main/d1_in_d2/d1_in_d2_levels.c
    ${repo_root}/d2/main/switch.c
    ${repo_root}/d2/main/collide.c
    ${repo_root}/d2/main/cntrlcen.c
    ${repo_root}/d2/main/robot.c
    ${repo_root}/d2/main/d1_in_d2/d1_in_d2.c
    ${repo_root}/d2/main/d1_in_d2/d1_in_d2_ai.c
    ${repo_root}/d2/main/d1_in_d2/d1_in_d2_semantics.c
    ${repo_root}/d2/maths/vecmat.c
    ${repo_root}/d2/maths/tables.c
    ${repo_root}/d2/maths/fixc.c
    ${repo_root}/d2/maths/rand.c
    ${xl}/runtime/object_trigger.cpp
    ${xl}/runtime/object_birth.cpp
    ${xl}/runtime/speed_boost.cpp
    ${xl}/runtime/spawn_points.cpp
    ${xl}/runtime/fluids.cpp
    ${xl}/runtime/geometry.cpp
    ${xl}/lighting.cpp
    ${xl}/runtime/projectile.cpp
    ${xl}/runtime/trigger.cpp
    ${xl}/runtime/robot_disarm.cpp
    ${xl}/runtime/seismic_gameplay.cpp
    ${xl}/runtime/reactor_countdown.cpp
    ${xl}/runtime/boss_spawn.cpp
    ${xl}/runtime/robot_generator.cpp
    ${xl}/runtime/teleport.cpp
    ${shared}/boss_health_shared.c)
# The non-NETWORK reactor unit still refers to CONNECT_PLAYING in an unused
# function. Import its production declaration for this source only.
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    set_property(SOURCE ${repo_root}/d2/main/cntrlcen.c APPEND PROPERTY COMPILE_OPTIONS
                 -include ${repo_root}/d2/main/multi.h)
elseif(MSVC)
    set_property(SOURCE ${repo_root}/d2/main/cntrlcen.c APPEND PROPERTY COMPILE_OPTIONS
                 /FI${repo_root}/d2/main/multi.h)
endif()
target_compile_features(xl_boss_gameplay_consumer PRIVATE cxx_std_17)
target_compile_definitions(xl_boss_gameplay_consumer PRIVATE DXX_BUILD_DESCENT_II DXX_XL_RUNTIME)
target_include_directories(
    xl_boss_gameplay_consumer PRIVATE ${repo_root}/d2/include ${repo_root}/d2/main
                                      ${repo_root}/d2/arch/include ${xl} ${xl}/runtime ${shared} "${CMAKE_CURRENT_BINARY_DIR}")
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
