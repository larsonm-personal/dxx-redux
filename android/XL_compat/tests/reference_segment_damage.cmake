# Hash checks for producers, caller, reader and player header also belong to the existing mandatory
# reader/repair oracles; keep this adapter independently pinned
foreach(
    pair IN
    ITEMS
        "main/producers.cpp|c79263fcd98f686d99dfcc8f5e12aade60f70f75cc25544795fbab71c93318b6"
        "objects/updateobject.cpp|c4f3b9f1cd225f4dda99bb2a4e42af8546bc37ae36cc0bd19091dad6f48b01ac"
        "objects/player.cpp|bab2ca407a6ac51993fb1194b785caa8101fe0a8a5896bd8a010cf68e07c09e5"
        "include/player.h|4e134411347c2dee70f22c7b60c63717390496022964a77c1eb8dc5b5ff15cf4")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Segment damage reference changed: ${path}")
    endif()
endforeach()
set(extracted "")
# ShieldDamage has nested column-zero braces; use its pinned final return boundary
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/main/producers.cpp" source)
string(FIND "${source}" "fix CSegment::ShieldDamage (" start)
string(SUBSTRING "${source}" ${start} -1 tail)
string(FIND "${tail}" "RETVAL (amount)\n}" end)
if(start LESS 0 OR end LESS 0)
    message(FATAL_ERROR "Missing pinned ShieldDamage body")
endif()
string(LENGTH "RETVAL (amount)\n}" ending)
math(EXPR length "${end} + ${ending}")
string(SUBSTRING "${tail}" 0 ${length} extracted)
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/producers.cpp"
                      "fix CSegment::EnergyDamage (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/objects/updateobject.cpp"
                      "void CObject::HandleSegmentFunction (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/objects/player.cpp" "bool CShipEnergy::Set (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_segment_damage.h"
     CONTENT "${extracted}")
add_executable(xl_segment_damage_reference tests/reference_segment_damage.cpp)
target_compile_features(xl_segment_damage_reference PRIVATE cxx_std_17)
target_include_directories(xl_segment_damage_reference PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME xl_segment_damage_reference COMMAND xl_segment_damage_reference)
