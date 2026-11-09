file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/ai/aiboss.cpp" boss_hash)
file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/main/segment.cpp" segment_hash)
file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/include/segment.h" segment_header_hash)
file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/ai/ailib.cpp" collision_hash)
file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/include/vecmat.h" vector_hash)
file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/include/maths.h" math_hash)
if(NOT boss_hash STREQUAL "6ee8d46cb9211dde69cdd4ed1d2f1a4efde7dc13c862a3ec4c0206f1a2e29e99"
   OR NOT segment_hash STREQUAL "eece1c8ba624bd42e6e77b9f2a3dd9a54489338139ee102245672b2b7353e60d"
   OR NOT segment_header_hash STREQUAL
      "4f43198ba394971128c81373940b5a58283cbbbc2851ca6a6e4187e78382a667"
   OR NOT collision_hash STREQUAL "81389cc11b24dbf0971c455e8611d9864ffdcfb81798aea883c403c1833da8c4"
   OR NOT vector_hash STREQUAL "83d7f73f89a2f588a0b56a08fb9e293b747cc65a5ed22bad6771d64319bdca0f"
   OR NOT math_hash STREQUAL "43adf417dd1c282a39c8fed02bdad9d05a7f3ad5850a39a87897fd533eb857cd")
    message(FATAL_ERROR "Boss reference differs from audited revision")
endif()
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/ai/aiboss.cpp" boss_source)
string(FIND "${boss_source}" "int32_t spewBots " start)
string(FIND "${boss_source}" "int32_t CObject::CreateGatedRobot " end)
math(EXPR length "${end} - ${start}")
string(SUBSTRING "${boss_source}" ${start} ${length} extracted)
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/include/vecmat.h"
                      "inline const CFixVector& CFixVector::operator*= (const fix s)")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/include/vecmat.h"
                      "inline const fix CFixVector::Normalize (CFixVector& v)")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/segment.cpp"
                      "CFixVector CSegment::RandomPoint (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/ai/ailib.cpp"
                      "int32_t CheckObjectObjectIntersection (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/ai/aiboss.cpp"
                      "int32_t CObject::CreateGatedRobot (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/ai/aiboss.cpp"
                      "int32_t CObject::BossSpewRobot (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_boss.h" CONTENT "${extracted}")
add_executable(xl_boss_reference tests/reference_boss.cpp)
target_compile_features(xl_boss_reference PRIVATE cxx_std_17)
target_include_directories(xl_boss_reference PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/objects/reactor.cpp" reactor_hash)
if(NOT reactor_hash STREQUAL "9e011a86ff48f745fd2d6e0fb2beb0e539014a75894fbac93a7a114ea86b7b2c")
    message(FATAL_ERROR "Reactor objective reference differs from audited revision")
endif()
set(extracted "// Actual pinned reactor completion function\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/objects/reactor.cpp"
                      "void DoReactorDestroyedStuff (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_reactor_objectives.h"
     CONTENT "${extracted}")
target_sources(xl_boss_reference PRIVATE tests/reference_reactor_objectives.cpp)

# Execute the pinned signed role decoding; preserve both effective type and bEndsLevel instead of
# treating a negative byte as an ordinary robot
file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/objects/robot.cpp" signed_robot_hash)
file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/ai/aiframe.cpp" boss_properties_hash)
if(NOT signed_robot_hash STREQUAL "a33aee40ef05eb35d41538c75190df09bb744cd484146c6a3c6afb7a807818a3"
   OR NOT boss_properties_hash STREQUAL
      "9fdc24b85cdfc7282830a0cc6e5a52fce9390151ecb4f157080e919c9c9046fd")
    message(FATAL_ERROR "Signed boss profile reference changed")
endif()
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/objects/robot.cpp" role_source)
string(FIND "${role_source}" "botInfo [h].bossFlag = cf.ReadByte ();" start)
string(FIND "${role_source}" "botInfo [h].companion = cf.ReadByte ();" end)
math(EXPR length "${end} - ${start}")
string(SUBSTRING "${role_source}" ${start} ${length} role_source)
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_boss_role.h"
     CONTENT "${role_source}")
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/ai/aiframe.cpp" properties)
string(FIND "${properties}" "tBossProps bossProps " start)
string(FIND "${properties}" "// These globals are set" end)
math(EXPR length "${end} - ${start}")
string(SUBSTRING "${properties}" ${start} ${length} properties)
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_boss_properties.h"
     CONTENT "${properties}")
target_sources(xl_boss_reference PRIVATE tests/reference_boss_profile.cpp)
set(extracted "// Actual pinned conditional boss death completion\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/ai/aiboss.cpp" "void DoBossDyingFrame (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_boss_death.h" CONTENT "${extracted}")

file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/effects/fireball.cpp" blast_hash)
if(NOT blast_hash STREQUAL "e91c43142cbb1f85c0274aac19e97c95d62e95bc5aa31c1d2537db2113387bf4")
    message(FATAL_ERROR "Boss radial damage source changed")
endif()
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/effects/fireball.cpp" blast_source)
string(FIND "${blast_source}" "if (pObj->IsBoss () && pObj->BossId () >= BOSS_D2" start)
string(FIND "${blast_source}" "if (pObj->ApplyDamageToRobot (damage, nParent))" end)
math(EXPR length "${end} - ${start}")
string(SUBSTRING "${blast_source}" ${start} ${length} blast_source)
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_boss_radial.h"
     CONTENT "${blast_source}")

# FindObjTrigger intentionally ignores nType and current trigger ownership
file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/main/trigger.cpp" trigger_lookup_hash)
if(NOT trigger_lookup_hash STREQUAL "74f1284260fb4a4bb7396bc45eeb83868a7f2dfa5e3fdf0bcb767b94f1b12852")
    message(FATAL_ERROR "Boss attachment lookup source changed")
endif()
set(extracted "// Actual pinned attachment-presence lookup\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/trigger.cpp" "CTrigger *FindObjTrigger (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_boss_trigger_lookup.h" CONTENT "${extracted}")
