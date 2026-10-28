foreach(
    pair IN
    ITEMS
        "3d/hitbox.cpp|2062a3230e07a0a00ff880621d22e66e322330f7c98c6f6e5d28896ed8a25eed"
        "models/polymodel.cpp|5e0b43cc1b12b5cc8531bf6f3fd81ecc10703b0ea5189ad4c7ce70dec3e24f17"
        "gameio/loadmodeldata.cpp|4b191d4abf1a3a68348b468bd6db77da805c2334ba9d49d623697c294c9bfb8c")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "HXM reference changed: ${path}")
    endif()
endforeach()
set(extracted "")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/3d/hitbox.cpp" "int32_t GetPolyModelMinMax (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/models/polymodel.cpp" "fix CPolyModel::Size (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_hxm_geometry.h"
     CONTENT "${extracted}")
add_executable(xl_hxm_reference tests/reference_hxm.cpp)
target_include_directories(xl_hxm_reference PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(xl_hxm_reference PRIVATE xl_inventory)
add_test(NAME xl_hxm_geometry_reference COMMAND xl_hxm_reference)
