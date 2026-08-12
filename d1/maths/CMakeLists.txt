add_library(${DXX_TARGET_PREFIX}maths STATIC
    fixc.c
    rand.c
    ../../android/app/src/main/cpp/shared/input_demo_rng_trace.c
    tables.c
    vecmat.c
    )

include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/platform-math.cmake)
dxx_link_platform_math(${DXX_TARGET_PREFIX}maths)

include_directories(../include ../arch/include ../main)

find_package(SDL)
target_include_directories(${DXX_TARGET_PREFIX}maths PUBLIC ${SDL_INCLUDE_DIR})
target_include_directories(${DXX_TARGET_PREFIX}maths PRIVATE ../../android/app/src/main/cpp/shared)
