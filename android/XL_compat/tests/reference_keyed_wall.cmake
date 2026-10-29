foreach(
    pair IN
    ITEMS
        "main/wall.cpp|05eaa2cab8dd7f5661afd210e0edc8dea4fcd2bb2075c00d3012bec395ab0c42"
        "include/wall.h|08c59bf8c299ea05967f806b92b3d03b4226914ef747ae1ef5afa4154c70c2bd"
        "ai/ailib.cpp|81389cc11b24dbf0971c455e8611d9864ffdcfb81798aea883c403c1833da8c4"
        "include/ai.h|3cbbee97161de8e59744aa217a6adb3325c8267a1d76a70aad839721594148e8"
        "include/aistruct.h|25950b637d9c7308a63d386bbf28d4a7613c14474e0373b0cf5a8cee2a3ccf1f"
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
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/wall.cpp" "int32_t CWall::ProcessHit (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/ai/ailib.cpp" "int32_t AIDoorIsOpenable (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_keyed_wall.h" CONTENT "${extracted}")
add_executable(xl_keyed_wall_reference tests/reference_keyed_wall.cpp)
target_include_directories(xl_keyed_wall_reference PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(xl_keyed_wall_reference PRIVATE cxx_std_17)
