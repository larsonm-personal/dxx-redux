set(FIXTURE_TARGET "test_${PROJECT_NAME}")
if(NOT ANDROID OR NOT GAME MATCHES "^d[12]$" OR NOT ENGINE_BUILD_DIR)
    message(FATAL_ERROR "Select Android GAME=d1/d2 and an existing ENGINE_BUILD_DIR")
endif()
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
if(ENGINE_LIBRARY_DIR)
    set(ENGINE_LIB_DIR "${ENGINE_LIBRARY_DIR}")
else()
    file(STRINGS "${ENGINE_BUILD_DIR}/CMakeCache.txt" OUTPUT_LINE
         REGEX "^CMAKE_LIBRARY_OUTPUT_DIRECTORY:")
    string(REGEX REPLACE "^[^=]+=" "" ENGINE_LIB_DIR "${OUTPUT_LINE}")
endif()
file(TO_CMAKE_PATH "${ENGINE_LIB_DIR}" ENGINE_LIB_DIR)
add_library(engine SHARED IMPORTED)
set_target_properties(engine PROPERTIES IMPORTED_LOCATION
                                        "${ENGINE_LIB_DIR}/libdxx-redux-${GAME}.so")
add_library(physfs SHARED IMPORTED)
set_target_properties(physfs PROPERTIES IMPORTED_LOCATION "${ENGINE_LIB_DIR}/libphysfs.so")
add_executable(${FIXTURE_TARGET} main.cpp)
target_compile_features(${FIXTURE_TARGET} PRIVATE cxx_std_17)
target_compile_options(${FIXTURE_TARGET} PRIVATE -Wno-macro-redefined)
target_compile_definitions(${FIXTURE_TARGET} PRIVATE ANDROID)
if(GAME STREQUAL "d2")
    target_compile_definitions(${FIXTURE_TARGET} PRIVATE DXX_BUILD_DESCENT_II)
endif()
target_include_directories(
    ${FIXTURE_TARGET}
    PRIVATE "${REPO_ROOT}/${GAME}/main"
            "${REPO_ROOT}/${GAME}/include"
            "${REPO_ROOT}/${GAME}/arch/include"
            "${REPO_ROOT}/android/app/src/main/cpp/shared"
            "${REPO_ROOT}/android/app/src/main/cpp/shared/net"
            "${ENGINE_BUILD_DIR}/_deps/physfs-src/src"
            "${ENGINE_BUILD_DIR}/_deps/sdl12-src/include"
            "${ENGINE_BUILD_DIR}/_deps/nlohmann_json-src/single_include")
target_link_libraries(${FIXTURE_TARGET} PRIVATE engine physfs)
target_link_options(${FIXTURE_TARGET} PRIVATE "-Wl,-rpath-link,${ENGINE_LIB_DIR}")
