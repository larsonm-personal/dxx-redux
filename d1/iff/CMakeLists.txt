add_library(${DXX_TARGET_PREFIX}iff STATIC
    iff.c
    )

include_directories(../include ../arch/include ../main)

find_package(SDL)
find_package(PhysFS)
target_include_directories(${DXX_TARGET_PREFIX}iff PUBLIC ${SDL_INCLUDE_DIR} ${PHYSFS_INCLUDE_DIR})
