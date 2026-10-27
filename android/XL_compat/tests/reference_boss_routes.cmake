foreach(
    pair IN
    ITEMS "include/descent.h|f8c34f569c51541dc4d1e69ae3ac04972401b4c6cad99ac450bbc57a245f83e4"
          "objects/boss.cpp|553b98cd769c6105532b48ccd9d4b31b25f33d8d7bdda4c29a09434d80aa9042"
          "main/trigger.cpp|74f1284260fb4a4bb7396bc45eeb83868a7f2dfa5e3fdf0bcb767b94f1b12852")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Boss destination reference differs: ${path}")
    endif()
endforeach()
set(extracted "// Actual pinned boss destination setup and object teleport\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/objects/boss.cpp"
                      "bool CBossInfo::SetupSegments (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/objects/boss.cpp" "bool CBossInfo::Setup (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/trigger.cpp"
                      "void CTrigger::DoTeleportBot (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_boss_routes.h"
     CONTENT "${extracted}")
add_executable(xl_boss_routes_reference tests/reference_boss_routes.cpp)
target_compile_features(xl_boss_routes_reference PRIVATE cxx_std_17)
target_include_directories(xl_boss_routes_reference PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
