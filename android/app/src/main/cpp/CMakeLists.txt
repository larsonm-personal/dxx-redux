# Android native build for DXX-Redux (Descent 2)
#
# Builds the full D2 game as a shared library for Android. Downloads SDL 1.2 and PhysFS via
# FetchContent at configure time.

cmake_minimum_required(VERSION 3.22)
project(dxx-redux-android LANGUAGES C CXX)

# CMake 4.x policy compat
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW) # use extraction timestamp for FetchContent
endif()
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD) # allow FetchContent_Populate (SDL has no cmake)
endif()
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE) # PhysFS 3.2 compat

include(FetchContent)
include(${CMAKE_CURRENT_LIST_DIR}/../../../../../cmake/input-demo-codec-deps.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/../../../../../cmake/input-demo-build-metadata.cmake)

# ──────────────────────────────────────────────────────────────
# Compiler warnings
# ──────────────────────────────────────────────────────────────
add_compile_options(
    -Wall
    -Wextra
    -Wformat=2
    -Wno-unused-parameter # too pervasive in callback-heavy legacy C
    -Wno-missing-field-initializers # partial struct init is idiomatic C
    -Wno-sign-compare # pervasive int/unsigned mixing in game code
    # d1/d2 legacy-code noise (thousands of hits, unfixable without rewriting)
    -Wno-ignored-qualifiers
    -Wno-format-nonliteral
    -Wno-self-assign
    -Wno-misleading-indentation
    -Wno-macro-redefined
    -Wno-unused-but-set-variable
    -Wno-string-plus-int
    -Wno-logical-op-parentheses
    -Wno-logical-not-parentheses
    -Wno-unknown-attributes)

set(D1_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../d1")
set(D2_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../d2")
set(DXX_DISC_IMPORT_JNI_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/extract/jni_disc_import.c")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/jni_disc_import.c")
    message(FATAL_ERROR "DiscImportBridge JNI must have only the canonical extract implementation")
endif()

# ──────────────────────────────────────────────────────────────
# 1. SDL 1.2  (no cmake build — we compile it ourselves)
# ──────────────────────────────────────────────────────────────
FetchContent_Declare(
    sdl12 URL https://github.com/libsdl-org/SDL-1.2/archive/refs/tags/release-1.2.15.tar.gz)
FetchContent_Populate(sdl12)

# Install our Android-specific SDL_config.h
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/SDL_config_android.h
               ${sdl12_SOURCE_DIR}/include/SDL_config.h COPYONLY)

# Patch SDL sources to register our Android OpenSL ES audio driver. Inject the bootstrap extern
# declaration into SDL_sysaudio.h
file(READ "${sdl12_SOURCE_DIR}/src/audio/SDL_sysaudio.h" _sysaudio_h)
string(FIND "${_sysaudio_h}" "SDL_AUDIO_DRIVER_ANDROID" _android_found)
if(_android_found EQUAL -1)
    string(
        REPLACE
            "#if SDL_AUDIO_DRIVER_DUMMY\nextern AudioBootStrap DUMMYAUD_bootstrap;"
            "#if SDL_AUDIO_DRIVER_ANDROID\nextern AudioBootStrap ANDROIDAUD_bootstrap;\n#endif\n#if SDL_AUDIO_DRIVER_DUMMY\nextern AudioBootStrap DUMMYAUD_bootstrap;"
            _sysaudio_h
            "${_sysaudio_h}")
    file(WRITE "${sdl12_SOURCE_DIR}/src/audio/SDL_sysaudio.h" "${_sysaudio_h}")
endif()

# Inject our bootstrap entry into SDL_audio.c's bootstrap[] array
file(READ "${sdl12_SOURCE_DIR}/src/audio/SDL_audio.c" _audio_c)
set(_audio_c_dirty FALSE)

string(FIND "${_audio_c}" "SDL_AUDIO_DRIVER_ANDROID" _android_found2)
if(_android_found2 EQUAL -1)
    string(
        REPLACE
            "#if SDL_AUDIO_DRIVER_DUMMY\n\t&DUMMYAUD_bootstrap,"
            "#if SDL_AUDIO_DRIVER_ANDROID\n\t&ANDROIDAUD_bootstrap,\n#endif\n#if SDL_AUDIO_DRIVER_DUMMY\n\t&DUMMYAUD_bootstrap,"
            _audio_c
            "${_audio_c}")
    set(_audio_c_dirty TRUE)
endif()

# Park SDL_RunAudio's thread on Android — audio is driven entirely by the OpenSL ES buffer-queue
# callback in SDL_androidaudio.c.
string(FIND "${_audio_c}" "Audio driven by OpenSL ES callback" _park_found)
if(_park_found EQUAL -1)
    string(
        REPLACE
            "\t/* Loop, filling the audio buffers */\n\twhile ( audio->enabled ) {"
            "\t/* Loop, filling the audio buffers */\n#ifdef __ANDROID__\n\t/* Audio driven by OpenSL ES callback — park this thread */\n\twhile ( audio->enabled ) { SDL_Delay(1000); }\n\treturn(0);\n#endif\n\twhile ( audio->enabled ) {"
            _audio_c
            "${_audio_c}")
    set(_audio_c_dirty TRUE)
endif()

if(_audio_c_dirty)
    file(WRITE "${sdl12_SOURCE_DIR}/src/audio/SDL_audio.c" "${_audio_c}")
endif()

set(SDL12_DIR ${sdl12_SOURCE_DIR})
add_library(
    SDL12 SHARED
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
    ${SDL12_DIR}/src/video/dummy/SDL_nullvideo.c)

target_include_directories(
    SDL12
    PUBLIC ${SDL12_DIR}/include
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
            ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(SDL12 PRIVATE dl OpenSLES log)
target_compile_options(SDL12 PRIVATE -w) # suppress all warnings for third-party code

# ──────────────────────────────────────────────────────────────
# 1b. SDL_mixer 1.2  (SFX mixing only — MIDI handled by TSF)
# ──────────────────────────────────────────────────────────────
FetchContent_Declare(sdl_mixer12 GIT_REPOSITORY https://github.com/arbruijn/SDL_mixer-1.2-cmake
                     GIT_TAG master GIT_SHALLOW TRUE)
FetchContent_Populate(sdl_mixer12)

set(SDLMIXER_DIR ${sdl_mixer12_SOURCE_DIR})
add_library(
    SDL_mixer SHARED
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

target_include_directories(
    SDL_mixer PUBLIC ${SDLMIXER_DIR} # SDL_mixer.h
                     ${SDL12_DIR}/include # SDL headers
)

# WAV only — no MIDI, OGG, FLAC, MP3, FluidSynth, native MIDI
target_compile_definitions(SDL_mixer PRIVATE WAV_MUSIC)

target_link_libraries(SDL_mixer PRIVATE SDL12)
target_compile_options(SDL_mixer PRIVATE -w) # suppress warnings for third-party code

# ──────────────────────────────────────────────────────────────
# 2a. TinySoundFont  (tsf.h + tml.h — MIDI synth, header-only)
# ──────────────────────────────────────────────────────────────
set(_TSF_DIR "${CMAKE_CURRENT_BINARY_DIR}/tinysoundfont")
file(MAKE_DIRECTORY "${_TSF_DIR}")
if(NOT EXISTS "${_TSF_DIR}/tsf.h")
    file(DOWNLOAD "https://raw.githubusercontent.com/schellingb/TinySoundFont/main/tsf.h"
         "${_TSF_DIR}/tsf.h")
endif()
if(NOT EXISTS "${_TSF_DIR}/tml.h")
    file(DOWNLOAD "https://raw.githubusercontent.com/schellingb/TinySoundFont/main/tml.h"
         "${_TSF_DIR}/tml.h")
endif()

# ──────────────────────────────────────────────────────────────
# 2b. GM Soundfont — downloaded by Gradle (downloadSoundfont task) into app/src/main/assets/gm.sf2
# before the native build. Version/hash defined in get_deps/tool_versions.conf.
# ──────────────────────────────────────────────────────────────

# ──────────────────────────────────────────────────────────────
# 2c. nlohmann/json  (header-only, debug/introspection builds only)
# ──────────────────────────────────────────────────────────────
FetchContent_Declare(nlohmann_json GIT_REPOSITORY https://github.com/nlohmann/json.git
                     GIT_TAG v3.11.3 GIT_SHALLOW TRUE)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install OFF CACHE BOOL "" FORCE)
set(JSON_MultipleHeaders OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json)

# ──────────────────────────────────────────────────────────────
# 1. PhysFS  (has native cmake support)
# ──────────────────────────────────────────────────────────────
set(PHYSFS_BUILD_SHARED ON CACHE BOOL "" FORCE)
set(PHYSFS_BUILD_STATIC OFF CACHE BOOL "" FORCE)
set(PHYSFS_BUILD_TEST OFF CACHE BOOL "" FORCE)
set(PHYSFS_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(PHYSFS_TARGETNAME_UNINSTALL "physfs_uninstall" CACHE STRING "" FORCE)

FetchContent_Declare(physfs
                     URL https://github.com/icculus/physfs/archive/refs/tags/release-3.2.0.tar.gz)
FetchContent_MakeAvailable(physfs)

# ──────────────────────────────────────────────────────────────
# 3b. LZMA SDK  (for InnoSetup/GOG decompression)
# ──────────────────────────────────────────────────────────────
FetchContent_Declare(
    lzma_sdk GIT_REPOSITORY https://github.com/jljusten/LZMA-SDK.git
    GIT_TAG 781863cdf592da3e97420f50de5dac056ad352a5 # 18.05
    GIT_SHALLOW TRUE)
FetchContent_GetProperties(lzma_sdk)
if(NOT lzma_sdk_POPULATED)
    FetchContent_Populate(lzma_sdk)
endif()
add_library(
    lzma_sdk_c SHARED
    ${lzma_sdk_SOURCE_DIR}/C/LzmaDec.c ${lzma_sdk_SOURCE_DIR}/C/Lzma2Dec.c
    ${lzma_sdk_SOURCE_DIR}/C/7zCrc.c ${lzma_sdk_SOURCE_DIR}/C/7zCrcOpt.c
    ${lzma_sdk_SOURCE_DIR}/C/CpuArch.c)
target_include_directories(lzma_sdk_c PUBLIC ${lzma_sdk_SOURCE_DIR}/C)
target_compile_definitions(lzma_sdk_c PRIVATE _7ZIP_ST)

# ──────────────────────────────────────────────────────────────
# 3b-ii. Chromaprint  (audio fingerprinting, uses bundled KissFFT)
# ──────────────────────────────────────────────────────────────
FetchContent_Declare(chromaprint
                     URL https://github.com/acoustid/chromaprint/archive/refs/tags/v1.5.1.tar.gz)
FetchContent_GetProperties(chromaprint)
if(NOT chromaprint_POPULATED)
    FetchContent_Populate(chromaprint)
endif()
# Build core Chromaprint sources + bundled KissFFT (no FFTW needed)
set(_CP_SRC ${chromaprint_SOURCE_DIR}/src)
add_library(
    chromaprint SHARED
    ${_CP_SRC}/chromaprint.cpp
    ${_CP_SRC}/audio_processor.cpp
    ${_CP_SRC}/chroma.cpp
    ${_CP_SRC}/chroma_filter.cpp
    ${_CP_SRC}/chroma_resampler.cpp
    ${_CP_SRC}/fft.cpp
    ${_CP_SRC}/fft_lib_kissfft.cpp
    ${_CP_SRC}/fingerprint_calculator.cpp
    ${_CP_SRC}/fingerprint_compressor.cpp
    ${_CP_SRC}/fingerprint_decompressor.cpp
    ${_CP_SRC}/fingerprint_matcher.cpp
    ${_CP_SRC}/fingerprinter.cpp
    ${_CP_SRC}/fingerprinter_configuration.cpp
    ${_CP_SRC}/image_builder.cpp
    ${_CP_SRC}/silence_remover.cpp
    ${_CP_SRC}/simhash.cpp
    ${_CP_SRC}/spectrum.cpp
    ${_CP_SRC}/utils/base64.cpp
    # KissFFT (bundled with Chromaprint)
    ${chromaprint_SOURCE_DIR}/src/3rdparty/kissfft/kiss_fft.c
    ${chromaprint_SOURCE_DIR}/src/3rdparty/kissfft/tools/kiss_fftr.c
    # Bundled avresample (resampler from libav, included with Chromaprint)
    ${chromaprint_SOURCE_DIR}/src/avresample/resample2.c)
# Generate config.h that Chromaprint expects
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/chromaprint_config/config.h"
     "#ifndef CHROMAPRINT_CONFIG_H\n" "#define CHROMAPRINT_CONFIG_H\n" "#define HAVE_ROUND 1\n"
     "#define USE_KISSFFT 1\n" "#endif\n")
target_include_directories(
    chromaprint
    PUBLIC ${chromaprint_SOURCE_DIR}/src
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/chromaprint_config
            ${chromaprint_SOURCE_DIR}/src/3rdparty/kissfft
            ${chromaprint_SOURCE_DIR}/src/3rdparty/kissfft/tools)
target_compile_definitions(chromaprint PRIVATE HAVE_CONFIG_H)
target_compile_options(chromaprint PRIVATE -w)

# ──────────────────────────────────────────────────────────────
# 3b-iii. Single-file PCM decoders  (minimp3, stb_vorbis, dr_flac)
# ──────────────────────────────────────────────────────────────
set(_DECODERS_DIR "${CMAKE_CURRENT_BINARY_DIR}/pcm_decoders")
file(MAKE_DIRECTORY "${_DECODERS_DIR}")

if(NOT EXISTS "${_DECODERS_DIR}/minimp3.h")
    file(
        DOWNLOAD
        "https://raw.githubusercontent.com/lieff/minimp3/afb604c06bc8beb145fecd42c0ceb5bda8795144/minimp3.h"
        "${_DECODERS_DIR}/minimp3.h")
endif()
if(NOT EXISTS "${_DECODERS_DIR}/minimp3_ex.h")
    file(
        DOWNLOAD
        "https://raw.githubusercontent.com/lieff/minimp3/afb604c06bc8beb145fecd42c0ceb5bda8795144/minimp3_ex.h"
        "${_DECODERS_DIR}/minimp3_ex.h")
endif()
set(_stb_vorbis_need_dl TRUE)
if(EXISTS "${_DECODERS_DIR}/stb_vorbis.c")
    file(SIZE "${_DECODERS_DIR}/stb_vorbis.c" _stb_vorbis_size)
    if(_stb_vorbis_size GREATER 0)
        set(_stb_vorbis_need_dl FALSE)
    endif()
endif()
if(_stb_vorbis_need_dl)
    file(
        DOWNLOAD
        "https://raw.githubusercontent.com/nothings/stb/1ee679ca2ef753a528db5ba6801e1067b40481b8/stb_vorbis.c"
        "${_DECODERS_DIR}/stb_vorbis.c"
        STATUS _stb_dl_status)
    list(GET _stb_dl_status 0 _stb_dl_code)
    if(NOT _stb_dl_code EQUAL 0)
        message(FATAL_ERROR "Failed to download stb_vorbis.c: ${_stb_dl_status}")
    endif()
endif()
if(NOT EXISTS "${_DECODERS_DIR}/dr_flac.h")
    file(
        DOWNLOAD
        "https://raw.githubusercontent.com/mackron/dr_libs/da35f9d6c7374a95353fd1df1d394d44ab66cf01/dr_flac.h"
        "${_DECODERS_DIR}/dr_flac.h")
endif()
set(_stb_image_need_dl TRUE)
if(EXISTS "${_DECODERS_DIR}/stb_image.h")
    file(SIZE "${_DECODERS_DIR}/stb_image.h" _stb_image_size)
    if(_stb_image_size GREATER 0)
        set(_stb_image_need_dl FALSE)
    endif()
endif()
if(_stb_image_need_dl)
    file(
        DOWNLOAD
        "https://raw.githubusercontent.com/nothings/stb/013ac3beddff3dbffafd5177e7972067cd2b5083/stb_image.h"
        "${_DECODERS_DIR}/stb_image.h"
        STATUS _stb_img_dl_status)
    list(GET _stb_img_dl_status 0 _stb_img_dl_code)
    if(NOT _stb_img_dl_code EQUAL 0)
        message(FATAL_ERROR "Failed to download stb_image.h: ${_stb_img_dl_status}")
    endif()
endif()

# ──────────────────────────────────────────────────────────────
# 3b-iv. KTX-Software  (KTX2 texture container reader, read-only static)
# ──────────────────────────────────────────────────────────────
set(KTX_FEATURE_STATIC_LIBRARY ON CACHE BOOL "" FORCE)
set(KTX_FEATURE_TOOLS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_TESTS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_DOC OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_JNI OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_GL_UPLOAD OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_VK_UPLOAD OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_LOADTEST_APPS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(ktx_software GIT_REPOSITORY https://github.com/KhronosGroup/KTX-Software.git
                     GIT_TAG v4.4.2 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(ktx_software)

# ──────────────────────────────────────────────────────────────
# 3c. TinySoundFont shared library  (MIDI synth, linked by both games)
# ──────────────────────────────────────────────────────────────
add_library(tsf SHARED ${CMAKE_CURRENT_SOURCE_DIR}/shared/tsf_impl.c)
target_include_directories(tsf PRIVATE ${_TSF_DIR})
target_compile_options(tsf PRIVATE -w)

# ──────────────────────────────────────────────────────────────
# 3d. PCM decoders + fingerprinting shared library
# ──────────────────────────────────────────────────────────────
add_library(
    dxx_fingerprint SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/pcm_decoders.c
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/chromaprint_db.c
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/fingerprint_gen.c ${_DECODERS_DIR}/stb_vorbis.c)
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/shared/chromaprint_db.c PROPERTIES LANGUAGE
                                                                                           CXX)
target_include_directories(dxx_fingerprint PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/shared
                           PRIVATE ${_DECODERS_DIR} ${chromaprint_SOURCE_DIR}/src)
target_compile_options(dxx_fingerprint PRIVATE -w) # suppress warnings in vendored decoder code
target_link_libraries(dxx_fingerprint PRIVATE chromaprint log nlohmann_json::nlohmann_json)

# ──────────────────────────────────────────────────────────────
# 1. Pre-set variables so find_package(SDL) / find_package(PhysFS) in the D2 cmake files succeed
#   without a system search.
# ──────────────────────────────────────────────────────────────
set(SDL_FOUND TRUE CACHE BOOL "" FORCE)
set(SDL_INCLUDE_DIR "${SDL12_DIR}/include" CACHE PATH "" FORCE)
set(SDL_LIBRARY SDL12 CACHE STRING "" FORCE)
set(SDLLIB SDL12 CACHE STRING "" FORCE)

set(PHYSFS_FOUND TRUE CACHE BOOL "" FORCE)
set(PHYSFS_INCLUDE_DIR "${physfs_SOURCE_DIR}/src" CACHE PATH "" FORCE)
set(PHYSFS_LIBRARY physfs CACHE STRING "" FORCE)

set(SDL_MIXER_FOUND TRUE CACHE BOOL "" FORCE)
set(SDL_MIXER_INCLUDE_DIRS "${SDLMIXER_DIR}" CACHE PATH "" FORCE)
set(SDL_MIXER_LIBRARY SDL_mixer CACHE STRING "" FORCE)

# ──────────────────────────────────────────────────────────────
# 1. Build options
# ──────────────────────────────────────────────────────────────
# ANDROID_USE_OPENGL is passed from Gradle (gradle.properties: useOpenGL)
if(ANDROID_USE_OPENGL STREQUAL "true")
    set(OPENGL ON CACHE BOOL "" FORCE)
else()
    set(OPENGL OFF CACHE BOOL "" FORCE)
endif()
set(OPENGLMERGE ON CACHE BOOL "" FORCE) # shader-based texmerge for super-transparent
set(SDLMIXER ON CACHE BOOL "" FORCE)
set(PNG OFF CACHE BOOL "" FORCE)
set(ASM OFF CACHE BOOL "" FORCE)
set(EDITOR OFF CACHE BOOL "" FORCE)
set(UDP ON CACHE BOOL "" FORCE)
set(TRACKER OFF CACHE BOOL "" FORCE)

if(OPENGL)
    # Pre-set OpenGL/GLEW so find_package() in the D2 tree succeeds without a system search. Android
    # NDK provides EGL + GLESv3 natively.
    set(OPENGL_FOUND TRUE CACHE BOOL "" FORCE)
    set(OPENGL_LIBRARY "" CACHE STRING "" FORCE) # linked explicitly below
    set(GLEW_FOUND TRUE CACHE BOOL "" FORCE)
    set(GLEW_INCLUDE_DIRS "" CACHE PATH "" FORCE)
    set(GLEW_LIBRARIES "" CACHE STRING "" FORCE)
    # Create a dummy GLEW::GLEW imported target so the D2 CMakeLists can reference it.
    if(NOT TARGET GLEW::GLEW)
        add_library(GLEW::GLEW INTERFACE IMPORTED)
    endif()

    # Provide stub Find modules so find_package(OpenGL REQUIRED) and find_package(GLEW REQUIRED) in
    # the D2 tree succeed on Android.
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/cmake_modules/FindOpenGL.cmake"
         "set(OPENGL_FOUND TRUE)\nset(OPENGL_LIBRARY \"\")\nset(OPENGL_INCLUDE_DIR \"\")\n")
    file(
        WRITE "${CMAKE_CURRENT_BINARY_DIR}/cmake_modules/FindGLEW.cmake"
        "set(GLEW_FOUND TRUE)\nset(GLEW_INCLUDE_DIRS \"\")\nset(GLEW_LIBRARIES \"\")\nset(GLEW_INCLUDE_DIR \"\")\nif(NOT TARGET GLEW::GLEW)\n  add_library(GLEW::GLEW INTERFACE IMPORTED)\nendif()\n"
    )
    list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_CURRENT_BINARY_DIR}/cmake_modules")

    # OGLES must be visible to ALL targets in the D2 tree (2d, 3d, main, etc.) so that ogl_init.h
    # correctly guards <GL/glew.h>.
    add_definitions(-DOGLES)
    # gles3_shim.h is included from ogl_init.h, so all targets need this path
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/shared)
endif() # OPENGL

# ──────────────────────────────────────────────────────────────
# 1. Add D1 source tree
# ──────────────────────────────────────────────────────────────
set(DXX_TARGET_PREFIX "d1_")
add_subdirectory("${D1_SRC}" "${CMAKE_CURRENT_BINARY_DIR}/d1")
target_sources(
    ${DXX_TARGET_PREFIX}misc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/shared/hmp_android_shared.c
                                     ${CMAKE_CURRENT_SOURCE_DIR}/shared/physfsx_android_shared.c)
target_include_directories(${DXX_TARGET_PREFIX}misc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/shared)

# ──────────────────────────────────────────────────────────────
# 1. Android-specific fixups for dxx-redux-d1 target
# ──────────────────────────────────────────────────────────────
target_link_libraries(dxx-redux-d1 PRIVATE android log OpenSLES)
if(OPENGL)
    target_link_libraries(dxx-redux-d1 PRIVATE EGL GLESv3)
endif()
target_sources(
    dxx-redux-d1
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/jni_main.c
            ${CMAKE_CURRENT_SOURCE_DIR}/jni_udp_reconnect.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/jni_string.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/utf8_codec.c
            ${CMAKE_CURRENT_SOURCE_DIR}/jni_saf.c
            ${CMAKE_CURRENT_SOURCE_DIR}/android_surface.c
            ${CMAKE_CURRENT_SOURCE_DIR}/android_input.c
            ${CMAKE_CURRENT_SOURCE_DIR}/android_gamepad_config.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/android_autoselect.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/android_pilot_prefs.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/android_level_preview.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/jni_level_metadata.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/jni_resume_save.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/extract/cue_parser.c
            ${CMAKE_CURRENT_SOURCE_DIR}/extract/hfs_reader.c
            ${CMAKE_CURRENT_SOURCE_DIR}/extract/iso9660_reader.c
            ${CMAKE_CURRENT_SOURCE_DIR}/extract/mac_hfs_extract.c
            ${CMAKE_CURRENT_SOURCE_DIR}/extract/sti2_extract.c
            ${CMAKE_CURRENT_SOURCE_DIR}/extract/sow_extract.c
            ${CMAKE_CURRENT_SOURCE_DIR}/extract/stuffit_extract.c
            ${DXX_DISC_IMPORT_JNI_SOURCE}
            ${CMAKE_CURRENT_SOURCE_DIR}/extract/game_file_extensions.c
            ${CMAKE_CURRENT_SOURCE_DIR}/extract/inno_reader.c
            ${CMAKE_CURRENT_SOURCE_DIR}/extract/pkg_reader.c
            ${CMAKE_CURRENT_SOURCE_DIR}/extract/jni_gog_import.c
            ${CMAKE_CURRENT_SOURCE_DIR}/jni_music_control.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/songs_android_shared.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/messagebox.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/physfs_archiver_saf.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/input_demo_rng_mode.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/input_demo_fixture.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/input_demo_controls.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/input_demo_codec.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/input_demo_result.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/input_demo_state_trace.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/input_demo_recorder.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/input_demo_replay.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/input_demo_direct_command_policy.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/game_introspect.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/game_automate.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/android_axis_mailbox.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/console_ringbuf.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/merged_wall_debug.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/ogl_msaa_android.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/ogl_gpu_timer_android.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/ogl_texture_android.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/ogl_viewport_android.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/android_texture_debug.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/overlay_ringbuf.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/android_jni_overlay.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/android_loading_progress.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/android_meta_actions.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/android_rewind_policy.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/android_rewind.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/android_log.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/android_slowdown_detector.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/android_profile.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/android_resume_pilot.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/android_menu_scale.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/android_menu_touch_log.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/android_pilot_listbox_hold.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/android_save_meta.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/android_save_set.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/android_crash_handler.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/android_graphics_options.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/graphics_config_transaction.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/coop_indicator_lines.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/track_names.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/cd_preview.c
            ${CMAKE_CURRENT_SOURCE_DIR}/jni_cd_preview.c
            ${CMAKE_CURRENT_SOURCE_DIR}/jni_fingerprint.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/midi_preview.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/midi_enumeration.c
            ${CMAKE_CURRENT_SOURCE_DIR}/jni_midi_preview.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/pngfile_stb.c)
target_include_directories(
    dxx-redux-d1
    PRIVATE ${D1_SRC}/arch/include
            ${D1_SRC}/main
            ${D1_SRC}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/shared
            ${CMAKE_CURRENT_SOURCE_DIR}/extract
            ${CMAKE_CURRENT_SOURCE_DIR}/../../../build/generated/native-build-info
            ${_TSF_DIR}
            ${_DECODERS_DIR})

target_compile_definitions(dxx-redux-d1 PRIVATE INTROSPECT_ON HAVE_LIBPNG)
target_compile_definitions(${DXX_TARGET_PREFIX}arch_sdl PRIVATE INTROSPECT_ON)
target_compile_definitions(${DXX_TARGET_PREFIX}arch_ogl PRIVATE INTROSPECT_ON)
target_include_directories(${DXX_TARGET_PREFIX}arch_sdl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/shared)

# Replace SDL_mixer's music with TinySoundFont, same as D2
get_target_property(_d1_arch_sdl_sources ${DXX_TARGET_PREFIX}arch_sdl SOURCES)
list(FILTER _d1_arch_sdl_sources EXCLUDE REGEX "digi_mixer_music\\.c$")
list(FILTER _d1_arch_sdl_sources EXCLUDE REGEX "rbaudio\\.c$")
set_property(TARGET ${DXX_TARGET_PREFIX}arch_sdl PROPERTY SOURCES ${_d1_arch_sdl_sources})
target_sources(${DXX_TARGET_PREFIX}arch_sdl
               PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/shared/digi_tsf_music.c)
target_sources(${DXX_TARGET_PREFIX}arch_sdl
               PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/shared/rbaudio_bin.c)
target_include_directories(${DXX_TARGET_PREFIX}arch_sdl PRIVATE ${_TSF_DIR})
target_compile_definitions(${DXX_TARGET_PREFIX}arch_sdl PRIVATE ANDROID)
target_link_libraries(${DXX_TARGET_PREFIX}arch_sdl tsf)

if(OPENGL)
    target_compile_definitions(${DXX_TARGET_PREFIX}arch_ogl PRIVATE ANDROID HAVE_LIBPNG)
    target_link_libraries(${DXX_TARGET_PREFIX}arch_ogl PRIVATE EGL GLESv3)
    target_sources(
        ${DXX_TARGET_PREFIX}arch_ogl
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/shared/gles3_shim.c
                ${CMAKE_CURRENT_SOURCE_DIR}/shared/android_egl_surface.c)
    target_include_directories(
        ${DXX_TARGET_PREFIX}arch_ogl PRIVATE ${D1_SRC}/arch/include ${D1_SRC}/main
                                             ${D1_SRC}/include ${CMAKE_CURRENT_SOURCE_DIR}/shared)
    target_include_directories(${DXX_TARGET_PREFIX}xmodel PRIVATE ${PHYSFS_INCLUDE_DIR})
endif() # OPENGL

target_link_libraries(dxx-redux-d1 PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(dxx-redux-d1 PRIVATE lzma_sdk_c z dxx_fingerprint chromaprint ktx)
dxx_input_demo_apply_codec_deps(dxx-redux-d1)
dxx_input_demo_apply_build_metadata(dxx-redux-d1)
target_compile_definitions(dxx-redux-d1 PRIVATE KHRONOS_STATIC)
if(DXX_STRICT_FP)
    set_property(TARGET dxx-redux-d1 PROPERTY INTERPROCEDURAL_OPTIMIZATION FALSE)
else()
    set_property(TARGET dxx-redux-d1 PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# ──────────────────────────────────────────────────────────────
# 1. Add D2 source tree
# ──────────────────────────────────────────────────────────────
set(DXX_TARGET_PREFIX "d2_")
add_subdirectory("${D2_SRC}" "${CMAKE_CURRENT_BINARY_DIR}/d2")
target_sources(
    ${DXX_TARGET_PREFIX}misc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/shared/hmp_android_shared.c
                                     ${CMAKE_CURRENT_SOURCE_DIR}/shared/physfsx_android_shared.c)
target_include_directories(${DXX_TARGET_PREFIX}misc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/shared)

# ──────────────────────────────────────────────────────────────
# 1. Android-specific fixups for dxx-redux-d2 target
# ──────────────────────────────────────────────────────────────
target_link_libraries(dxx-redux-d2 PRIVATE android log OpenSLES)
if(OPENGL)
    target_link_libraries(dxx-redux-d2 PRIVATE EGL GLESv3)
endif()
target_sources(
    dxx-redux-d2
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/jni_main.c
            ${CMAKE_CURRENT_SOURCE_DIR}/jni_udp_reconnect.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/jni_string.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/utf8_codec.c
            ${CMAKE_CURRENT_SOURCE_DIR}/jni_saf.c
            ${CMAKE_CURRENT_SOURCE_DIR}/android_surface.c
            ${CMAKE_CURRENT_SOURCE_DIR}/android_input.c
            ${CMAKE_CURRENT_SOURCE_DIR}/android_gamepad_config.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/android_autoselect.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/android_pilot_prefs.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/android_level_preview.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/jni_level_metadata.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/jni_resume_save.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/extract/cue_parser.c
            ${CMAKE_CURRENT_SOURCE_DIR}/extract/hfs_reader.c
            ${CMAKE_CURRENT_SOURCE_DIR}/extract/iso9660_reader.c
            ${CMAKE_CURRENT_SOURCE_DIR}/extract/mac_hfs_extract.c
            ${CMAKE_CURRENT_SOURCE_DIR}/extract/sti2_extract.c
            ${CMAKE_CURRENT_SOURCE_DIR}/extract/sow_extract.c
            ${CMAKE_CURRENT_SOURCE_DIR}/extract/stuffit_extract.c
            ${DXX_DISC_IMPORT_JNI_SOURCE}
            ${CMAKE_CURRENT_SOURCE_DIR}/extract/game_file_extensions.c
            ${CMAKE_CURRENT_SOURCE_DIR}/extract/inno_reader.c
            ${CMAKE_CURRENT_SOURCE_DIR}/extract/pkg_reader.c
            ${CMAKE_CURRENT_SOURCE_DIR}/extract/jni_gog_import.c
            ${CMAKE_CURRENT_SOURCE_DIR}/jni_music_control.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/songs_android_shared.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/messagebox.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/physfs_archiver_saf.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/input_demo_rng_mode.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/input_demo_fixture.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/input_demo_controls.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/input_demo_codec.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/input_demo_result.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/input_demo_state_trace.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/input_demo_recorder.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/input_demo_replay.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/input_demo_direct_command_policy.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/game_introspect.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/game_automate.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/android_axis_mailbox.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/console_ringbuf.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/merged_wall_debug.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/ogl_msaa_android.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/ogl_gpu_timer_android.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/ogl_texture_android.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/ogl_viewport_android.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/android_texture_debug.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/overlay_ringbuf.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/android_jni_overlay.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/android_loading_progress.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/android_meta_actions.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/android_rewind_policy.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/android_rewind.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/android_log.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/android_slowdown_detector.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/android_profile.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/android_resume_pilot.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/android_menu_scale.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/android_menu_touch_log.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/android_pilot_listbox_hold.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/android_save_meta.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/android_save_set.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/android_crash_handler.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/android_graphics_options.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/graphics_config_transaction.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/coop_indicator_lines.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/track_names.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/cd_preview.c
            ${CMAKE_CURRENT_SOURCE_DIR}/jni_cd_preview.c
            ${CMAKE_CURRENT_SOURCE_DIR}/jni_fingerprint.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/midi_preview.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/midi_enumeration.c
            ${CMAKE_CURRENT_SOURCE_DIR}/jni_midi_preview.c
            ${CMAKE_CURRENT_SOURCE_DIR}/shared/pngfile_stb.c)
target_include_directories(
    dxx-redux-d2
    PRIVATE ${D2_SRC}/arch/include
            ${D2_SRC}/main
            ${D2_SRC}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/shared
            ${CMAKE_CURRENT_SOURCE_DIR}/extract
            ${CMAKE_CURRENT_SOURCE_DIR}/../../../build/generated/native-build-info
            ${_TSF_DIR}
            ${_DECODERS_DIR})

# Enable introspection API (AI-assisted testing, debug Android builds only). The Kotlin layer gates
# adb access behind BuildConfig.DEBUG. Must also be set on arch_sdl since window.c and event.c live
# there.
target_compile_definitions(dxx-redux-d2 PRIVATE INTROSPECT_ON HAVE_LIBPNG)
target_compile_definitions(dxx-redux-d2 PRIVATE DXX_BUILD_DESCENT_II)
target_compile_definitions(${DXX_TARGET_PREFIX}arch_sdl PRIVATE INTROSPECT_ON)
target_compile_definitions(${DXX_TARGET_PREFIX}arch_ogl PRIVATE INTROSPECT_ON)
target_include_directories(${DXX_TARGET_PREFIX}arch_sdl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/shared)

# Replace SDL_mixer's Timidity-based music with TinySoundFont MIDI synth. digi_mixer_music.c is
# already compiled into arch_sdl by the D2 CMakeLists; we remove it and add our TSF-based
# replacement instead.
get_target_property(_arch_sdl_sources ${DXX_TARGET_PREFIX}arch_sdl SOURCES)
list(FILTER _arch_sdl_sources EXCLUDE REGEX "digi_mixer_music\\.c$")
list(FILTER _arch_sdl_sources EXCLUDE REGEX "rbaudio\\.c$")
set_property(TARGET ${DXX_TARGET_PREFIX}arch_sdl PROPERTY SOURCES ${_arch_sdl_sources})
target_sources(${DXX_TARGET_PREFIX}arch_sdl
               PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/shared/digi_tsf_music.c)
target_sources(${DXX_TARGET_PREFIX}arch_sdl
               PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/shared/rbaudio_bin.c)
target_include_directories(${DXX_TARGET_PREFIX}arch_sdl PRIVATE ${_TSF_DIR})
target_compile_definitions(${DXX_TARGET_PREFIX}arch_sdl PRIVATE ANDROID)
target_link_libraries(${DXX_TARGET_PREFIX}arch_sdl tsf)

# OpenGL ES 3.0: link EGL + GLESv3 into the arch_ogl static library, and add Android-specific
# include paths + GLES3 shim.
if(OPENGL)
    target_compile_definitions(${DXX_TARGET_PREFIX}arch_ogl PRIVATE ANDROID HAVE_LIBPNG)
    target_link_libraries(${DXX_TARGET_PREFIX}arch_ogl PRIVATE EGL GLESv3)
    target_sources(
        ${DXX_TARGET_PREFIX}arch_ogl
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/shared/gles3_shim.c
                ${CMAKE_CURRENT_SOURCE_DIR}/shared/android_egl_surface.c)
    target_include_directories(
        ${DXX_TARGET_PREFIX}arch_ogl PRIVATE ${D2_SRC}/arch/include ${D2_SRC}/main
                                             ${D2_SRC}/include ${CMAKE_CURRENT_SOURCE_DIR}/shared)

    # xmodel needs the PhysFS include path (not set by its own CMakeLists.txt).
    target_include_directories(${DXX_TARGET_PREFIX}xmodel PRIVATE ${PHYSFS_INCLUDE_DIR})
endif() # OPENGL

# nlohmann/json — used by the introspection/automation C++ files
target_link_libraries(dxx-redux-d2 PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(dxx-redux-d2 PRIVATE lzma_sdk_c z dxx_fingerprint chromaprint ktx)
dxx_input_demo_apply_codec_deps(dxx-redux-d2)
dxx_input_demo_apply_build_metadata(dxx-redux-d2)
target_compile_definitions(dxx-redux-d2 PRIVATE KHRONOS_STATIC)
if(DXX_STRICT_FP)
    set_property(TARGET dxx-redux-d2 PROPERTY INTERPROCEDURAL_OPTIMIZATION FALSE)
else()
    set_property(TARGET dxx-redux-d2 PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()
