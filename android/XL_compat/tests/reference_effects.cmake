foreach(
    pair IN
    ITEMS "objects/objectio.cpp|528f64e09469b5a452e9c6cf0ae8e9d5dd3eaffeaf5478378669f0783deaf68f"
          "include/object.h|ed546edd2bbda56a84e9fef8543706707d47a08b48e36616852346b5a2f96127"
          "include/cfile.h|34d3db395260649003a848015d3ac4dbac516779012f34c4315b530626016191"
          "include/maths.h|43adf417dd1c282a39c8fed02bdad9d05a7f3ad5850a39a87897fd533eb857cd")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Effect reader reference differs from audited revision: ${path}")
    endif()
endforeach()
set(extracted "// Actual pinned object reader and effect identity normalization\n")
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/maths.h" math_header)
string(REGEX MATCH "inline float FRound[^\n]+" rounded "${math_header}")
if(NOT rounded)
    message(FATAL_ERROR "Missing source effect volume rounding")
endif()
string(APPEND extracted "${rounded}\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/objects/objectio.cpp"
                      "void CObject::FixEffectRenderType (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/objects/objectio.cpp"
                      "void CObject::Read (CFile& cf)")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_effects.h" CONTENT "${extracted}")
add_executable(xl_effect_reference tests/reference_effects.cpp)
target_link_libraries(xl_effect_reference PRIVATE xl_inventory)
target_include_directories(xl_effect_reference PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(xl_effect_reference PRIVATE cxx_std_17)
add_test(
    NAME xl_effect_decode_reference
    COMMAND
        ${CMAKE_COMMAND} -DFIXTURE=$<TARGET_FILE:xl_inventory_test>
        -DREFERENCE=$<TARGET_FILE:xl_effect_reference>
        -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/effect-decode -P
        ${CMAKE_CURRENT_LIST_DIR}/effect_decode.cmake)
