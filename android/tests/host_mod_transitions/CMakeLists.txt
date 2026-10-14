cmake_minimum_required(VERSION 3.22)
project(host_mod_transitions LANGUAGES C CXX)
get_filename_component(REPO_ROOT "../../.." ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
find_path(SDL_INCLUDE_DIR SDL.h PATH_SUFFIXES SDL REQUIRED)
find_library(SDL_LIBRARY SDL REQUIRED)
find_path(PHYSFS_INCLUDE_DIR physfs.h REQUIRED)
find_library(PHYSFS_LIBRARY physfs REQUIRED)
find_path(JSON_INCLUDE_DIR nlohmann/json.hpp REQUIRED)
option(MOD_TRANSITIONS_SANITIZE "Enable address/undefined sanitizers" ON)
foreach(GAME d1 d2)
    add_executable(
        host_mod_transitions_${GAME}
        main.cpp "${REPO_ROOT}/android/app/src/main/cpp/shared/android_mod_assets.cpp"
        "${REPO_ROOT}/${GAME}/misc/ignorecase.c")
    target_compile_features(host_mod_transitions_${GAME} PRIVATE cxx_std_17)
    target_compile_definitions(host_mod_transitions_${GAME} PRIVATE ANDROID)
    if(GAME STREQUAL "d2")
        target_compile_definitions(host_mod_transitions_${GAME} PRIVATE DXX_BUILD_DESCENT_II)
    endif()
    target_include_directories(
        host_mod_transitions_${GAME}
        PRIVATE "${REPO_ROOT}/${GAME}/main"
                "${REPO_ROOT}/${GAME}/include"
                "${REPO_ROOT}/${GAME}/arch/include"
                "${REPO_ROOT}/android/app/src/main/cpp/shared"
                "${SDL_INCLUDE_DIR}"
                "${PHYSFS_INCLUDE_DIR}"
                "${JSON_INCLUDE_DIR}")
    target_link_libraries(host_mod_transitions_${GAME} PRIVATE "${SDL_LIBRARY}" "${PHYSFS_LIBRARY}")
    if(MOD_TRANSITIONS_SANITIZE)
        target_compile_options(host_mod_transitions_${GAME} PRIVATE -fsanitize=address,undefined
                                                                    -fno-omit-frame-pointer)
        target_link_options(host_mod_transitions_${GAME} PRIVATE -fsanitize=address,undefined)
    endif()
endforeach()
