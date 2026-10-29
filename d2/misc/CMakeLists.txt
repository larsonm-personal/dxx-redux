add_library(${DXX_TARGET_PREFIX}misc STATIC
    args.c
    error.c
    hash.c
    hmp.c
    ignorecase.c
    physfsrwops.c
    physfsx.c
    ../../android/XL_compat/hog_archiver.cpp
    strio.c
    strutil.c
    )

if(PNG)
    target_sources(${DXX_TARGET_PREFIX}misc PRIVATE pngfile.c)
endif()

include_directories(../include ../arch/include ../main)

find_package(SDL)
find_package(PhysFS)
target_include_directories(${DXX_TARGET_PREFIX}misc PUBLIC ${SDL_INCLUDE_DIR} ${PHYSFS_INCLUDE_DIR})

target_compile_features(${DXX_TARGET_PREFIX}misc PRIVATE cxx_std_17)

# Android filesystem setup is compiled in this library, not the game target
target_compile_definitions(${DXX_TARGET_PREFIX}misc PRIVATE DXX_BUILD_DESCENT_II)
