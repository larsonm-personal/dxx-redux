file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/main/trigger.cpp" trigger_hash)
if(NOT trigger_hash STREQUAL "74f1284260fb4a4bb7396bc45eeb83868a7f2dfa5e3fdf0bcb767b94f1b12852")
    message(FATAL_ERROR "Trigger reference differs from pinned revision")
endif()
set(extracted "// Hash-pinned XL trigger functions with storage adapters\n")
foreach(signature IN
        ITEMS "int32_t CTrigger::DoMasterTrigger (" "int32_t CTrigger::DoEnableTrigger ("
              "int32_t CTrigger::DoDisableTrigger (" "void CTrigger::Read (")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/trigger.cpp" "${signature}")
endforeach()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_trigger.h" CONTENT "${extracted}")
add_executable(xl_trigger_reference tests/reference_trigger.cpp)
target_include_directories(xl_trigger_reference PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(xl_trigger_reference PRIVATE cxx_std_17)
