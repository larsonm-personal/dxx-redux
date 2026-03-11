add_library(${DXX_TARGET_PREFIX}xmodel STATIC
aseread.cpp
submodel.cpp
tga.cpp
xmodel.cpp
	)

find_package(SDL)
find_package(GLEW REQUIRED)

include_directories(../include ../arch/include ../main)
target_include_directories(${DXX_TARGET_PREFIX}xmodel PUBLIC ${SDL_INCLUDE_DIR} ${GLEW_INCLUDE_DIR})
