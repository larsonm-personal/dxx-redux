add_library(${DXX_TARGET_PREFIX}misc STATIC
    args.c
    error.c
    hash.c
    hmp.c
    ignorecase.c
    physfsx.c
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

if(NOT ANDROID)
    enable_testing()
    add_executable(test_args_defaults ../../android/tests/test_args_defaults.c)
    target_link_libraries(test_args_defaults PRIVATE
        ${DXX_TARGET_PREFIX}misc
        ${DXX_TARGET_PREFIX}mem
        ${SDL_LIBRARY}
        ${PHYSFS_LIBRARY})
    add_test(NAME test_args_defaults COMMAND test_args_defaults)
endif()
