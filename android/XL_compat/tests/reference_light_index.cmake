foreach(
    pair IN
    ITEMS "lighting/light.cpp|ca95b9183729c7f3665ef0706206afce093b53b0d97aa6ba11379dd5fdf5f36f"
          "gameio/loadobjects.cpp|01fe3cbc1cf781d1180921c9e235593f1b79d68924c0d49a42502ef8c3ffcd3c"
          "objects/objectio.cpp|528f64e09469b5a452e9c6cf0ae8e9d5dd3eaffeaf5478378669f0783deaf68f"
          "include/segment.h|4f43198ba394971128c81373940b5a58283cbbbc2851ca6a6e4187e78382a667")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Light-index source changed: ${path}")
    endif()
endforeach()
set(extracted "")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/lighting/light.cpp"
                      "void CLightDeltaIndex::Read (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_light_index.h"
     CONTENT "${extracted}")
add_executable(xl_light_index_reference tests/reference_light_index.cpp)
target_link_libraries(xl_light_index_reference PRIVATE xl_inventory)
target_include_directories(xl_light_index_reference PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME xl_light_index_reference COMMAND xl_light_index_reference)
