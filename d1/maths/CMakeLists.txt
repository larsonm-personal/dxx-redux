add_library(${DXX_TARGET_PREFIX}maths STATIC
    fixc.c
    rand.c
    tables.c
    vecmat.c
    )

add_executable(test_rng_seed_resume ../../android/tests/test_rng_seed_resume.c)
target_link_libraries(test_rng_seed_resume PRIVATE ${DXX_TARGET_PREFIX}maths)

include_directories(../include ../arch/include ../main)

find_package(SDL)
target_include_directories(${DXX_TARGET_PREFIX}maths PUBLIC ${SDL_INCLUDE_DIR})
