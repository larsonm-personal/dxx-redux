foreach(
    pair IN
    ITEMS "main/effects.cpp|242db260ae911c375d7f6f4c2b59a87d4fc1c6e72996abc3da963548fbc6b9c3"
          "ogl/ogl_texture.cpp|97ceec7d0c05661f5945bd198eab8b482cd69ee2cd8899ab09ddd055619e58ec"
          "main/vclip.cpp|88acb10deb5b4cec28496e07c5d5a563111b9ce1005fce4bfcc994d2ec0156f2")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Texture animation reference changed: ${path}")
    endif()
endforeach()
set(extracted "// Actual pinned effect frame time, compiled with both DBG values\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/effects.cpp" "fix EffectFrameTime (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_texture_time.h"
     CONTENT "${extracted}")
set(extracted "// Actual pinned animation setup and physics bitmap selection\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/effects.cpp" "CBitmap *FindAnimBaseTex (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/effects.cpp" "CBitmap *SetupHiresAnim (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/ogl/ogl_texture.cpp"
                      "CBitmap *LoadFaceBitmap (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_texture_animation.h"
     CONTENT "${extracted}")
target_sources(xl_pog_reference PRIVATE tests/reference_texture_animation.cpp)
set(extracted "// Actual pinned object animation setup and lifetime frame selection\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/vclip.cpp" "int32_t SetupHiresVClip (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/vclip.cpp" "inline int32_t CurFrame (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_vclip_animation.h"
     CONTENT "${extracted}")
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/ogl/ogl_texture.cpp" bitmap_source)
string(FIND "${bitmap_source}" "nFlags = BM_FLAG_TGA;" begin)
if(begin LESS 0)
    message(FATAL_ERROR "Missing source child-frame flag setup")
endif()
string(SUBSTRING "${bitmap_source}" ${begin} -1 tail)
string(FIND "${tail}" "pBmf->SetFlags (nFlags);" end)
if(end LESS 0)
    message(FATAL_ERROR "Missing source child-frame flag terminator")
endif()
string(SUBSTRING "${tail}" 0 ${end} flag_body)
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_texture_frame_flags.h"
     CONTENT "${flag_body}\n")
