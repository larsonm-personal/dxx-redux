# Supported reference setup uses exact CRLF working-tree bytes
foreach(
    pair IN
    ITEMS "include/player.h|4e134411347c2dee70f22c7b60c63717390496022964a77c1eb8dc5b5ff15cf4"
          "main/gamestates.cpp|34aeb804a0e1b428781d355b69f7b9aae357ff39a9c584ccfaaf5b0357d69744")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Slow-motion default/flag source differs: ${path}")
    endif()
endforeach()
set(slow_motion_source "${XL_REFERENCE_GEOMETRY_ROOT}/physics/slowmotion.cpp")
file(SHA256 "${slow_motion_source}" actual)
if(NOT actual STREQUAL "577260e0a2119e87e0ba7232553facd12efebafee962c737152bdd8b74f88fab")
    message(FATAL_ERROR "Slow-motion source differs from pinned default profile")
endif()
set(extracted "// Actual pinned default-profile control and ramp functions\n")
foreach(
    signature
    "void SetSlowMotionState ("
    "void SlowMotionMessage ("
    "void InitBulletTime ("
    "void InitSlowMotion ("
    "int32_t SlowMotionActive ("
    "int32_t BulletTimeActive ("
    "void SlowMotionOff ("
    "int32_t ToggleSlowMotion ("
    "void DoSlowMotionFrame (")
    extract_face_function("${slow_motion_source}" "${signature}")
endforeach()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_slow_motion.h"
     CONTENT "${extracted}")
add_executable(xl_slow_motion_reference tests/reference_slow_motion.cpp)
target_include_directories(xl_slow_motion_reference PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(xl_slow_motion_reference PRIVATE xl_inventory)
add_test(NAME xl_slow_motion_reference COMMAND xl_slow_motion_reference)
set_tests_properties(xl_slow_motion_reference PROPERTIES TIMEOUT 30)
