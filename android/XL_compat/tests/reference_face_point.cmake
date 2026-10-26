file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/physics/sphere_collision.cpp" sphere_hash)
file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/maths/vecmat.cpp" vector_hash)
if(NOT sphere_hash STREQUAL "499873929ce6606f00b504e59624595e6bee57f9f4e225f53c85ebd0c1a80b60"
   OR NOT vector_hash STREQUAL "4115afe844ad77a81e9f0de57ae1bbf577b06d5f49aed8b02d19002ff2021335")
    message(FATAL_ERROR "XL face geometry source differs from audited revision")
endif()
function(extract_face_function path signature)
    file(READ "${path}" source)
    string(FIND "${source}" "${signature}" start)
    if(start LESS 0)
        message(FATAL_ERROR "Missing reference function: ${signature}")
    endif()
    string(SUBSTRING "${source}" ${start} -1 tail)
    string(FIND "${tail}" "\n}\n" end)
    if(end LESS 0)
        message(FATAL_ERROR "Missing reference function delimiter")
    endif()
    math(EXPR length "${end} + 3")
    string(SUBSTRING "${tail}" 0 ${length} fragment)
    set(extracted "${extracted}\n${fragment}" PARENT_SCOPE)
endfunction()
set(extracted "// Generated from pinned XL geometry functions\n")
file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/main/segmath.cpp" segmath_hash)
if(NOT segmath_hash STREQUAL "65949a1ce9a0a001ccb4a474b7b009d41e9cf0ea6975376350418d85ed1b6f4c")
    message(FATAL_ERROR "XL portal normal ordering source differs from audited revision")
endif()
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/segmath.cpp"
                      "uint16_t SortVertsForNormal (")
extract_face_function(
    "${XL_REFERENCE_GEOMETRY_ROOT}/maths/vecmat.cpp"
    "const int32_t FindPointLineIntersection (CFloatVector& hitP, const CFloatVector& p1, const CFloatVector& p2, const CFloatVector& p3, int32_t bClamp)"
)
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/physics/sphere_collision.cpp"
                      "uint8_t PointIsOutsideFace (CFloatVector*")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/physics/sphere_collision.cpp"
                      "float DistToFace (CFloatVector3")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_face_point.h" CONTENT "${extracted}")
add_executable(xl_face_point_test "${CMAKE_CURRENT_LIST_DIR}/reference_face_point_test.cpp")
target_include_directories(xl_face_point_test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(xl_face_point_test PRIVATE xl_inventory)
add_test(NAME xl_face_point_reference COMMAND xl_face_point_test)

file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/main/side.cpp" side_hash)
if(NOT side_hash STREQUAL "5f8552e6fb177b24a624c47c9c644029acf95eda9f7001624c7f28e7972b4989")
    message(FATAL_ERROR "XL side-center source differs from audited revision")
endif()
set(extracted "// Generated from pinned XL side-center functions\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/physics/sphere_collision.cpp"
                      "int32_t FindPlaneLineIntersection (CFixVector&")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/side.cpp"
                      "void CSide::ComputeCenter (void)")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_side_center.h"
     CONTENT "${extracted}")
add_executable(xl_side_center_test "${CMAKE_CURRENT_LIST_DIR}/reference_side_center_test.cpp")
target_include_directories(xl_side_center_test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(xl_side_center_test PRIVATE xl_inventory)
add_test(NAME xl_side_center_reference COMMAND xl_side_center_test)

set(extracted "// Generated from the returning XL fixed Cross overload\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/maths/vecmat.cpp"
                      "fix FixQuadAdjust (int64_t q)")
extract_face_function(
    "${XL_REFERENCE_GEOMETRY_ROOT}/maths/vecmat.cpp"
    "const CFixVector CFixVector::Cross (const CFixVector& v0, const CFixVector& v1)")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_fixed_cross.h"
     CONTENT "${extracted}")
add_executable(xl_fixed_cross_test "${CMAKE_CURRENT_LIST_DIR}/reference_fixed_cross_test.cpp")
target_include_directories(xl_fixed_cross_test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(xl_fixed_cross_test PRIVATE xl_inventory)
add_test(NAME xl_fixed_cross_reference COMMAND xl_fixed_cross_test)

file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/3d/points.cpp" points_hash)
if(NOT points_hash STREQUAL "0d89f71e96f275b360b37badaf274dc0fbcc751b50627ab0b94667b452644e10")
    message(FATAL_ERROR "XL projection source differs from audited revision")
endif()
set(extracted "// Generated from pinned XL screen projection\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/3d/points.cpp"
                      "uint8_t CRenderPoint::Project (CTransformation&")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_projection.h" CONTENT "${extracted}")
add_executable(xl_projection_test "${CMAKE_CURRENT_LIST_DIR}/reference_projection_test.cpp")
target_include_directories(xl_projection_test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(xl_projection_test PRIVATE xl_inventory)
add_test(NAME xl_projection_reference COMMAND xl_projection_test)

file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/include/vecmat.h" vector_header_hash)
if(NOT vector_header_hash STREQUAL
   "83d7f73f89a2f588a0b56a08fb9e293b747cc65a5ed22bad6771d64319bdca0f")
    message(FATAL_ERROR "XL normalization source differs from audited revision")
endif()
set(extracted "// Generated from pinned XL magnitude and normalization functions\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/maths/vecmat.cpp"
                      "fix CFixVector::Mag (void) const")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/include/vecmat.h"
                      "inline const fix CFixVector::Normalize (CFixVector& v)")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_magnitude.h" CONTENT "${extracted}")
add_executable(xl_magnitude_test "${CMAKE_CURRENT_LIST_DIR}/reference_magnitude_test.cpp")
target_include_directories(xl_magnitude_test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(xl_magnitude_test PRIVATE xl_inventory)
add_test(NAME xl_magnitude_reference COMMAND xl_magnitude_test)
