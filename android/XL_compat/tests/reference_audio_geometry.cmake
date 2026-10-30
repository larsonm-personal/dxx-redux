foreach(
    pair IN
    ITEMS "maths/tables.cpp|107434ea4cd7ccbe4d756d4f89006bc566d00d5c5fdac785ff6c31fea79c0926"
          "maths/vecmat.cpp|4115afe844ad77a81e9f0de57ae1bbf577b06d5f49aed8b02d19002ff2021335"
          "include/vecmat.h|83d7f73f89a2f588a0b56a08fb9e293b747cc65a5ed22bad6771d64319bdca0f")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Audio vector source changed: ${path}")
    endif()
endforeach()
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/maths/tables.cpp" tables)
string(REGEX MATCH "uint16_t acos_table \\[\\] = \\{[^}]*\\};" table "${tables}")
if(NOT table)
    message(FATAL_ERROR "Missing source inverse-cosine table")
endif()
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/maths.h" maths)
string(REGEX MATCH "#define FixDiv[^\n]+" division "${maths}")
set(extracted
    "// Pinned source vector consumers; storage adapters only\n#define XRound(x) (fix)(x)\n${division}\n${table}\n"
)
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/maths/fixc.cpp" "fixang FixACos(")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/maths/vecmat.cpp"
                      "fix CFixVector::Mag (void) const")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/maths/vecmat.cpp" "fix FixQuadAdjust (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/maths/vecmat.cpp"
                      "const CFixVector CFixVector::Cross (const CFixVector&")
foreach(
    signature IN
    ITEMS "inline const fix CFixVector::Normalize (" "inline const fix CFixVector::NormalizedDir ("
          "inline const fix CFixVector::Dot (const CFixVector& v0"
          "inline const fixang CFixVector::DeltaAngleNorm (")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/include/vecmat.h" "${signature}")
endforeach()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_audio_geometry.h"
     CONTENT "${extracted}")
target_sources(xl_named_audio_source PRIVATE tests/reference_audio_geometry.cpp)
