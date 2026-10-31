file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/gameio/mission.cpp" actual)
if(NOT actual STREQUAL "09e553926d82aa084d398a9e5e6dae0aca91b7f4c843971cb6243e3c0021e47a")
    message(FATAL_ERROR "Mission flow source differs from pinned revision")
endif()
set(extracted "// Actual pinned mission destination policy\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/gameio/mission.cpp"
                      "void CMissionManager::AdvanceLevel (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/trigger.cpp" "bool CTrigger::DoExit (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/trigger.cpp" "int32_t FindNextLevel (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_mission_flow.h"
     CONTENT "${extracted}")
target_sources(xl_trigger_flags_reference PRIVATE tests/reference_mission_flow.cpp)
