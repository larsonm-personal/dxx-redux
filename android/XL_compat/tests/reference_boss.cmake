file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/ai/aiboss.cpp" boss_hash)
file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/main/segment.cpp" segment_hash)
file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/include/segment.h" segment_header_hash)
file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/ai/ailib.cpp" collision_hash)
file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/include/vecmat.h" vector_hash)
file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/include/maths.h" math_hash)
if(NOT boss_hash STREQUAL "6ee8d46cb9211dde69cdd4ed1d2f1a4efde7dc13c862a3ec4c0206f1a2e29e99"
   OR NOT segment_hash STREQUAL "eece1c8ba624bd42e6e77b9f2a3dd9a54489338139ee102245672b2b7353e60d"
   OR NOT segment_header_hash STREQUAL
      "4f43198ba394971128c81373940b5a58283cbbbc2851ca6a6e4187e78382a667"
   OR NOT collision_hash STREQUAL "81389cc11b24dbf0971c455e8611d9864ffdcfb81798aea883c403c1833da8c4"
   OR NOT vector_hash STREQUAL "83d7f73f89a2f588a0b56a08fb9e293b747cc65a5ed22bad6771d64319bdca0f"
   OR NOT math_hash STREQUAL "43adf417dd1c282a39c8fed02bdad9d05a7f3ad5850a39a87897fd533eb857cd")
    message(FATAL_ERROR "Boss reference differs from audited revision")
endif()
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/ai/aiboss.cpp" boss_source)
string(FIND "${boss_source}" "int32_t spewBots " start)
string(FIND "${boss_source}" "int32_t CObject::CreateGatedRobot " end)
math(EXPR length "${end} - ${start}")
string(SUBSTRING "${boss_source}" ${start} ${length} extracted)
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/include/vecmat.h"
                      "inline const CFixVector& CFixVector::operator*= (const fix s)")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/include/vecmat.h"
                      "inline const fix CFixVector::Normalize (CFixVector& v)")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/segment.cpp"
                      "CFixVector CSegment::RandomPoint (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/ai/ailib.cpp"
                      "int32_t CheckObjectObjectIntersection (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/ai/aiboss.cpp"
                      "int32_t CObject::CreateGatedRobot (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/ai/aiboss.cpp"
                      "int32_t CObject::BossSpewRobot (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_boss.h" CONTENT "${extracted}")
add_executable(xl_boss_reference tests/reference_boss.cpp)
target_compile_features(xl_boss_reference PRIVATE cxx_std_17)
target_include_directories(xl_boss_reference PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
