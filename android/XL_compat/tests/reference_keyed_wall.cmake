foreach(
    pair IN
    ITEMS
        "main/wall.cpp|05eaa2cab8dd7f5661afd210e0edc8dea4fcd2bb2075c00d3012bec395ab0c42"
        "include/wall.h|08c59bf8c299ea05967f806b92b3d03b4226914ef747ae1ef5afa4154c70c2bd"
        "physics/sphere_collision.cpp|499873929ce6606f00b504e59624595e6bee57f9f4e225f53c85ebd0c1a80b60"
)
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Keyed-wall reference changed: ${path}")
    endif()
endforeach()
set(extracted "// Actual pinned CWall::IsPassable; no production wall helper\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/wall.cpp" "int32_t CWall::IsPassable (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_keyed_wall.h" CONTENT "${extracted}")
add_executable(xl_keyed_wall_reference tests/reference_keyed_wall.cpp)
target_include_directories(xl_keyed_wall_reference PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(xl_keyed_wall_reference PRIVATE cxx_std_17)
