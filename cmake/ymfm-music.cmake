include_guard(GLOBAL)
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()
include(${CMAKE_CURRENT_LIST_DIR}/dxx-verified-dependencies.cmake)
dxx_verified_fetchcontent_declare(ymfm YMFM)
dxx_verified_fetchcontent_declare(ymfmidi YMFMIDI)
FetchContent_Populate(ymfm)
FetchContent_Populate(ymfmidi)
add_library(
    music_ymfm STATIC ${ymfm_SOURCE_DIR}/src/ymfm_opl.cpp ${ymfm_SOURCE_DIR}/src/ymfm_adpcm.cpp
                      ${ymfm_SOURCE_DIR}/src/ymfm_pcm.cpp)
set_target_properties(music_ymfm PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(music_ymfm SYSTEM PUBLIC ${ymfm_SOURCE_DIR}/src)

# Same bounded, pinned player variant used in the listening experiments
find_package(Python3 REQUIRED COMPONENTS Interpreter)
get_filename_component(_music_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(_music_patch "${_music_root}/android/tests/fm_feasibility/patch_live.py")
set_property(
    DIRECTORY
    APPEND
    PROPERTY CMAKE_CONFIGURE_DEPENDS "${_music_patch}"
             "${_music_root}/android/tests/fm_feasibility/patch_hmi.py"
             "${_music_root}/android/tests/fm_feasibility/hmi_pitch_table.inc")
set(_music_player_dir "${CMAKE_CURRENT_BINARY_DIR}/music-ymfmidi")
file(GLOB _music_player_files "${ymfmidi_SOURCE_DIR}/src/*.h" "${ymfmidi_SOURCE_DIR}/src/*.cpp")
foreach(source IN LISTS _music_player_files)
    get_filename_component(name "${source}" NAME)
    configure_file("${source}" "${_music_player_dir}/${name}" COPYONLY)
endforeach()
file(READ "${ymfmidi_SOURCE_DIR}/src/player.cpp" _player)
string(REPLACE "%-6llu" "%-6zu" _player "${_player}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/music_player_baseline.cpp" "${_player}")
execute_process(
    COMMAND
        "${Python3_EXECUTABLE}" "${_music_patch}"
        "${CMAKE_CURRENT_BINARY_DIR}/music_player_baseline.cpp" "${_music_player_dir}/player.cpp"
        --hmi-pitch --hmi-driver --hmi-header "${_music_player_dir}/player.h" --runtime
        --runtime-header "${_music_player_dir}/player.h" COMMAND_ERROR_IS_FATAL ANY)
add_library(
    music_ymfmidi STATIC
    ${_music_player_dir}/player.cpp
    ${_music_player_dir}/patches.cpp
    ${_music_player_dir}/patchnames.cpp
    ${_music_player_dir}/sequence.cpp
    ${_music_player_dir}/sequence_mid.cpp
    ${_music_player_dir}/sequence_hmp.cpp
    ${_music_player_dir}/sequence_hmi.cpp
    ${_music_player_dir}/sequence_mus.cpp
    ${_music_player_dir}/sequence_xmi.cpp)
set_target_properties(music_ymfmidi PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(music_ymfmidi SYSTEM PUBLIC ${_music_player_dir})
target_link_libraries(music_ymfmidi PUBLIC music_ymfm)
if(MSVC)
    target_compile_definitions(music_ymfmidi PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
