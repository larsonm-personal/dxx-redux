cmake_minimum_required(VERSION 3.20)
project(etc2tool LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 11)

# etc2comp source (use already-fetched copy or fetch fresh)
include(FetchContent)
FetchContent_Declare(etc2comp
    GIT_REPOSITORY https://github.com/google/etc2comp.git
    GIT_TAG        39422c1
    GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(etc2comp)
if(NOT etc2comp_POPULATED)
    FetchContent_Populate(etc2comp)
endif()

file(GLOB _ETC2_SOURCES
    ${etc2comp_SOURCE_DIR}/EtcLib/Etc/*.cpp
    ${etc2comp_SOURCE_DIR}/EtcLib/EtcCodec/*.cpp
)
add_library(EtcLib STATIC ${_ETC2_SOURCES})
target_include_directories(EtcLib PUBLIC
    ${etc2comp_SOURCE_DIR}/EtcLib/Etc
    ${etc2comp_SOURCE_DIR}/EtcLib/EtcCodec
)
if(MSVC)
    target_compile_options(EtcLib PRIVATE /W0)
else()
    target_compile_options(EtcLib PRIVATE -w)
endif()

# stb_image.h (single-header image loader)
set(_STB_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/stb_image.h")
if(EXISTS "${_STB_IMAGE}")
    file(SIZE "${_STB_IMAGE}" _stb_image_size)
endif()
if(NOT EXISTS "${_STB_IMAGE}" OR _stb_image_size LESS 1000)
    file(DOWNLOAD
        "https://raw.githubusercontent.com/nothings/stb/013ac3beddff3dbffafd5177e7972067cd2b5083/stb_image.h"
        "${_STB_IMAGE}"
        STATUS _stb_dl_status)
    list(GET _stb_dl_status 0 _stb_dl_rc)
    if(NOT _stb_dl_rc EQUAL 0)
        message(FATAL_ERROR "Failed to download stb_image.h: ${_stb_dl_status}")
    endif()
endif()

# etc2tool executable
add_executable(etc2tool etc2tool.cpp)
target_link_libraries(etc2tool PRIVATE EtcLib)
target_include_directories(etc2tool PRIVATE
    ${etc2comp_SOURCE_DIR}/EtcLib/Etc
    ${etc2comp_SOURCE_DIR}/EtcLib/EtcCodec
    ${CMAKE_CURRENT_BINARY_DIR}
)
