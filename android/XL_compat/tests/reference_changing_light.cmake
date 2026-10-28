foreach(
    pair IN
    ITEMS "lighting/light.cpp|ca95b9183729c7f3665ef0706206afce093b53b0d97aa6ba11379dd5fdf5f36f"
          "include/segment.h|4f43198ba394971128c81373940b5a58283cbbbc2851ca6a6e4187e78382a667"
          "gameio/loadobjects.cpp|01fe3cbc1cf781d1180921c9e235593f1b79d68924c0d49a42502ef8c3ffcd3c")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Changing-light reference changed: ${path}")
    endif()
endforeach()
set(extracted "// Actual pinned XL light lookup, UVL application and segment attenuation\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/lighting/light.cpp" "int32_t FindDLIndex (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/lighting/light.cpp" "void ChangeLight (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/lighting/light.cpp"
                      "void ApplyLightToSegment (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_changing_light.h"
     CONTENT "${extracted}")
add_executable(xl_changing_light_reference tests/reference_changing_light.cpp)
target_include_directories(xl_changing_light_reference PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(xl_changing_light_reference PRIVATE cxx_std_17)
