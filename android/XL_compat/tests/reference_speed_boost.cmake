# Reuse the extracted motion math and orientation functions from reference_teleport
foreach(
    pair IN
    ITEMS "physics/physics.cpp|2ca8d5c939f31da9d4686637ed3e4305bf89e208a575f2f7f6ea2b4d2e6fab76"
          "maths/vecmat.cpp|4115afe844ad77a81e9f0de57ae1bbf577b06d5f49aed8b02d19002ff2021335"
          "include/vecmat.h|83d7f73f89a2f588a0b56a08fb9e293b747cc65a5ed22bad6771d64319bdca0f")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Boost reference changed: ${path}")
    endif()
endforeach()
string(APPEND extracted "fix speedBoostSpeed = 0;\n")
foreach(signature IN ITEMS "void SetSpeedBoostVelocity (" "void CTrigger::StopSpeedBoost ("
                           "void CTrigger::DoSpeedBoost (")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/trigger.cpp" "${signature}")
endforeach()
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/physics/physics.cpp"
                      "void CObject::ProcessDrag (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_speed_boost.h"
     CONTENT "${extracted}")
add_executable(xl_speed_boost_reference tests/reference_speed_boost.cpp)
target_include_directories(xl_speed_boost_reference PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(xl_speed_boost_reference PRIVATE cxx_std_17)
