cmake_minimum_required(VERSION 3.19)
project(fm_feasibility LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(FM_DEPS "" CACHE PATH "Pinned experiment dependencies, fetched by experiment.py")
find_package(Git REQUIRED)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/dependencies.json" manifest)
foreach(dep ymfmidi ymfm emu8950)
    string(JSON expected GET "${manifest}" ${dep} commit)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${FM_DEPS}/${dep}" rev-parse HEAD OUTPUT_VARIABLE actual
        OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "${dep} revision differs from the experiment manifest")
    endif()
endforeach()
add_library(ymfm STATIC "${FM_DEPS}/ymfm/src/ymfm_opl.cpp" "${FM_DEPS}/ymfm/src/ymfm_adpcm.cpp"
                        "${FM_DEPS}/ymfm/src/ymfm_pcm.cpp")
target_include_directories(ymfm SYSTEM PUBLIC "${FM_DEPS}/ymfm/src")
set(ymfmidi_support
    "${FM_DEPS}/ymfmidi/src/patches.cpp"
    "${FM_DEPS}/ymfmidi/src/patchnames.cpp"
    "${FM_DEPS}/ymfmidi/src/sequence.cpp"
    "${FM_DEPS}/ymfmidi/src/sequence_mid.cpp"
    "${FM_DEPS}/ymfmidi/src/sequence_hmp.cpp"
    "${FM_DEPS}/ymfmidi/src/sequence_hmi.cpp"
    "${FM_DEPS}/ymfmidi/src/sequence_mus.cpp"
    "${FM_DEPS}/ymfmidi/src/sequence_xmi.cpp")
# Correct an upstream debug-only size_t printf format on Android LP64 Keep both audio
# implementations otherwise unchanged at this stage
file(READ "${FM_DEPS}/ymfmidi/src/player.cpp" player_source)
string(REPLACE "%-6llu" "%-6zu" player_source "${player_source}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/player_baseline.cpp" "${player_source}")
add_library(ymfmidi STATIC "${CMAKE_CURRENT_BINARY_DIR}/player_baseline.cpp" ${ymfmidi_support})
target_include_directories(ymfmidi SYSTEM PUBLIC "${FM_DEPS}/ymfmidi/src")
target_link_libraries(ymfmidi PUBLIC ymfm)
add_library(emu8950 STATIC "${FM_DEPS}/emu8950/emu8950.c" "${FM_DEPS}/emu8950/emuadpcm.c")
target_include_directories(emu8950 SYSTEM PUBLIC "${FM_DEPS}/emu8950")
add_executable(fm_probe probe.cpp)
target_link_libraries(fm_probe PRIVATE ymfmidi emu8950)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS patch_live.py patch_hmi.py hmi_pitch_table.inc)
execute_process(
    COMMAND
        "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/patch_live.py"
        "${CMAKE_CURRENT_BINARY_DIR}/player_baseline.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/player_live.cpp" COMMAND_ERROR_IS_FATAL ANY)
add_library(ymfmidi_live STATIC "${CMAKE_CURRENT_BINARY_DIR}/player_live.cpp" ${ymfmidi_support})
target_include_directories(ymfmidi_live SYSTEM PUBLIC "${FM_DEPS}/ymfmidi/src")
target_link_libraries(ymfmidi_live PUBLIC ymfm)
add_executable(fm_probe_live_fix probe.cpp)
target_link_libraries(fm_probe_live_fix PRIVATE ymfmidi_live emu8950)
execute_process(
    COMMAND
        "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/patch_live.py"
        "${CMAKE_CURRENT_BINARY_DIR}/player_baseline.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/player_hmi_pitch.cpp" --hmi-pitch COMMAND_ERROR_IS_FATAL ANY)
add_library(ymfmidi_hmi_pitch STATIC "${CMAKE_CURRENT_BINARY_DIR}/player_hmi_pitch.cpp"
                                     ${ymfmidi_support})
target_include_directories(ymfmidi_hmi_pitch SYSTEM PUBLIC "${FM_DEPS}/ymfmidi/src")
target_link_libraries(ymfmidi_hmi_pitch PUBLIC ymfm)
add_executable(fm_probe_hmi_pitch probe.cpp)
target_link_libraries(fm_probe_hmi_pitch PRIVATE ymfmidi_hmi_pitch emu8950)
set(hmi_player_dir "${CMAKE_CURRENT_BINARY_DIR}/hmi-driver")
file(GLOB hmi_player_files "${FM_DEPS}/ymfmidi/src/*.h" "${FM_DEPS}/ymfmidi/src/*.cpp")
foreach(source IN LISTS hmi_player_files)
    get_filename_component(name "${source}" NAME)
    configure_file("${source}" "${hmi_player_dir}/${name}" COPYONLY)
endforeach()
set(hmi_player_support)
foreach(source IN LISTS ymfmidi_support)
    get_filename_component(name "${source}" NAME)
    list(APPEND hmi_player_support "${hmi_player_dir}/${name}")
endforeach()
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/patch_live.py"
        "${CMAKE_CURRENT_BINARY_DIR}/player_baseline.cpp"
        "${hmi_player_dir}/player.cpp" --hmi-pitch --hmi-driver --hmi-header "${hmi_player_dir}/player.h"
        COMMAND_ERROR_IS_FATAL ANY)
add_library(ymfmidi_hmi_driver STATIC "${hmi_player_dir}/player.cpp" ${hmi_player_support})
target_include_directories(ymfmidi_hmi_driver SYSTEM PUBLIC "${hmi_player_dir}")
target_link_libraries(ymfmidi_hmi_driver PUBLIC ymfm)
add_executable(fm_probe_hmi_driver probe.cpp)
target_link_libraries(fm_probe_hmi_driver PRIVATE ymfmidi_hmi_driver emu8950)
if(MSVC)
    target_compile_definitions(ymfmidi PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(ymfmidi_live PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(ymfmidi_hmi_pitch PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(ymfmidi_hmi_driver PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(fm_probe PRIVATE /W4)
    target_compile_options(fm_probe_live_fix PRIVATE /W4)
    target_compile_options(fm_probe_hmi_pitch PRIVATE /W4)
else()
    target_compile_options(fm_probe PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(fm_probe_live_fix PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(fm_probe_hmi_pitch PRIVATE -Wall -Wextra -Wpedantic)
endif()
