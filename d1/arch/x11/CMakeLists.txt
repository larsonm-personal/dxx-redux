add_library(${DXX_TARGET_PREFIX}arch_x11 STATIC
    messagebox.c
    x11messagebox.c
    )

include_directories(${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/arch/include ${PROJECT_SOURCE_DIR}/main)

target_include_directories(${DXX_TARGET_PREFIX}arch_x11 PUBLIC ${SDL_INCLUDE_DIR})
