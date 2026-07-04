add_library(${DXX_TARGET_PREFIX}maths STATIC
    fixc.c
    rand.c
    ../../android/app/src/main/cpp/shared/input_demo_rng_trace.c
    tables.c
    vecmat.c
    )

include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/platform-math.cmake)
dxx_link_platform_math(${DXX_TARGET_PREFIX}maths)

if(NOT ANDROID)
    if(NOT TARGET nlohmann_json::nlohmann_json)
        include(FetchContent)
        FetchContent_Declare(
            nlohmann_json
            GIT_REPOSITORY https://github.com/nlohmann/json.git
            GIT_TAG        v3.11.3
            GIT_SHALLOW    TRUE
        )
        set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
        set(JSON_Install OFF CACHE BOOL "" FORCE)
        set(JSON_MultipleHeaders OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(nlohmann_json)
    endif()

    include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/input-demo-codec-deps.cmake)
    include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/input-demo-build-metadata.cmake)

    find_package(PhysFS REQUIRED)
    find_package(ZLIB REQUIRED)

    add_executable(test_rng_seed_resume ../../android/tests/test_rng_seed_resume.c)
    target_link_libraries(test_rng_seed_resume PRIVATE ${DXX_TARGET_PREFIX}maths)

    add_executable(test_deterministic_math ../../android/tests/test_deterministic_math.c)
    target_link_libraries(test_deterministic_math PRIVATE ${DXX_TARGET_PREFIX}maths)
    target_include_directories(test_deterministic_math PRIVATE ../../android/app/src/main/cpp/shared)

    add_executable(test_coop_indicator_lines_math
        ../../android/tests/test_coop_indicator_lines_math.c)
    target_link_libraries(test_coop_indicator_lines_math PRIVATE ${DXX_TARGET_PREFIX}maths)
    target_include_directories(test_coop_indicator_lines_math PRIVATE ../../android/app/src/main/cpp/shared)

    add_executable(test_android_save_meta
        ../../android/tests/test_android_save_meta.c
        ../../android/app/src/main/cpp/shared/android_save_meta.c)
    target_link_libraries(test_android_save_meta PRIVATE ${DXX_TARGET_PREFIX}maths ${PHYSFS_LIBRARY})
    target_include_directories(test_android_save_meta PRIVATE ../../android/app/src/main/cpp/shared ${PHYSFS_INCLUDE_DIR})

    add_executable(test_android_save_set
        ../../android/tests/test_android_save_set.c
        ../../android/app/src/main/cpp/shared/android_save_set.c)
    target_include_directories(test_android_save_set PRIVATE ../../android/app/src/main/cpp/shared)

    add_executable(test_android_rewind_policy
        ../../android/tests/test_android_rewind_policy.c
        ../../android/app/src/main/cpp/shared/android_rewind_policy.c)
    target_link_libraries(test_android_rewind_policy PRIVATE ${DXX_TARGET_PREFIX}maths)
    target_include_directories(test_android_rewind_policy PRIVATE ../../android/app/src/main/cpp/shared)

    add_executable(test_input_demo_rng_mode
        ../../android/tests/test_input_demo_rng_mode.c
        ../../android/app/src/main/cpp/shared/input_demo_rng_mode.c)
    target_link_libraries(test_input_demo_rng_mode PRIVATE ${DXX_TARGET_PREFIX}maths)
    target_include_directories(test_input_demo_rng_mode PRIVATE ../../android/app/src/main/cpp/shared)

    add_executable(test_level_metadata_scan
        ../../android/tests/test_level_metadata_scan.c
        ../../android/app/src/main/cpp/shared/level_metadata_scan.c)
    target_link_libraries(test_level_metadata_scan PRIVATE ${DXX_TARGET_PREFIX}maths)
    target_include_directories(test_level_metadata_scan PRIVATE ../../android/app/src/main/cpp/shared)

    add_executable(test_input_demo_controls
        ../../android/tests/test_input_demo_controls.cpp
        ../../android/app/src/main/cpp/shared/input_demo_controls.cpp)
    target_compile_features(test_input_demo_controls PRIVATE cxx_std_11)
    target_compile_definitions(test_input_demo_controls PRIVATE INPUT_DEMO_TEST_D1=1)
    target_link_libraries(test_input_demo_controls PRIVATE ${DXX_TARGET_PREFIX}maths nlohmann_json::nlohmann_json)
    target_include_directories(test_input_demo_controls PRIVATE ../../android/app/src/main/cpp/shared ../main ${PHYSFS_INCLUDE_DIR})

    add_executable(test_input_demo_fixture
        ../../android/tests/test_input_demo_fixture.cpp
        ../../android/app/src/main/cpp/shared/input_demo_controls.cpp
        ../../android/app/src/main/cpp/shared/input_demo_fixture.cpp
        ../../android/app/src/main/cpp/shared/input_demo_result.cpp
        ../../android/app/src/main/cpp/shared/input_demo_rng_mode.c)
    target_compile_features(test_input_demo_fixture PRIVATE cxx_std_11)
    target_compile_definitions(test_input_demo_fixture PRIVATE INPUT_DEMO_TEST_D1=1)
    target_link_libraries(test_input_demo_fixture PRIVATE ${DXX_TARGET_PREFIX}maths nlohmann_json::nlohmann_json)
    target_include_directories(test_input_demo_fixture PRIVATE ../../android/app/src/main/cpp/shared)
    dxx_input_demo_apply_build_metadata(test_input_demo_fixture)

    add_executable(test_input_demo_recorder
        ../../android/tests/test_input_demo_recorder.cpp
        ../../android/app/src/main/cpp/shared/input_demo_controls.cpp
        ../../android/app/src/main/cpp/shared/input_demo_fixture.cpp
        ../../android/app/src/main/cpp/shared/input_demo_codec.cpp
        ../../android/app/src/main/cpp/shared/input_demo_result.cpp
        ../../android/app/src/main/cpp/shared/input_demo_state_trace.cpp
        ../../android/app/src/main/cpp/shared/input_demo_replay.cpp
        ../../android/app/src/main/cpp/shared/input_demo_recorder.cpp)
    target_compile_features(test_input_demo_recorder PRIVATE cxx_std_11)
    target_compile_definitions(test_input_demo_recorder PRIVATE INPUT_DEMO_TEST_D1=1)
    target_link_libraries(test_input_demo_recorder PRIVATE ${DXX_TARGET_PREFIX}maths nlohmann_json::nlohmann_json ZLIB::ZLIB)
    target_include_directories(test_input_demo_recorder PRIVATE ../../android/app/src/main/cpp/shared)
    dxx_input_demo_apply_codec_deps(test_input_demo_recorder)
    dxx_input_demo_apply_build_metadata(test_input_demo_recorder)

    add_executable(test_input_demo_result
        ../../android/tests/test_input_demo_result.cpp
        ../../android/app/src/main/cpp/shared/input_demo_result.cpp)
    target_compile_features(test_input_demo_result PRIVATE cxx_std_11)
    target_link_libraries(test_input_demo_result PRIVATE nlohmann_json::nlohmann_json)
    target_include_directories(test_input_demo_result PRIVATE ../../android/app/src/main/cpp/shared)

    add_executable(test_input_demo_replay
        ../../android/tests/test_input_demo_replay.cpp
        ../../android/app/src/main/cpp/shared/input_demo_controls.cpp
        ../../android/app/src/main/cpp/shared/input_demo_fixture.cpp
        ../../android/app/src/main/cpp/shared/input_demo_codec.cpp
        ../../android/app/src/main/cpp/shared/input_demo_result.cpp
        ../../android/app/src/main/cpp/shared/input_demo_state_trace.cpp
        ../../android/app/src/main/cpp/shared/input_demo_recorder.cpp
        ../../android/app/src/main/cpp/shared/input_demo_replay.cpp)
    target_compile_features(test_input_demo_replay PRIVATE cxx_std_11)
    target_compile_definitions(test_input_demo_replay PRIVATE INPUT_DEMO_TEST_D1=1)
    target_link_libraries(test_input_demo_replay PRIVATE ${DXX_TARGET_PREFIX}maths nlohmann_json::nlohmann_json ZLIB::ZLIB)
    target_include_directories(test_input_demo_replay PRIVATE ../../android/app/src/main/cpp/shared)
    dxx_input_demo_apply_codec_deps(test_input_demo_replay)
    dxx_input_demo_apply_build_metadata(test_input_demo_replay)

    enable_testing()
    foreach(test_target IN ITEMS
            test_rng_seed_resume
            test_deterministic_math
            test_coop_indicator_lines_math
            test_android_save_meta
            test_android_save_set
            test_android_rewind_policy
            test_input_demo_rng_mode
            test_level_metadata_scan
            test_input_demo_controls
            test_input_demo_fixture
            test_input_demo_recorder
            test_input_demo_result
            test_input_demo_replay)
        add_test(NAME ${test_target} COMMAND ${test_target})
        set_tests_properties(${test_target} PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    endforeach()
endif()

include_directories(../include ../arch/include ../main)

find_package(SDL)
target_include_directories(${DXX_TARGET_PREFIX}maths PUBLIC ${SDL_INCLUDE_DIR})
target_include_directories(${DXX_TARGET_PREFIX}maths PRIVATE ../../android/app/src/main/cpp/shared)
