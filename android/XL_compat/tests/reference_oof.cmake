file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/models/oofread.cpp" oof_reader_hash)
if(NOT oof_reader_hash STREQUAL "97cc190e8d901d8ce49be52464727801caf2aab8c958916fa3903e369794a91d")
    message(FATAL_ERROR "OOF reader reference changed")
endif()
set(extracted "// Pinned OOF timed and special-point readers\n")
foreach(
    signature IN
    ITEMS "static int32_t OOF_ReadInt ("
          "static float OOF_ReadFloat ("
          "static void OOF_ReadVector ("
          "static char *OOF_ReadString ("
          "int32_t OOF::CFrameInfo::Read ("
          "int32_t CRotFrame::Read ("
          "int32_t CRotAnim::Read ("
          "int32_t CSpecialPoint::Read ("
          "int32_t CSpecialList::Read (")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/models/oofread.cpp" "${signature}")
endforeach()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_oof_readers.h"
     CONTENT "${extracted}")
add_executable(xl_oof_metadata_reference tests/reference_oof_metadata.cpp)
target_include_directories(xl_oof_metadata_reference PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(xl_oof_metadata_reference PRIVATE cxx_std_17)
add_test(NAME xl_oof_metadata_reference COMMAND xl_oof_metadata_reference)

file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/render/oofrender.cpp" oof_render_hash)
file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/3d/oof2model.cpp" oof_converter_hash)
if(NOT oof_render_hash STREQUAL "97bdbddd6771e5ed55014fba48e13945591fe044e326ab153c1c038d74168e26"
   OR NOT oof_converter_hash STREQUAL
      "aee6218b50428f975cd7b551a5cd749cb74bab421bf491e2c853ecdea62160e0")
    message(FATAL_ERROR "OOF render reference changed")
endif()
set(extracted "// Pinned optimized OOF converter\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/3d/oof2model.cpp"
                      "void CModel::GetOOFModelItems (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/oof_conversion_source.h" CONTENT "${extracted}")
set(extracted "// Pinned direct vertex transform and hierarchy bounds\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/render/oofrender.cpp"
                      "inline void CSubModel::TransformVertex (")
foreach(signature IN ITEMS "void CSubModel::Transform (" "int32_t CSubModel::Render ("
                           "int32_t CModel::Draw (" "int32_t CModel::Render (")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/render/oofrender.cpp" "${signature}")
endforeach()
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/models/oofread.cpp"
                      "void CModel::GetSubModelBounds (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/oof_direct_source.h" CONTENT "${extracted}")
add_executable(xl_oof_render_reference tests/reference_oof_render.cpp)
target_include_directories(xl_oof_render_reference PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(xl_oof_render_reference PRIVATE cxx_std_17)
add_test(NAME xl_oof_render_reference COMMAND xl_oof_render_reference)
