foreach(pair IN
        ITEMS "main/side.cpp|5f8552e6fb177b24a624c47c9c644029acf95eda9f7001624c7f28e7972b4989"
              "include/wall.h|08c59bf8c299ea05967f806b92b3d03b4226914ef747ae1ef5afa4154c70c2bd")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Wall capacity reference changed: ${path}")
    endif()
endforeach()
file(STRINGS "${XL_REFERENCE_GEOMETRY_ROOT}/include/wall.h" wall_capacity
     REGEX "^#define MAX_WALLS[ \t]+[0-9]+")
set(extracted "${wall_capacity}\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/side.cpp" "void CSide::ReadWallNum (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_wall_capacity.h"
     CONTENT "${extracted}")
add_executable(xl_wall_capacity_reference tests/reference_wall_capacity.cpp)
target_include_directories(xl_wall_capacity_reference PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(xl_wall_capacity_reference PRIVATE cxx_std_17)
add_test(NAME xl_wall_capacity_reference COMMAND xl_wall_capacity_reference)
