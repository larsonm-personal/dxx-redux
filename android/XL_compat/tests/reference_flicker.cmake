# light.cpp and loadobjects.cpp hashes are checked by reference_changing_light.cmake
set(extracted "// Actual pinned XL flicker frame and explicit enable/disable routines\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/lighting/light.cpp" "void FlickerLights (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/lighting/light.cpp"
                      "CVariableLight *FindVariableLight (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/lighting/light.cpp"
                      "void DisableVariableLight (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/lighting/light.cpp"
                      "void EnableVariableLight (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_flicker.h" CONTENT "${extracted}")
add_executable(xl_flicker_reference tests/reference_flicker.cpp)
target_include_directories(xl_flicker_reference PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(xl_flicker_reference PRIVATE cxx_std_17)
