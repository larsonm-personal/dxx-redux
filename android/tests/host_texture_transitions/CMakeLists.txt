cmake_minimum_required(VERSION 3.22)
project(host_texture_transitions LANGUAGES C CXX)
get_filename_component(REPO_ROOT "../../.." ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
include("${REPO_ROOT}/cmake/dxx-verified-dependencies.cmake")
find_path(PHYSFS_INCLUDE_DIR physfs.h REQUIRED)
find_library(PHYSFS_LIBRARY physfs REQUIRED)
find_path(JSON_INCLUDE_DIR nlohmann/json.hpp REQUIRED)
option(TEXTURE_TRANSITIONS_SANITIZE "Enable address/undefined sanitizers" ON)
set(KTX_FEATURE_STATIC_LIBRARY ON CACHE BOOL "" FORCE)
set(KTX_FEATURE_TOOLS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_TESTS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_DOC OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_JNI OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_GL_UPLOAD OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_VK_UPLOAD OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_LOADTEST_APPS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
dxx_verified_fetchcontent_declare(ktx_software KTX_SOFTWARE)
dxx_dependency_value(KTX_SOFTWARE_URL ktx_url)
string(REGEX MATCH "v[0-9]+\\.[0-9]+\\.[0-9]+" KTX_GIT_VERSION_FULL "${ktx_url}")
FetchContent_MakeAvailable(ktx_software)
if(TEXTURE_TRANSITIONS_SANITIZE)
    target_compile_options(ktx PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
endif()
dxx_verified_source(STB_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/stb_image.h")
foreach(GAME d1 d2)
    add_executable(host_texture_transitions_${GAME}
                   main.cpp "${REPO_ROOT}/android/app/src/main/cpp/shared/pngfile.c")
    target_compile_features(host_texture_transitions_${GAME} PRIVATE cxx_std_17 c_std_99)
    target_compile_definitions(host_texture_transitions_${GAME} PRIVATE ANDROID)
    # pngfile.c defines this itself; avoid libktx interface macro redefinition
    target_compile_options(host_texture_transitions_${GAME}
                           PRIVATE "$<$<COMPILE_LANGUAGE:C>:-UKHRONOS_STATIC>")
    target_include_directories(
        host_texture_transitions_${GAME}
        PRIVATE "${REPO_ROOT}/${GAME}/include" "${REPO_ROOT}/android/app/src/main/cpp/shared"
                "${CMAKE_CURRENT_BINARY_DIR}" "${PHYSFS_INCLUDE_DIR}" "${JSON_INCLUDE_DIR}")
    target_link_libraries(host_texture_transitions_${GAME} PRIVATE "${PHYSFS_LIBRARY}" ktx m)
    if(TEXTURE_TRANSITIONS_SANITIZE)
        target_compile_options(
            host_texture_transitions_${GAME} PRIVATE -fsanitize=address,undefined
                                                     -fno-omit-frame-pointer -fno-pie)
        target_link_options(host_texture_transitions_${GAME} PRIVATE -fsanitize=address,undefined
                            -no-pie)
    endif()
endforeach()
