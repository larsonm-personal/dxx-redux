foreach(
    pair IN
    ITEMS "include/object.h|ed546edd2bbda56a84e9fef8543706707d47a08b48e36616852346b5a2f96127"
          "include/descent.h|f8c34f569c51541dc4d1e69ae3ac04972401b4c6cad99ac450bbc57a245f83e4"
          "ai/aiframe.cpp|9fdc24b85cdfc7282830a0cc6e5a52fce9390151ecb4f157080e919c9c9046fd"
          "objects/object.cpp|e71b0a906cd686549c783563ab93365d1bbf5cf05f08c1971ef08c7116c67a64"
          "ai/aifire.cpp|c6bd36bde64f9e881d59a17798016f8b2d2ba1e8fec2e6f4371bce82d7a743f6"
          "ai/thief.cpp|d4fb58f44549fff1015d89374f2306cd44f6989a650a2bcb56bd6e2b52582358"
          "physics/collide.cpp|d10f88f82e5d283077914881d1acb719f06785a86be22e50617d92d6fce47bc9")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Disarm source differs: ${path}")
    endif()
endforeach()
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/descent.h" lookup_header)
string(REGEX MATCH "inline CObject\\* CGameData::Object \\(int32_t nObject\\) \\{[^}]*\\}" lookup
             "${lookup_header}")
if(NOT lookup)
    message(FATAL_ERROR "Missing actual runtime object lookup")
endif()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_disarm_lookup.h"
     CONTENT "${lookup}\n")
set(extracted "// Actual source inline attack-mode consumers\n")
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/object.h" object_header)
foreach(method IN ITEMS Arm Disarm Disarmed Reprogram Reprogrammed)
    string(REGEX MATCH "inline (void|bool) ${method} \\(void\\) \\{[^}]*\\}" fragment
                 "${object_header}")
    if(NOT fragment)
        message(FATAL_ERROR "Missing inline attack-mode method: ${method}")
    endif()
    string(APPEND extracted "${fragment}\n")
endforeach()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_disarm_methods.h"
     CONTENT "${extracted}")
set(extracted "// Actual source action, predicates and melee consumer\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/trigger.cpp"
                      "int32_t CTrigger::DoDisarmRobots (")
foreach(signature IN ITEMS "bool CObject::AttacksRobots (" "bool CObject::AttacksPlayer ("
                           "bool CObject::AttacksObject (")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/objects/object.cpp" "${signature}")
endforeach()
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/ai/aifire.cpp" "void DoAIRobotHitAttack (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_disarm.h" CONTENT "${extracted}")
# Execute the exact ranged/thief return guards, without claiming whole-AI equivalence
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/ai/aifire.cpp" source)
string(REGEX MATCH
             "if \\(TARGETOBJ->IsPlayer \\(\\) && !pObj->AttacksPlayer \\(\\)\\)[ \t\r\n]+RETURN"
             fire_guard "${source}")
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/ai/thief.cpp" source)
string(REGEX MATCH
             "if \\(pObj->Disarmed \\(\\) \\|\\| pObj->Reprogrammed \\(\\)\\)[ \t\r\n]+return;"
             thief_guard "${source}")
if(NOT fire_guard OR NOT thief_guard)
    message(FATAL_ERROR "Disarm consumer guard missing")
endif()
file(
    GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_disarm_guards.h"
    CONTENT
        "void fire_guard(CObject *pObj) { ${fire_guard}\n ++shots; }\nvoid thief_guard(CObject *pObj) { ${thief_guard}\n ++decisions; }\n"
)
target_sources(xl_trigger_flags_reference PRIVATE tests/reference_disarm.cpp)
