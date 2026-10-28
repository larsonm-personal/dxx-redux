foreach(
    pair IN
    ITEMS
        "main/producers.cpp|c79263fcd98f686d99dfcc8f5e12aade60f70f75cc25544795fbab71c93318b6"
        "network/network_init.cpp|6732bb4033ebfdfc0f17563a04beeed1b5620c53c91e36f7b6e798326158c8a2"
        "objects/updateobject.cpp|c4f3b9f1cd225f4dda99bb2a4e42af8546bc37ae36cc0bd19091dad6f48b01ac"
        "include/player.h|4e134411347c2dee70f22c7b60c63717390496022964a77c1eb8dc5b5ff15cf4")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Repair reference changed: ${path}")
    endif()
endforeach()
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/main/producers.cpp" source)
string(FIND "${source}" "fix CSegment::Repair (" start)
if(start LESS 0)
    message(FATAL_ERROR "Missing source Repair body")
endif()
string(SUBSTRING "${source}" ${start} -1 tail)
# This function contains a column-zero nested closing brace; select its exact return
string(FIND "${tail}" "RETVAL (amount)\n}" end)
if(end LESS 0)
    message(FATAL_ERROR "Missing final source Repair return")
endif()
string(LENGTH "RETVAL (amount)\n}" ending_length)
math(EXPR length "${end} + ${ending_length}")
string(SUBSTRING "${tail}" 0 ${length} extracted)
string(REPLACE "static fix lastPlayTime = 0;" "fix& lastPlayTime = repair_clock;" extracted
               "${extracted}")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/objects/updateobject.cpp"
                      "void CObject::HandleSegmentFunction (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_repair.h" CONTENT "${extracted}")
add_executable(xl_repair_reference tests/reference_repair.cpp)
target_include_directories(xl_repair_reference PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(xl_repair_reference PRIVATE cxx_std_17)
