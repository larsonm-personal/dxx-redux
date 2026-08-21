if(NOT DXX_TEST_GAME MATCHES "^d[12]$")
    message(FATAL_ERROR "DXX_TEST_GAME must be d1 or d2")
endif()

set(DXX_TEST_GAME_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../${DXX_TEST_GAME}")
set(DXX_TEST_SHARED_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../app/src/main/cpp/shared")

include_directories("${DXX_TEST_GAME_SOURCE_DIR}/include"
                    "${DXX_TEST_GAME_SOURCE_DIR}/arch/include" "${DXX_TEST_GAME_SOURCE_DIR}/main")

if(NOT TARGET nlohmann_json::nlohmann_json)
    include(FetchContent)
    FetchContent_Declare(nlohmann_json GIT_REPOSITORY https://github.com/nlohmann/json.git
                         GIT_TAG v3.11.3 GIT_SHALLOW TRUE)
    set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
    set(JSON_Install OFF CACHE BOOL "" FORCE)
    set(JSON_MultipleHeaders OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(nlohmann_json)
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/input-demo-codec-deps.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/input-demo-build-metadata.cmake")

find_package(PhysFS REQUIRED)
find_package(SDL)
find_package(ZLIB REQUIRED)

add_executable(test_rng_seed_resume test_rng_seed_resume.c)
target_link_libraries(test_rng_seed_resume PRIVATE ${DXX_TARGET_PREFIX}maths)

add_executable(test_deterministic_math test_deterministic_math.c)
target_link_libraries(test_deterministic_math PRIVATE ${DXX_TARGET_PREFIX}maths)
target_include_directories(test_deterministic_math PRIVATE "${DXX_TEST_SHARED_SOURCE_DIR}")

if(DXX_TEST_GAME STREQUAL "d2")
    add_executable(test_d1_pig_validation test_d1_pig_validation.c
                                          "${DXX_TEST_GAME_SOURCE_DIR}/main/d1_pig_validation.c")
    target_include_directories(test_d1_pig_validation PRIVATE "${DXX_TEST_GAME_SOURCE_DIR}/main")
endif()

add_executable(test_coop_indicator_lines_math test_coop_indicator_lines_math.c)
target_link_libraries(test_coop_indicator_lines_math PRIVATE ${DXX_TARGET_PREFIX}maths)
target_include_directories(test_coop_indicator_lines_math PRIVATE "${DXX_TEST_SHARED_SOURCE_DIR}")

add_executable(test_coop_warp_policy test_coop_warp_policy.c)
target_link_libraries(test_coop_warp_policy PRIVATE ${DXX_TARGET_PREFIX}maths)
target_include_directories(test_coop_warp_policy PRIVATE "${DXX_TEST_SHARED_SOURCE_DIR}/coop")

add_executable(
    test_coop_powerup_duplication test_coop_powerup_duplication.c
                                  "${DXX_TEST_SHARED_SOURCE_DIR}/coop/coop_powerup_duplication.c")
if(DXX_TEST_GAME STREQUAL "d2")
    target_compile_definitions(test_coop_powerup_duplication PRIVATE DXX_BUILD_DESCENT_II=1)
endif()
target_link_libraries(test_coop_powerup_duplication PRIVATE ${DXX_TARGET_PREFIX}maths)
target_include_directories(
    test_coop_powerup_duplication
    PRIVATE "${DXX_TEST_SHARED_SOURCE_DIR}" "${DXX_TEST_SHARED_SOURCE_DIR}/coop"
            ${PHYSFS_INCLUDE_DIR})

add_executable(test_hud_layout test_hud_layout.c)
target_include_directories(test_hud_layout PRIVATE "${DXX_TEST_SHARED_SOURCE_DIR}")

add_executable(test_homing_compat test_homing_compat.c)
target_include_directories(test_homing_compat PRIVATE "${DXX_TEST_SHARED_SOURCE_DIR}")

add_executable(test_matcen_mode test_matcen_mode.c "${DXX_TEST_SHARED_SOURCE_DIR}/matcen_mode.c")
target_include_directories(test_matcen_mode PRIVATE "${DXX_TEST_SHARED_SOURCE_DIR}")

add_executable(test_android_save_meta test_android_save_meta.c
                                      "${DXX_TEST_SHARED_SOURCE_DIR}/android_save_meta.c")
target_link_libraries(test_android_save_meta PRIVATE ${DXX_TARGET_PREFIX}maths ${PHYSFS_LIBRARY})
target_include_directories(test_android_save_meta PRIVATE "${DXX_TEST_SHARED_SOURCE_DIR}"
                                                          ${PHYSFS_INCLUDE_DIR})

add_executable(test_android_save_set test_android_save_set.c
                                     "${DXX_TEST_SHARED_SOURCE_DIR}/android_save_set.c")
target_include_directories(test_android_save_set PRIVATE "${DXX_TEST_SHARED_SOURCE_DIR}")

add_executable(
    test_android_file_pair_transaction
    test_android_file_pair_transaction.c
    "${DXX_TEST_SHARED_SOURCE_DIR}/android_file_pair_transaction.c")
target_include_directories(test_android_file_pair_transaction
                           PRIVATE "${DXX_TEST_SHARED_SOURCE_DIR}")

add_executable(test_playsave_transaction test_playsave_transaction.c
                                         "${DXX_TEST_SHARED_SOURCE_DIR}/playsave_transaction.c")
target_compile_definitions(test_playsave_transaction PRIVATE PLAYSAVE_TRANSACTION_TESTING=1)
target_include_directories(test_playsave_transaction PRIVATE "${DXX_TEST_SHARED_SOURCE_DIR}")

add_executable(test_playsave_layout test_playsave_layout.c
                                    "${DXX_TEST_SHARED_SOURCE_DIR}/playsave_layout.c")

add_executable(
    test_playsave_text test_playsave_text.c "${DXX_TEST_SHARED_SOURCE_DIR}/playsave_text.c"
                       "${DXX_TEST_SHARED_SOURCE_DIR}/playsave_transaction.c")

add_executable(test_kconfig_android_shared test_kconfig_android_shared.c
                                           "${DXX_TEST_SHARED_SOURCE_DIR}/kconfig_android_shared.c")

if(DXX_TEST_GAME STREQUAL "d2")
    add_executable(test_classic_demo_json test_classic_demo_json.c
                                          "${DXX_TEST_SHARED_SOURCE_DIR}/classic_demo_json.c")
    target_include_directories(test_classic_demo_json PRIVATE "${DXX_TEST_SHARED_SOURCE_DIR}")

    add_executable(test_bounded_rle test_bounded_rle.c
                                    "${DXX_TEST_SHARED_SOURCE_DIR}/bounded_rle.c")
    target_include_directories(test_bounded_rle PRIVATE "${DXX_TEST_SHARED_SOURCE_DIR}")
endif()

add_executable(test_android_rewind_policy test_android_rewind_policy.c
                                          "${DXX_TEST_SHARED_SOURCE_DIR}/android_rewind_policy.c")
target_link_libraries(test_android_rewind_policy PRIVATE ${DXX_TARGET_PREFIX}maths)
target_include_directories(test_android_rewind_policy PRIVATE "${DXX_TEST_SHARED_SOURCE_DIR}")

add_executable(test_android_render_fov test_android_render_fov.c
                                       "${DXX_TEST_SHARED_SOURCE_DIR}/android_render_fov.c")
target_include_directories(test_android_render_fov PRIVATE "${DXX_TEST_SHARED_SOURCE_DIR}")

add_executable(
    test_android_slowdown_detector test_android_slowdown_detector.c
                                   "${DXX_TEST_SHARED_SOURCE_DIR}/android_slowdown_detector.c")
target_include_directories(test_android_slowdown_detector PRIVATE "${DXX_TEST_SHARED_SOURCE_DIR}")

add_executable(
    test_net_udp_reconnect_auth test_net_udp_reconnect_auth.c
                                "${DXX_TEST_SHARED_SOURCE_DIR}/net/net_udp_reconnect_auth.c")
target_include_directories(test_net_udp_reconnect_auth PRIVATE "${DXX_TEST_SHARED_SOURCE_DIR}/net")

add_executable(
    test_coop_host_migration_policy
    test_coop_host_migration_policy.c
    "${DXX_TEST_SHARED_SOURCE_DIR}/coop/coop_host_migration_policy.c")
target_include_directories(test_coop_host_migration_policy
                           PRIVATE "${DXX_TEST_SHARED_SOURCE_DIR}/coop")

add_executable(test_coop_player_session test_coop_player_session.c)
target_include_directories(
    test_coop_player_session PRIVATE "${DXX_TEST_SHARED_SOURCE_DIR}/coop" ${PHYSFS_INCLUDE_DIR}
                                     ${SDL_INCLUDE_DIR})

add_executable(test_multi_save_transfer_policy test_multi_save_transfer_policy.c)
target_include_directories(test_multi_save_transfer_policy PRIVATE "${DXX_TEST_SHARED_SOURCE_DIR}")

add_executable(test_input_demo_rng_mode test_input_demo_rng_mode.c
                                        "${DXX_TEST_SHARED_SOURCE_DIR}/input_demo_rng_mode.c")
target_link_libraries(test_input_demo_rng_mode PRIVATE ${DXX_TARGET_PREFIX}maths)
target_include_directories(test_input_demo_rng_mode PRIVATE "${DXX_TEST_SHARED_SOURCE_DIR}")

add_executable(test_android_axis_mailbox test_android_axis_mailbox.cpp
                                         "${DXX_TEST_SHARED_SOURCE_DIR}/android_axis_mailbox.cpp")
target_compile_features(test_android_axis_mailbox PRIVATE cxx_std_11)
target_include_directories(test_android_axis_mailbox PRIVATE "${DXX_TEST_SHARED_SOURCE_DIR}")

add_executable(test_level_statistics test_level_statistics.cpp)
target_compile_features(test_level_statistics PRIVATE cxx_std_11)
target_include_directories(test_level_statistics PRIVATE "${DXX_TEST_SHARED_SOURCE_DIR}")

add_executable(
    test_level_metadata_scan
    test_level_metadata_scan.c
    "${DXX_TEST_SHARED_SOURCE_DIR}/level_metadata_scan.c"
    "${DXX_TEST_SHARED_SOURCE_DIR}/route_snapshot.cpp"
    "${DXX_TEST_SHARED_SOURCE_DIR}/route_edge.cpp"
    "${DXX_TEST_SHARED_SOURCE_DIR}/route_planner.cpp")
target_compile_features(test_level_metadata_scan PRIVATE cxx_std_11)
target_link_libraries(test_level_metadata_scan PRIVATE ${DXX_TARGET_PREFIX}maths)
target_include_directories(test_level_metadata_scan PRIVATE "${DXX_TEST_SHARED_SOURCE_DIR}")

add_executable(
    test_route_snapshot
    test_route_snapshot.cpp
    "${DXX_TEST_SHARED_SOURCE_DIR}/route_snapshot.cpp"
    "${DXX_TEST_SHARED_SOURCE_DIR}/route_edge.cpp"
    "${DXX_TEST_SHARED_SOURCE_DIR}/route_planner.cpp"
    "${DXX_TEST_SHARED_SOURCE_DIR}/level_metadata_scan.c")
target_compile_features(test_route_snapshot PRIVATE cxx_std_11)
target_link_libraries(test_route_snapshot PRIVATE ${DXX_TARGET_PREFIX}maths)
target_include_directories(test_route_snapshot PRIVATE "${DXX_TEST_SHARED_SOURCE_DIR}")

add_executable(test_route_analysis_cache test_route_analysis_cache.c
                                         "${DXX_TEST_SHARED_SOURCE_DIR}/route_analysis_cache.c")
target_include_directories(test_route_analysis_cache PRIVATE "${DXX_TEST_SHARED_SOURCE_DIR}")

add_executable(test_android_route_metadata_progress test_android_route_metadata_progress.c)
target_include_directories(test_android_route_metadata_progress
                           PRIVATE "${DXX_TEST_SHARED_SOURCE_DIR}")

if(DXX_TEST_GAME STREQUAL "d2")
    add_executable(
        test_escort_owner_policy test_escort_owner_policy.c
                                 "${DXX_TEST_GAME_SOURCE_DIR}/main/escort_owner_policy.c")
    target_include_directories(test_escort_owner_policy PRIVATE "${DXX_TEST_GAME_SOURCE_DIR}/main")

    add_executable(test_escort_exit_policy test_escort_exit_policy.c)
    target_include_directories(test_escort_exit_policy PRIVATE "${DXX_TEST_GAME_SOURCE_DIR}/main")

    add_executable(test_escort_goal_policy test_escort_goal_policy.c)
    target_include_directories(test_escort_goal_policy PRIVATE "${DXX_TEST_GAME_SOURCE_DIR}/main")

    add_executable(test_thief_network_policy test_thief_network_policy.c)
    target_include_directories(test_thief_network_policy PRIVATE "${DXX_TEST_GAME_SOURCE_DIR}/main")
endif()

if(DXX_TEST_GAME STREQUAL "d1")
    set(DXX_INPUT_DEMO_TEST_DEFINITION INPUT_DEMO_TEST_D1=1)
else()
    set(DXX_INPUT_DEMO_TEST_DEFINITION INPUT_DEMO_TEST_D2=1)
endif()

add_executable(test_input_demo_controls test_input_demo_controls.cpp
                                        "${DXX_TEST_SHARED_SOURCE_DIR}/input_demo_controls.cpp")
target_compile_features(test_input_demo_controls PRIVATE cxx_std_11)
target_compile_definitions(test_input_demo_controls PRIVATE ${DXX_INPUT_DEMO_TEST_DEFINITION})
target_link_libraries(test_input_demo_controls PRIVATE ${DXX_TARGET_PREFIX}maths
                                                       nlohmann_json::nlohmann_json)
target_include_directories(
    test_input_demo_controls PRIVATE "${DXX_TEST_SHARED_SOURCE_DIR}"
                                     "${DXX_TEST_GAME_SOURCE_DIR}/main" ${PHYSFS_INCLUDE_DIR})

add_executable(
    test_input_demo_fixture
    test_input_demo_fixture.cpp
    "${DXX_TEST_SHARED_SOURCE_DIR}/input_demo_controls.cpp"
    "${DXX_TEST_SHARED_SOURCE_DIR}/input_demo_fixture.cpp"
    "${DXX_TEST_SHARED_SOURCE_DIR}/input_demo_result.cpp"
    "${DXX_TEST_SHARED_SOURCE_DIR}/input_demo_rng_mode.c")
target_compile_features(test_input_demo_fixture PRIVATE cxx_std_11)
target_compile_definitions(test_input_demo_fixture PRIVATE ${DXX_INPUT_DEMO_TEST_DEFINITION})
target_link_libraries(test_input_demo_fixture PRIVATE ${DXX_TARGET_PREFIX}maths
                                                      nlohmann_json::nlohmann_json)
target_include_directories(test_input_demo_fixture PRIVATE "${DXX_TEST_SHARED_SOURCE_DIR}")
dxx_input_demo_apply_build_metadata(test_input_demo_fixture)

add_executable(
    test_input_demo_recorder
    test_input_demo_recorder.cpp
    "${DXX_TEST_SHARED_SOURCE_DIR}/input_demo_controls.cpp"
    "${DXX_TEST_SHARED_SOURCE_DIR}/input_demo_fixture.cpp"
    "${DXX_TEST_SHARED_SOURCE_DIR}/input_demo_codec.cpp"
    "${DXX_TEST_SHARED_SOURCE_DIR}/input_demo_result.cpp"
    "${DXX_TEST_SHARED_SOURCE_DIR}/input_demo_state_trace.cpp"
    "${DXX_TEST_SHARED_SOURCE_DIR}/input_demo_replay.cpp"
    "${DXX_TEST_SHARED_SOURCE_DIR}/input_demo_direct_command_policy.c"
    "${DXX_TEST_SHARED_SOURCE_DIR}/input_demo_recorder.cpp")
target_compile_features(test_input_demo_recorder PRIVATE cxx_std_11)
target_compile_definitions(test_input_demo_recorder PRIVATE ${DXX_INPUT_DEMO_TEST_DEFINITION})
target_link_libraries(test_input_demo_recorder PRIVATE ${DXX_TARGET_PREFIX}maths
                                                       nlohmann_json::nlohmann_json ZLIB::ZLIB)
target_include_directories(test_input_demo_recorder PRIVATE "${DXX_TEST_SHARED_SOURCE_DIR}")
dxx_input_demo_apply_codec_deps(test_input_demo_recorder)
dxx_input_demo_apply_build_metadata(test_input_demo_recorder)

add_executable(test_input_demo_result test_input_demo_result.cpp
                                      "${DXX_TEST_SHARED_SOURCE_DIR}/input_demo_result.cpp")
target_compile_features(test_input_demo_result PRIVATE cxx_std_11)
target_link_libraries(test_input_demo_result PRIVATE nlohmann_json::nlohmann_json)
target_include_directories(test_input_demo_result PRIVATE "${DXX_TEST_SHARED_SOURCE_DIR}")

add_executable(
    test_input_demo_replay
    test_input_demo_replay.cpp
    test_input_demo_policy_pack1.c
    test_input_demo_policy_pack16.c
    "${DXX_TEST_SHARED_SOURCE_DIR}/input_demo_controls.cpp"
    "${DXX_TEST_SHARED_SOURCE_DIR}/input_demo_fixture.cpp"
    "${DXX_TEST_SHARED_SOURCE_DIR}/input_demo_codec.cpp"
    "${DXX_TEST_SHARED_SOURCE_DIR}/input_demo_result.cpp"
    "${DXX_TEST_SHARED_SOURCE_DIR}/input_demo_state_trace.cpp"
    "${DXX_TEST_SHARED_SOURCE_DIR}/input_demo_recorder.cpp"
    "${DXX_TEST_SHARED_SOURCE_DIR}/input_demo_replay.cpp"
    "${DXX_TEST_SHARED_SOURCE_DIR}/input_demo_direct_command_policy.c")
target_compile_features(test_input_demo_replay PRIVATE cxx_std_11)
target_compile_definitions(test_input_demo_replay PRIVATE ${DXX_INPUT_DEMO_TEST_DEFINITION})
target_link_libraries(test_input_demo_replay PRIVATE ${DXX_TARGET_PREFIX}maths
                                                     nlohmann_json::nlohmann_json ZLIB::ZLIB)
target_include_directories(test_input_demo_replay PRIVATE "${DXX_TEST_SHARED_SOURCE_DIR}")
dxx_input_demo_apply_codec_deps(test_input_demo_replay)
dxx_input_demo_apply_build_metadata(test_input_demo_replay)

add_executable(test_args_defaults test_args_defaults.c)
if(DXX_TEST_GAME STREQUAL "d2")
    target_compile_definitions(test_args_defaults PRIVATE DXX_ARGS_TEST_D2=1)
endif()
target_link_libraries(test_args_defaults PRIVATE ${DXX_TARGET_PREFIX}misc ${DXX_TARGET_PREFIX}mem
                                                 ${SDL_LIBRARY} ${PHYSFS_LIBRARY})

set(DXX_HOST_TEST_TARGETS
    test_rng_seed_resume
    test_deterministic_math
    test_coop_indicator_lines_math
    test_coop_warp_policy
    test_coop_powerup_duplication
    test_hud_layout
    test_homing_compat
    test_android_save_meta
    test_android_save_set
    test_android_file_pair_transaction
    test_playsave_transaction
    test_playsave_layout
    test_playsave_text
    test_kconfig_android_shared
    test_android_rewind_policy
    test_android_render_fov
    test_android_slowdown_detector
    test_net_udp_reconnect_auth
    test_coop_host_migration_policy
    test_coop_player_session
    test_multi_save_transfer_policy
    test_input_demo_rng_mode
    test_android_axis_mailbox
    test_level_statistics
    test_level_metadata_scan
    test_route_snapshot
    test_route_analysis_cache
    test_android_route_metadata_progress
    test_input_demo_controls
    test_input_demo_fixture
    test_input_demo_recorder
    test_input_demo_result
    test_input_demo_replay
    test_args_defaults)

if(DXX_TEST_GAME STREQUAL "d2")
    list(
        APPEND
        DXX_HOST_TEST_TARGETS
        test_d1_pig_validation
        test_classic_demo_json
        test_bounded_rle
        test_escort_owner_policy
        test_escort_exit_policy
        test_escort_goal_policy
        test_thief_network_policy)
endif()

foreach(test_target IN LISTS DXX_HOST_TEST_TARGETS)
    add_test(NAME ${test_target} COMMAND ${test_target})
    set_tests_properties(${test_target} PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
endforeach()
