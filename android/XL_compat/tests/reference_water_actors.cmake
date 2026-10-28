# Physics and motion math inputs are pinned by reference_speed_boost.cmake
set(extracted "// Pinned XL fluid motion consumers\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/physics/physics.cpp"
                      "void CPhysSimData::Setup (void)")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/physics/physics.cpp"
                      "int32_t CObject::UpdateOffset (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_water_actors.h"
     CONTENT "${extracted}")
add_executable(xl_water_actors_reference tests/reference_water_actors.cpp)
target_include_directories(xl_water_actors_reference PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(xl_water_actors_reference PRIVATE cxx_std_17)
