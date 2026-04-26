add_library(${DXX_TARGET_PREFIX}maths STATIC
    fixc.c
    rand.c
    tables.c
    vecmat.c
    )

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

    find_package(PhysFS REQUIRED)

    add_executable(test_rng_seed_resume ../../android/tests/test_rng_seed_resume.c)
    target_link_libraries(test_rng_seed_resume PRIVATE ${DXX_TARGET_PREFIX}maths)

    add_executable(test_input_demo_rng_mode
        ../../android/tests/test_input_demo_rng_mode.c
        ../../android/app/src/main/cpp/shared/input_demo_rng_mode.c)
    target_link_libraries(test_input_demo_rng_mode PRIVATE ${DXX_TARGET_PREFIX}maths)
    target_include_directories(test_input_demo_rng_mode PRIVATE ../../android/app/src/main/cpp/shared)

    add_executable(test_input_demo_controls
        ../../android/tests/test_input_demo_controls.cpp
        ../../android/app/src/main/cpp/shared/input_demo_controls.cpp)
    target_compile_features(test_input_demo_controls PRIVATE cxx_std_11)
    target_compile_definitions(test_input_demo_controls PRIVATE INPUT_DEMO_TEST_D1=1)
    target_link_libraries(test_input_demo_controls PRIVATE ${DXX_TARGET_PREFIX}maths nlohmann_json::nlohmann_json)
    target_include_directories(test_input_demo_controls PRIVATE ../../android/app/src/main/cpp/shared ../main ${PHYSFS_INCLUDE_DIR})

    add_executable(test_input_demo_fixture
        ../../android/tests/test_input_demo_fixture.cpp
        ../../android/app/src/main/cpp/shared/input_demo_fixture.cpp
        ../../android/app/src/main/cpp/shared/input_demo_rng_mode.c)
    target_compile_features(test_input_demo_fixture PRIVATE cxx_std_11)
    target_compile_definitions(test_input_demo_fixture PRIVATE INPUT_DEMO_TEST_D1=1)
    target_link_libraries(test_input_demo_fixture PRIVATE ${DXX_TARGET_PREFIX}maths nlohmann_json::nlohmann_json)
    target_include_directories(test_input_demo_fixture PRIVATE ../../android/app/src/main/cpp/shared)
endif()

include_directories(../include ../arch/include ../main)

find_package(SDL)
target_include_directories(${DXX_TARGET_PREFIX}maths PUBLIC ${SDL_INCLUDE_DIR})
