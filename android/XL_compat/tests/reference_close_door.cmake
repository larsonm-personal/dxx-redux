file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/main/segment.cpp" segment_hash)
if(NOT segment_hash STREQUAL "eece1c8ba624bd42e6e77b9f2a3dd9a54489338139ee102245672b2b7353e60d")
    message(FATAL_ERROR "Close-door reference segment source changed")
endif()
# wall.cpp is pinned by reference_keyed_wall.cmake before this include
set(extracted "// Actual pinned XL allocation, lookup, close transition and caller\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/wall.cpp"
                      "static CActiveDoor* AddActiveDoor (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/wall.cpp" "CActiveDoor* FindActiveDoor (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/wall.cpp"
                      "CActiveDoor* CWall::CloseDoor (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/segment.cpp" "void CSegment::CloseDoor (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_close_door.h" CONTENT "${extracted}")
add_executable(xl_close_door_reference tests/reference_close_door.cpp)
target_include_directories(xl_close_door_reference PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(xl_close_door_reference PRIVATE cxx_std_17)
