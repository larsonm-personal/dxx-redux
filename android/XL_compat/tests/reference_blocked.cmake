foreach(
    pair IN
    ITEMS "main/segment.cpp|eece1c8ba624bd42e6e77b9f2a3dd9a54489338139ee102245672b2b7353e60d"
          "include/segment.h|4f43198ba394971128c81373940b5a58283cbbbc2851ca6a6e4187e78382a667"
          "include/wall.h|08c59bf8c299ea05967f806b92b3d03b4226914ef747ae1ef5afa4154c70c2bd")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Blocked reference changed: ${path}")
    endif()
endforeach()
set(extracted "// Pinned active IsPassable implementation\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/segment.cpp"
                      "int32_t CSegment::IsPassable (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_blocked.h" CONTENT "${extracted}")
add_executable(xl_blocked_reference tests/reference_blocked.cpp)
target_include_directories(xl_blocked_reference PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(xl_blocked_reference PRIVATE cxx_std_17)
