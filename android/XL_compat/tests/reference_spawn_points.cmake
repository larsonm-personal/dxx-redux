# Transform math and trigger.cpp hash are pinned by reference_teleport.cmake
set(extracted "// Pinned XL spawn-point action\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/trigger.cpp"
                      "int32_t CTrigger::DoSetSpawnPoints (void)")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/trigger.cpp"
                      "void CTrigger::PrintMessage (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_spawn_points.h"
     CONTENT "${extracted}")
add_executable(xl_spawn_points_reference tests/reference_spawn_points.cpp)
target_include_directories(xl_spawn_points_reference PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(xl_spawn_points_reference PRIVATE cxx_std_17)
