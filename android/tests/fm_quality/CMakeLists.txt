cmake_minimum_required(VERSION 3.24)
project(fm_quality LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
get_filename_component(REPO "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
set(SHARED "${REPO}/android/app/src/main/cpp/shared")
set(EXTRACT "${REPO}/android/app/src/main/cpp/extract")
include("${REPO}/cmake/ymfm-music.cmake")
dxx_verified_fetchcontent_declare(tinysoundfont_test TINYSOUNDFONT)
FetchContent_MakeAvailable(tinysoundfont_test)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS prepare.py
                                       "${EXTRACT}/test_music_synth.c" "${SHARED}/music_synth.cpp")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/prepare.py" "${REPO}"
                        "${CMAKE_CURRENT_BINARY_DIR}" COMMAND_ERROR_IS_FATAL ANY)

set(player_support
    patches.cpp
    patchnames.cpp
    sequence.cpp
    sequence_mid.cpp
    sequence_hmp.cpp
    sequence_hmi.cpp
    sequence_mus.cpp
    sequence_xmi.cpp)
set(float_sources "${CMAKE_CURRENT_BINARY_DIR}/float-player/player.cpp")
foreach(source IN LISTS player_support)
    list(APPEND float_sources "${CMAKE_CURRENT_BINARY_DIR}/float-player/${source}")
endforeach()
add_library(quality_float_player STATIC ${float_sources})
target_include_directories(quality_float_player SYSTEM
                           PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/float-player")
target_link_libraries(quality_float_player PUBLIC music_ymfm)
foreach(variant current float native shipping)
    if(variant STREQUAL shipping)
        set(driver "${EXTRACT}/test_music_synth.c")
        set(synth "${SHARED}/music_synth.cpp")
    elseif(variant STREQUAL current)
        set(driver "${EXTRACT}/test_music_synth.c")
        set(synth "${CMAKE_CURRENT_BINARY_DIR}/legacy_synth.cpp")
    else()
        set(driver "${CMAKE_CURRENT_BINARY_DIR}/${variant}_driver.c")
        set(synth "${CMAKE_CURRENT_BINARY_DIR}/float_synth.cpp")
    endif()
    add_executable(
        fm_quality_${variant}
        "${driver}" "${synth}" "${SHARED}/music_soundfont.c" "${SHARED}/tsf_impl.c"
        "${SHARED}/hmp_android_shared.c" "${SHARED}/midi_seek_timeline.c")
    target_include_directories(
        fm_quality_${variant} PRIVATE "${EXTRACT}/test_hmp_stubs" "${SHARED}"
                                      "${tinysoundfont_test_SOURCE_DIR}")
    target_compile_definitions(fm_quality_${variant} PRIVATE DXX_MUSIC_SYNTH_TEST_TRACE)
    if(variant STREQUAL float)
        target_link_libraries(fm_quality_${variant} PRIVATE quality_float_player)
    else()
        target_link_libraries(fm_quality_${variant} PRIVATE music_ymfmidi)
    endif()
    if(NOT MSVC)
        target_link_libraries(fm_quality_${variant} PRIVATE m)
    endif()
    if(ANDROID)
        target_link_libraries(fm_quality_${variant} PRIVATE android log)
    endif()
endforeach()
if(MSVC)
    target_compile_definitions(quality_float_player PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

enable_testing()
add_executable(test_fm_resampler "${EXTRACT}/test_fm_resampler.cpp")
target_include_directories(test_fm_resampler PRIVATE "${SHARED}")
add_test(NAME fm_resampler COMMAND test_fm_resampler)
add_test(NAME music_synth COMMAND fm_quality_shipping "${REPO}/android/app/src/main/assets/gm.sf2")
