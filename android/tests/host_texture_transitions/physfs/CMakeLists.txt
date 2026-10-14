cmake_minimum_required(VERSION 3.22)
project(host_pinned_physfs LANGUAGES C)
get_filename_component(REPO_ROOT "../../../.." ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
include("${REPO_ROOT}/cmake/dxx-verified-dependencies.cmake")
set(PHYSFS_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(PHYSFS_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(PHYSFS_BUILD_TEST OFF CACHE BOOL "" FORCE)
set(PHYSFS_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
set(PHYSFS_BUILD_DOCS OFF CACHE BOOL "" FORCE)
dxx_verified_fetchcontent_declare(physfs PHYSFS)
FetchContent_MakeAvailable(physfs)
target_compile_options(physfs-static PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
add_executable(host_physfs_version version.c)
target_include_directories(host_physfs_version PRIVATE "${physfs_SOURCE_DIR}/src")
target_link_libraries(host_physfs_version PRIVATE physfs-static)
target_compile_options(host_physfs_version PRIVATE -fsanitize=address,undefined
                                                   -fno-omit-frame-pointer -fno-pie)
target_link_options(host_physfs_version PRIVATE -fsanitize=address,undefined -no-pie)
file(
    GENERATE
    OUTPUT "${CMAKE_BINARY_DIR}/paths.txt"
    CONTENT
        "PHYSFS_INCLUDE_DIR=${physfs_SOURCE_DIR}/src\nPHYSFS_LIBRARY=$<TARGET_FILE:physfs-static>\n"
)
