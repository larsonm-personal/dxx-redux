# Android native build for DXX-Redux (Descent 2)
#
# Builds the full D2 game as a shared library for Android.
# Downloads SDL 1.2 and PhysFS via FetchContent at configure time.

cmake_minimum_required(VERSION 3.22)
project(dxx-redux-android LANGUAGES C CXX)

# CMake 4.x policy compat
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)   # use extraction timestamp for FetchContent
endif()
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)   # allow FetchContent_Populate (SDL has no cmake)
endif()
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)   # PhysFS 3.2 compat

include(FetchContent)

set(D2_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../d2")

# ──────────────────────────────────────────────────────────────
# 1. SDL 1.2  (no cmake build — we compile it ourselves)
# ──────────────────────────────────────────────────────────────
FetchContent_Declare(sdl12
    URL https://github.com/libsdl-org/SDL-1.2/archive/refs/tags/release-1.2.15.tar.gz
)
FetchContent_Populate(sdl12)

# Install our Android-specific SDL_config.h
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/SDL_config_android.h
    ${sdl12_SOURCE_DIR}/include/SDL_config.h
    COPYONLY
)

set(SDL12_DIR ${sdl12_SOURCE_DIR})
add_library(SDL12 STATIC
    # --- core ---
    ${SDL12_DIR}/src/SDL.c
    ${SDL12_DIR}/src/SDL_error.c
    ${SDL12_DIR}/src/SDL_fatal.c
    # --- audio ---
    ${SDL12_DIR}/src/audio/SDL_audio.c
    ${SDL12_DIR}/src/audio/SDL_audiocvt.c
    ${SDL12_DIR}/src/audio/SDL_audiodev.c
    ${SDL12_DIR}/src/audio/SDL_mixer.c
    ${SDL12_DIR}/src/audio/SDL_wave.c
    ${SDL12_DIR}/src/audio/dummy/SDL_dummyaudio.c
    # --- cdrom ---
    ${SDL12_DIR}/src/cdrom/SDL_cdrom.c
    ${SDL12_DIR}/src/cdrom/dummy/SDL_syscdrom.c
    # --- cpuinfo ---
    ${SDL12_DIR}/src/cpuinfo/SDL_cpuinfo.c
    # --- events ---
    ${SDL12_DIR}/src/events/SDL_active.c
    ${SDL12_DIR}/src/events/SDL_events.c
    ${SDL12_DIR}/src/events/SDL_expose.c
    ${SDL12_DIR}/src/events/SDL_keyboard.c
    ${SDL12_DIR}/src/events/SDL_mouse.c
    ${SDL12_DIR}/src/events/SDL_quit.c
    ${SDL12_DIR}/src/events/SDL_resize.c
    # --- file ---
    ${SDL12_DIR}/src/file/SDL_rwops.c
    # --- joystick ---
    ${SDL12_DIR}/src/joystick/SDL_joystick.c
    ${SDL12_DIR}/src/joystick/dummy/SDL_sysjoystick.c
    # --- loadso ---
    ${SDL12_DIR}/src/loadso/dlopen/SDL_sysloadso.c
    # --- stdlib ---
    ${SDL12_DIR}/src/stdlib/SDL_getenv.c
    ${SDL12_DIR}/src/stdlib/SDL_iconv.c
    ${SDL12_DIR}/src/stdlib/SDL_malloc.c
    ${SDL12_DIR}/src/stdlib/SDL_qsort.c
    ${SDL12_DIR}/src/stdlib/SDL_stdlib.c
    ${SDL12_DIR}/src/stdlib/SDL_string.c
    # --- thread ---
    ${SDL12_DIR}/src/thread/SDL_thread.c
    ${SDL12_DIR}/src/thread/pthread/SDL_syscond.c
    ${SDL12_DIR}/src/thread/pthread/SDL_sysmutex.c
    ${SDL12_DIR}/src/thread/pthread/SDL_syssem.c
    ${SDL12_DIR}/src/thread/pthread/SDL_systhread.c
    # --- timer ---
    ${SDL12_DIR}/src/timer/SDL_timer.c
    ${SDL12_DIR}/src/timer/unix/SDL_systimer.c
    # --- video ---
    ${SDL12_DIR}/src/video/SDL_blit.c
    ${SDL12_DIR}/src/video/SDL_blit_0.c
    ${SDL12_DIR}/src/video/SDL_blit_1.c
    ${SDL12_DIR}/src/video/SDL_blit_A.c
    ${SDL12_DIR}/src/video/SDL_blit_N.c
    ${SDL12_DIR}/src/video/SDL_bmp.c
    ${SDL12_DIR}/src/video/SDL_cursor.c
    ${SDL12_DIR}/src/video/SDL_gamma.c
    ${SDL12_DIR}/src/video/SDL_pixels.c
    ${SDL12_DIR}/src/video/SDL_RLEaccel.c
    ${SDL12_DIR}/src/video/SDL_stretch.c
    ${SDL12_DIR}/src/video/SDL_surface.c
    ${SDL12_DIR}/src/video/SDL_video.c
    ${SDL12_DIR}/src/video/SDL_yuv.c
    ${SDL12_DIR}/src/video/SDL_yuv_sw.c
    ${SDL12_DIR}/src/video/dummy/SDL_nullevents.c
    ${SDL12_DIR}/src/video/dummy/SDL_nullmouse.c
    ${SDL12_DIR}/src/video/dummy/SDL_nullvideo.c
)

target_include_directories(SDL12
    PUBLIC  ${SDL12_DIR}/include
    PRIVATE ${SDL12_DIR}/src
            ${SDL12_DIR}/src/audio
            ${SDL12_DIR}/src/video
            ${SDL12_DIR}/src/video/dummy
            ${SDL12_DIR}/src/events
            ${SDL12_DIR}/src/joystick
            ${SDL12_DIR}/src/cdrom
            ${SDL12_DIR}/src/thread
            ${SDL12_DIR}/src/timer
            ${SDL12_DIR}/src/file
            ${SDL12_DIR}/src/loadso
            ${SDL12_DIR}/src/loadso/dlopen
)

target_link_libraries(SDL12 PRIVATE dl)
target_compile_options(SDL12 PRIVATE -w)   # suppress all warnings for third-party code

# ──────────────────────────────────────────────────────────────
# 2. PhysFS  (has native cmake support)
# ──────────────────────────────────────────────────────────────
set(PHYSFS_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(PHYSFS_BUILD_TEST   OFF CACHE BOOL "" FORCE)
set(PHYSFS_BUILD_DOCS   OFF CACHE BOOL "" FORCE)
set(PHYSFS_TARGETNAME_UNINSTALL "physfs_uninstall" CACHE STRING "" FORCE)

FetchContent_Declare(physfs
    URL https://github.com/icculus/physfs/archive/refs/tags/release-3.2.0.tar.gz
)
FetchContent_MakeAvailable(physfs)

# ──────────────────────────────────────────────────────────────
# 3. Pre-set variables so find_package(SDL) / find_package(PhysFS)
#    in the D2 cmake files succeed without a system search.
# ──────────────────────────────────────────────────────────────
set(SDL_FOUND       TRUE                         CACHE BOOL   "" FORCE)
set(SDL_INCLUDE_DIR "${SDL12_DIR}/include"        CACHE PATH   "" FORCE)
set(SDL_LIBRARY     SDL12                         CACHE STRING "" FORCE)
set(SDLLIB          SDL12                         CACHE STRING "" FORCE)

set(PHYSFS_FOUND       TRUE                       CACHE BOOL   "" FORCE)
set(PHYSFS_INCLUDE_DIR "${physfs_SOURCE_DIR}/src"  CACHE PATH   "" FORCE)
set(PHYSFS_LIBRARY     physfs-static               CACHE STRING "" FORCE)

# ──────────────────────────────────────────────────────────────
# 4. D2 build options  (disable everything that needs extra deps)
# ──────────────────────────────────────────────────────────────
set(OPENGL    OFF CACHE BOOL "" FORCE)
set(SDLMIXER  OFF CACHE BOOL "" FORCE)
set(PNG       OFF CACHE BOOL "" FORCE)
set(ASM       OFF CACHE BOOL "" FORCE)
set(EDITOR    OFF CACHE BOOL "" FORCE)
set(UDP       ON  CACHE BOOL "" FORCE)
set(TRACKER   OFF CACHE BOOL "" FORCE)

# ──────────────────────────────────────────────────────────────
# 5. Add D2 source tree
# ──────────────────────────────────────────────────────────────
add_subdirectory("${D2_SRC}" "${CMAKE_CURRENT_BINARY_DIR}/d2")

# ──────────────────────────────────────────────────────────────
# 6. Android-specific fixups for d2x-redux target
# ──────────────────────────────────────────────────────────────
target_link_libraries(d2x-redux PRIVATE android log)
target_sources(d2x-redux PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/jni_main.c
    ${D2_SRC}/arch/android/messagebox.c
)

# TODO: Replace the above with the real d1/d2 build by including the game
# CMakeLists.txt and adjusting dependencies for Android (OpenGL ES, SDL2, etc.)
