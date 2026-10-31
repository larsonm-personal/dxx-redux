foreach(
    pair IN
    ITEMS "objects/objectio.cpp|528f64e09469b5a452e9c6cf0ae8e9d5dd3eaffeaf5478378669f0783deaf68f"
          "include/object.h|ed546edd2bbda56a84e9fef8543706707d47a08b48e36616852346b5a2f96127"
          "include/cfile.h|34d3db395260649003a848015d3ac4dbac516779012f34c4315b530626016191"
          "include/maths.h|43adf417dd1c282a39c8fed02bdad9d05a7f3ad5850a39a87897fd533eb857cd")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Effect reader reference differs from audited revision: ${path}")
    endif()
endforeach()
set(extracted "// Actual pinned object reader and effect identity normalization\n")
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/maths.h" math_header)
string(REGEX MATCH "inline float FRound[^\n]+" rounded "${math_header}")
if(NOT rounded)
    message(FATAL_ERROR "Missing source effect volume rounding")
endif()
string(APPEND extracted "${rounded}\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/objects/objectio.cpp"
                      "void CObject::FixEffectRenderType (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/objects/objectio.cpp"
                      "void CObject::Read (CFile& cf)")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_effects.h" CONTENT "${extracted}")
add_executable(xl_effect_reference tests/reference_effects.cpp)
foreach(
    pair IN
    ITEMS "main/trigger.cpp|74f1284260fb4a4bb7396bc45eeb83868a7f2dfa5e3fdf0bcb767b94f1b12852"
          "gameio/loadobjects.cpp|01fe3cbc1cf781d1180921c9e235593f1b79d68924c0d49a42502ef8c3ffcd3c")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Effect target reference differs: ${path}")
    endif()
endforeach()
set(extracted "// Actual pinned target reader normalization and geometry cleanup\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/trigger.cpp"
                      "void CTriggerTargets::Check (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/gameio/loadobjects.cpp"
                      "static void CheckTriggerInfo (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_effect_targets.h"
     CONTENT "${extracted}")
target_sources(xl_effect_reference PRIVATE tests/reference_effect_targets.cpp)
file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/include/trigger.h" header_hash)
if(NOT header_hash STREQUAL "8f539027666ce768f467226381e82fd62c07a3cd3642fe97f857980091cf4dfd")
    message(FATAL_ERROR "Ambient activation constants changed")
endif()
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/trigger.h" header)
string(REGEX MATCHALL "#define[ \t]+(TT_|TF_)[A-Z0-9_]+[ \t]+[0-9]+" constants "${header}")
string(REPLACE ";" "\n" constants "${constants}")
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/main/trigger.cpp" trigger)
string(REGEX MATCH "int32_t oppTrigTypes[^;]+;" opposite "${trigger}")
if(NOT opposite)
    message(FATAL_ERROR "Missing ambient source alternate table")
endif()
set(extracted
    "// Actual pinned activation consumers and post-action flags\n${constants}\n${opposite}\n")
foreach(signature IN ITEMS "int32_t CTrigger::DoEnableTrigger ("
                           "int32_t CTrigger::DoDisableTrigger (")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/trigger.cpp" "${signature}")
endforeach()
string(FIND "${trigger}" "if (Flagged (TF_ONE_SHOT))" start)
if(start LESS 0)
    message(FATAL_ERROR "Missing source post-action flags")
endif()
string(SUBSTRING "${trigger}" ${start} -1 tail)
string(FIND "${tail}" "#if DBG" finish)
if(finish LESS 0)
    message(FATAL_ERROR "Missing source post-action boundary")
endif()
string(SUBSTRING "${tail}" 0 ${finish} flags)
string(APPEND extracted "\nvoid CTrigger::PostFlags() {\n${flags}\n}\n")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_ambient_activation.h"
     CONTENT "${extracted}")
target_sources(xl_effect_reference PRIVATE tests/reference_ambient_activation.cpp)
target_link_libraries(xl_effect_reference PRIVATE xl_inventory)
target_include_directories(xl_effect_reference PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(xl_effect_reference PRIVATE cxx_std_17)
add_test(
    NAME xl_effect_decode_reference
    COMMAND
        ${CMAKE_COMMAND} -DFIXTURE=$<TARGET_FILE:xl_inventory_test>
        -DREFERENCE=$<TARGET_FILE:xl_effect_reference>
        -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/effect-decode -P
        ${CMAKE_CURRENT_LIST_DIR}/effect_decode.cmake)

include("${CMAKE_CURRENT_LIST_DIR}/reference_static_explosion.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/reference_particles.cmake")
