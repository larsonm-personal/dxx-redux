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

# Patch SDL sources to register our Android OpenSL ES audio driver.
# Inject the bootstrap extern declaration into SDL_sysaudio.h
file(READ "${sdl12_SOURCE_DIR}/src/audio/SDL_sysaudio.h" _sysaudio_h)
string(FIND "${_sysaudio_h}" "SDL_AUDIO_DRIVER_ANDROID" _android_found)
if(_android_found EQUAL -1)
    string(REPLACE
        "#if SDL_AUDIO_DRIVER_DUMMY\nextern AudioBootStrap DUMMYAUD_bootstrap;"
        "#if SDL_AUDIO_DRIVER_ANDROID\nextern AudioBootStrap ANDROIDAUD_bootstrap;\n#endif\n#if SDL_AUDIO_DRIVER_DUMMY\nextern AudioBootStrap DUMMYAUD_bootstrap;"
        _sysaudio_h "${_sysaudio_h}")
    file(WRITE "${sdl12_SOURCE_DIR}/src/audio/SDL_sysaudio.h" "${_sysaudio_h}")
endif()

# Inject our bootstrap entry into SDL_audio.c's bootstrap[] array
file(READ "${sdl12_SOURCE_DIR}/src/audio/SDL_audio.c" _audio_c)
set(_audio_c_dirty FALSE)

string(FIND "${_audio_c}" "SDL_AUDIO_DRIVER_ANDROID" _android_found2)
if(_android_found2 EQUAL -1)
    string(REPLACE
        "#if SDL_AUDIO_DRIVER_DUMMY\n\t&DUMMYAUD_bootstrap,"
        "#if SDL_AUDIO_DRIVER_ANDROID\n\t&ANDROIDAUD_bootstrap,\n#endif\n#if SDL_AUDIO_DRIVER_DUMMY\n\t&DUMMYAUD_bootstrap,"
        _audio_c "${_audio_c}")
    set(_audio_c_dirty TRUE)
endif()

# Park SDL_RunAudio's thread on Android — audio is driven entirely by
# the OpenSL ES buffer-queue callback in SDL_androidaudio.c.
string(FIND "${_audio_c}" "Audio driven by OpenSL ES callback" _park_found)
if(_park_found EQUAL -1)
    string(REPLACE
        "\t/* Loop, filling the audio buffers */\n\twhile ( audio->enabled ) {"
        "\t/* Loop, filling the audio buffers */\n#ifdef __ANDROID__\n\t/* Audio driven by OpenSL ES callback — park this thread */\n\twhile ( audio->enabled ) { SDL_Delay(1000); }\n\treturn(0);\n#endif\n\twhile ( audio->enabled ) {"
        _audio_c "${_audio_c}")
    set(_audio_c_dirty TRUE)
endif()

if(_audio_c_dirty)
    file(WRITE "${sdl12_SOURCE_DIR}/src/audio/SDL_audio.c" "${_audio_c}")
endif()

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
    # --- Android OpenSL ES audio driver ---
    ${CMAKE_CURRENT_SOURCE_DIR}/SDL_androidaudio.c
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
            ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(SDL12 PRIVATE dl OpenSLES)
target_compile_options(SDL12 PRIVATE -w)   # suppress all warnings for third-party code

# ──────────────────────────────────────────────────────────────
# 1b. SDL_mixer 1.2  (SFX mixing only — MIDI handled by TSF)
# ──────────────────────────────────────────────────────────────
FetchContent_Declare(sdl_mixer12
    GIT_REPOSITORY https://github.com/arbruijn/SDL_mixer-1.2-cmake
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_Populate(sdl_mixer12)

set(SDLMIXER_DIR ${sdl_mixer12_SOURCE_DIR})
add_library(SDL_mixer STATIC
    # --- core mixer (SFX channels, effects, WAV loading) ---
    ${SDLMIXER_DIR}/mixer.c
    ${SDLMIXER_DIR}/music.c
    ${SDLMIXER_DIR}/wavestream.c
    ${SDLMIXER_DIR}/effect_position.c
    ${SDLMIXER_DIR}/effect_stereoreverse.c
    ${SDLMIXER_DIR}/effects_internal.c
    ${SDLMIXER_DIR}/load_aiff.c
    ${SDLMIXER_DIR}/load_voc.c
    # Timidity removed — MIDI is handled by TinySoundFont via Mix_HookMusic
)

target_include_directories(SDL_mixer
    PUBLIC  ${SDLMIXER_DIR}            # SDL_mixer.h
            ${SDL12_DIR}/include       # SDL headers
)

# WAV only — no MIDI, OGG, FLAC, MP3, FluidSynth, native MIDI
target_compile_definitions(SDL_mixer PRIVATE
    WAV_MUSIC
)

target_link_libraries(SDL_mixer PRIVATE SDL12)
target_compile_options(SDL_mixer PRIVATE -w)   # suppress warnings for third-party code

# ──────────────────────────────────────────────────────────────
# 2a. TinySoundFont  (tsf.h + tml.h — MIDI synth, header-only)
# ──────────────────────────────────────────────────────────────
set(_TSF_DIR "${CMAKE_CURRENT_BINARY_DIR}/tinysoundfont")
file(MAKE_DIRECTORY "${_TSF_DIR}")
if(NOT EXISTS "${_TSF_DIR}/tsf.h")
    file(DOWNLOAD
        "https://raw.githubusercontent.com/schellingb/TinySoundFont/main/tsf.h"
        "${_TSF_DIR}/tsf.h"
    )
endif()
if(NOT EXISTS "${_TSF_DIR}/tml.h")
    file(DOWNLOAD
        "https://raw.githubusercontent.com/schellingb/TinySoundFont/main/tml.h"
        "${_TSF_DIR}/tml.h"
    )
endif()

# ──────────────────────────────────────────────────────────────
# 2b. GM Soundfont — downloaded by Gradle (downloadSoundfont task)
#     into app/src/main/assets/gm.sf2 before the native build.
#     Version/hash defined in get_deps/tool_versions.conf.
# ──────────────────────────────────────────────────────────────

# ──────────────────────────────────────────────────────────────
# 2c. nlohmann/json  (header-only, debug/introspection builds only)
# ──────────────────────────────────────────────────────────────
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
set(JSON_BuildTests    OFF CACHE BOOL "" FORCE)
set(JSON_Install       OFF CACHE BOOL "" FORCE)
set(JSON_MultipleHeaders OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json)

# ──────────────────────────────────────────────────────────────
# 3. PhysFS  (has native cmake support)
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
# 4. Pre-set variables so find_package(SDL) / find_package(PhysFS)
#    in the D2 cmake files succeed without a system search.
# ──────────────────────────────────────────────────────────────
set(SDL_FOUND       TRUE                         CACHE BOOL   "" FORCE)
set(SDL_INCLUDE_DIR "${SDL12_DIR}/include"        CACHE PATH   "" FORCE)
set(SDL_LIBRARY     SDL12                         CACHE STRING "" FORCE)
set(SDLLIB          SDL12                         CACHE STRING "" FORCE)

set(PHYSFS_FOUND       TRUE                       CACHE BOOL   "" FORCE)
set(PHYSFS_INCLUDE_DIR "${physfs_SOURCE_DIR}/src"  CACHE PATH   "" FORCE)
set(PHYSFS_LIBRARY     physfs-static               CACHE STRING "" FORCE)

set(SDL_MIXER_FOUND        TRUE                    CACHE BOOL   "" FORCE)
set(SDL_MIXER_INCLUDE_DIRS "${SDLMIXER_DIR}"       CACHE PATH   "" FORCE)
set(SDL_MIXER_LIBRARY      SDL_mixer                CACHE STRING "" FORCE)

# ──────────────────────────────────────────────────────────────
# 5. D2 build options
# ──────────────────────────────────────────────────────────────
# ANDROID_USE_OPENGL is passed from Gradle (gradle.properties: useOpenGL)
if(ANDROID_USE_OPENGL STREQUAL "true")
    set(OPENGL     ON  CACHE BOOL "" FORCE)
else()
    set(OPENGL     OFF CACHE BOOL "" FORCE)
endif()
set(OPENGLMERGE OFF CACHE BOOL "" FORCE)   # no shader-based texmerge on GLES 1.1
set(SDLMIXER   ON  CACHE BOOL "" FORCE)
set(PNG        OFF CACHE BOOL "" FORCE)
set(ASM        OFF CACHE BOOL "" FORCE)
set(EDITOR     OFF CACHE BOOL "" FORCE)
set(UDP        ON  CACHE BOOL "" FORCE)
set(TRACKER    OFF CACHE BOOL "" FORCE)

if(OPENGL)
# Pre-set OpenGL/GLEW so find_package() in the D2 tree succeeds without
# a system search.  Android NDK provides EGL + GLESv1_CM natively.
set(OPENGL_FOUND      TRUE  CACHE BOOL   "" FORCE)
set(OPENGL_LIBRARY     ""   CACHE STRING "" FORCE)   # linked explicitly below
set(GLEW_FOUND         TRUE CACHE BOOL   "" FORCE)
set(GLEW_INCLUDE_DIRS  ""   CACHE PATH   "" FORCE)
set(GLEW_LIBRARIES     ""   CACHE STRING "" FORCE)
# Create a dummy GLEW::GLEW imported target so the D2 CMakeLists can reference it.
if(NOT TARGET GLEW::GLEW)
    add_library(GLEW::GLEW INTERFACE IMPORTED)
endif()

# Provide stub Find modules so find_package(OpenGL REQUIRED) and
# find_package(GLEW REQUIRED) in the D2 tree succeed on Android.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/cmake_modules/FindOpenGL.cmake"
     "set(OPENGL_FOUND TRUE)\nset(OPENGL_LIBRARY \"\")\nset(OPENGL_INCLUDE_DIR \"\")\n")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/cmake_modules/FindGLEW.cmake"
     "set(GLEW_FOUND TRUE)\nset(GLEW_INCLUDE_DIRS \"\")\nset(GLEW_LIBRARIES \"\")\nset(GLEW_INCLUDE_DIR \"\")\nif(NOT TARGET GLEW::GLEW)\n  add_library(GLEW::GLEW INTERFACE IMPORTED)\nendif()\n")
list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_CURRENT_BINARY_DIR}/cmake_modules")

# OGLES must be visible to ALL targets in the D2 tree (2d, 3d, main, etc.)
# so that ogl_init.h correctly guards <GL/glew.h>.
add_definitions(-DOGLES)
endif() # OPENGL

# ──────────────────────────────────────────────────────────────
# 6. Add D2 source tree
# ──────────────────────────────────────────────────────────────
add_subdirectory("${D2_SRC}" "${CMAKE_CURRENT_BINARY_DIR}/d2")

# ──────────────────────────────────────────────────────────────
# 7. Android-specific fixups for d2x-redux target
# ──────────────────────────────────────────────────────────────
target_link_libraries(d2x-redux PRIVATE android log)
if(OPENGL)
    target_link_libraries(d2x-redux PRIVATE EGL GLESv1_CM)
endif()
target_sources(d2x-redux PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/jni_main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/jni_saf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/android_surface.c
    ${CMAKE_CURRENT_SOURCE_DIR}/android_input.c
    ${CMAKE_CURRENT_SOURCE_DIR}/android_gamepad_config.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/extract/cue_parser.c
    ${CMAKE_CURRENT_SOURCE_DIR}/extract/iso9660_reader.c
    ${CMAKE_CURRENT_SOURCE_DIR}/extract/sow_extract.c
    ${CMAKE_CURRENT_SOURCE_DIR}/extract/jni_disc_import.c
    ${CMAKE_CURRENT_SOURCE_DIR}/jni_music_control.c
    ${D2_SRC}/arch/android/messagebox.c
    ${D2_SRC}/arch/android/physfs_archiver_saf.c
    ${D2_SRC}/introspect/game_introspect.cpp
    ${D2_SRC}/introspect/game_automate.cpp
)
# android_surface.h lives in d2/arch/include (already on arch_sdl's include
# path); make sure it's also visible when compiling android_surface.c itself.
# game.h (for Game_wind) lives in d2/main and pulls in headers from d2/include.
target_include_directories(d2x-redux PRIVATE
    ${D2_SRC}/arch/include
    ${D2_SRC}/main
    ${D2_SRC}/include
    ${D2_SRC}/introspect
    ${CMAKE_CURRENT_SOURCE_DIR}/extract
)

# Enable introspection API (AI-assisted testing, debug Android builds only).
# The Kotlin layer gates adb access behind BuildConfig.DEBUG.
# Must also be set on arch_sdl since window.c and event.c live there.
target_compile_definitions(d2x-redux PRIVATE INTROSPECT_ON)
target_compile_definitions(arch_sdl PRIVATE INTROSPECT_ON)
target_include_directories(arch_sdl PRIVATE ${D2_SRC}/introspect)

# Replace SDL_mixer's Timidity-based music with TinySoundFont MIDI synth.
# digi_mixer_music.c is already compiled into arch_sdl by the D2 CMakeLists;
# we remove it and add our TSF-based replacement instead.
get_target_property(_arch_sdl_sources arch_sdl SOURCES)
list(FILTER _arch_sdl_sources EXCLUDE REGEX "digi_mixer_music\\.c$")
list(FILTER _arch_sdl_sources EXCLUDE REGEX "rbaudio\\.c$")
set_property(TARGET arch_sdl PROPERTY SOURCES ${_arch_sdl_sources})
target_sources(arch_sdl PRIVATE ${D2_SRC}/arch/sdl/digi_tsf_music.c)
target_sources(arch_sdl PRIVATE ${D2_SRC}/arch/sdl/rbaudio_bin.c)
target_include_directories(arch_sdl PRIVATE ${_TSF_DIR})
target_compile_definitions(arch_sdl PRIVATE ANDROID)

# OpenGL ES 1.1: link EGL + GLESv1_CM into the arch_ogl static library,
# and add Android-specific include paths.
if(OPENGL)
target_compile_definitions(arch_ogl PRIVATE ANDROID)
target_link_libraries(arch_ogl PRIVATE EGL GLESv1_CM)
target_include_directories(arch_ogl PRIVATE
    ${D2_SRC}/arch/include
    ${D2_SRC}/main
    ${D2_SRC}/include
)

# xmodel needs the PhysFS include path (not set by its own CMakeLists.txt).
target_include_directories(xmodel PRIVATE ${PHYSFS_INCLUDE_DIR})
endif() # OPENGL

# nlohmann/json — used by the introspection/automation C++ files
target_link_libraries(d2x-redux PRIVATE nlohmann_json::nlohmann_json)

# sow_extract.c (in extract/) has a self-contained ARJ decoder — no external library needed
