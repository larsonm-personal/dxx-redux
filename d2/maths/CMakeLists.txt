add_library(${DXX_TARGET_PREFIX}maths STATIC
    fixc.c
    rand.c
    tables.c
    vecmat.c
    )

add_executable(test_rng_seed_resume ../../android/tests/test_rng_seed_resume.c)
target_link_libraries(test_rng_seed_resume PRIVATE ${DXX_TARGET_PREFIX}maths)

add_executable(test_input_demo_rng_mode
    ../../android/tests/test_input_demo_rng_mode.c
    ../../android/app/src/main/cpp/shared/input_demo_rng_mode.c)
target_link_libraries(test_input_demo_rng_mode PRIVATE ${DXX_TARGET_PREFIX}maths)
target_include_directories(test_input_demo_rng_mode PRIVATE ../../android/app/src/main/cpp/shared)

include_directories(../include ../arch/include ../main)

find_package(SDL)
target_include_directories(${DXX_TARGET_PREFIX}maths PUBLIC ${SDL_INCLUDE_DIR})
