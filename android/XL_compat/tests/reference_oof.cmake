include("${CMAKE_CURRENT_LIST_DIR}/reference_oof_pins.cmake")
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
                      "void CModel::AssignChildren (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/models/oofread.cpp"
                      "void CModel::GetSubModelBounds (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/oof_direct_source.h" CONTENT "${extracted}")
add_executable(xl_oof_render_reference tests/reference_oof_render.cpp)
target_include_directories(xl_oof_render_reference PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(xl_oof_render_reference PRIVATE cxx_std_17)
add_test(NAME xl_oof_render_reference COMMAND xl_oof_render_reference)

add_test(
    NAME xl_oof_source_pins
    COMMAND
        ${CMAKE_COMMAND} -DREFERENCE=${XL_REFERENCE_GEOMETRY_ROOT}
        -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/oof-source-pins -P
        ${CMAKE_CURRENT_LIST_DIR}/reference_oof_pins_test.cmake)
