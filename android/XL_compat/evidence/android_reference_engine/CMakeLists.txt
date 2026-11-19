cmake_minimum_required(VERSION 3.20)
project(xl_android_reference LANGUAGES C CXX)
if(NOT ANDROID)
    message(FATAL_ERROR "Configure with an Android NDK toolchain")
endif()
set(BUILD_SHARED_LIBS OFF)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_LIBCURL_DOCS OFF CACHE BOOL "" FORCE)
set(BUILD_MISC_DOCS OFF CACHE BOOL "" FORCE)
set(ENABLE_CURL_MANUAL OFF CACHE BOOL "" FORCE)
set(CURL_USE_OPENSSL OFF CACHE BOOL "" FORCE)
set(CURL_USE_LIBPSL OFF CACHE BOOL "" FORCE)
set(CURL_DISABLE_LDAP ON CACHE BOOL "" FORCE)
set(CURL_DISABLE_LDAPS ON CACHE BOOL "" FORCE)
add_subdirectory("${XL_DEPS}/dep-curl" curl-build EXCLUDE_FROM_ALL)
set(PNG_SHARED OFF CACHE BOOL "" FORCE)
set(PNG_TESTS OFF CACHE BOOL "" FORCE)
set(PNG_TOOLS OFF CACHE BOOL "" FORCE)
add_subdirectory("${XL_PNG}" png-build EXCLUDE_FROM_ALL)
add_library(SDL12 SHARED IMPORTED)
set_target_properties(SDL12 PROPERTIES IMPORTED_LOCATION "${XL_SDL_LIBRARY}")
add_library(GL4ES SHARED IMPORTED)
set_target_properties(GL4ES PROPERTIES IMPORTED_LOCATION "${XL_GL_LIBRARY}")
add_subdirectory("${XL_ORIGINAL}" original-build)
add_library(glew STATIC "${XL_GLEW}/src/glew.c")
target_compile_definitions(glew PRIVATE GLEW_EGL GLEW_NO_GLU GLEW_STATIC)
target_include_directories(glew PRIVATE "${XL_GLEW}/include" "${XL_GL_SOURCE}/include")
add_library(glu_projection STATIC "${XL_DEPS}/dep-glu/src/libutil/project.c")
target_include_directories(glu_projection PRIVATE "${XL_DEPS}/dep-glu/src/include"
                                                  "${XL_GL_SOURCE}/include")
set(mixer "${XL_DEPS}/dep-SDL_mixer")
add_library(
    sdl_mixer STATIC
    "${mixer}/mixer.c"
    "${mixer}/music.c"
    "${mixer}/wavestream.c"
    "${mixer}/effects_internal.c"
    "${mixer}/effect_position.c"
    "${mixer}/effect_stereoreverse.c"
    "${mixer}/load_aiff.c"
    "${mixer}/load_voc.c"
    "${mixer}/load_ogg.c"
    "${mixer}/load_flac.c")
target_compile_definitions(sdl_mixer PRIVATE WAV_MUSIC)
file(GLOB img "${XL_DEPS}/dep-SDL_image/IMG*.c")
add_library(sdl_img STATIC ${img})
target_compile_definitions(sdl_img PRIVATE LOAD_BMP LOAD_TGA LOAD_PNG LOAD_PCX LOAD_PNM LOAD_XPM)
target_link_libraries(sdl_img PRIVATE png_static)
set(net "${XL_DEPS}/dep-SDL_net")
add_library(sdl_net STATIC "${net}/SDLnet.c" "${net}/SDLnetTCP.c" "${net}/SDLnetUDP.c"
                           "${net}/SDLnetselect.c")
foreach(lib sdl_net sdl_img sdl_mixer)
    target_include_directories(${lib} PRIVATE "${XL_HEADERS}/SDL")
    target_link_libraries(${lib} PUBLIC SDL12)
endforeach()
add_executable(xl_original egl_sdl.cpp)
target_link_libraries(
    xl_original
    PRIVATE xl_original_core
            sdl_mixer
            sdl_img
            sdl_net
            libcurl_static
            glu_projection
            glew
            GL4ES
            EGL
            GLESv2
            android
            log
            z
            dl)
set_target_properties(xl_original PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE INSTALL_RPATH "$ORIGIN")
target_link_options(xl_original PRIVATE [[LINKER:-rpath,$ORIGIN]])
target_include_directories(xl_original PRIVATE "${XL_HEADERS}/SDL")
foreach(entry SDL_ListModes SDL_SetVideoMode SDL_VideoModeOK SDL_GL_GetProcAddress
              SDL_GL_SwapBuffers eglGetProcAddress)
    target_link_options(xl_original PRIVATE "LINKER:--wrap=${entry}")
endforeach()
