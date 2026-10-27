file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/gameio/loadobjects.cpp" loader_hash)
file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/main/producers.cpp" producer_hash)
if(NOT loader_hash STREQUAL "01fe3cbc1cf781d1180921c9e235593f1b79d68924c0d49a42502ef8c3ffcd3c"
   OR NOT producer_hash STREQUAL "c79263fcd98f686d99dfcc8f5e12aade60f70f75cc25544795fbab71c93318b6")
    message(FATAL_ERROR "Generator reference differs from audited revision")
endif()
set(extracted "// Extracted XL source assignment and disk reader\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/gameio/loadobjects.cpp"
                      "static int32_t AssignProducer (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/producers.cpp"
                      "void ReadObjectProducerInfo (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/producers.cpp"
                      "int32_t GetObjProducerObjType (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/producers.cpp"
                      "inline int32_t VertigoObjFlags (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/producers.cpp"
                      "int32_t StartObjectProducer (")
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/main/producers.cpp" producer_source)
string(FIND "${producer_source}"
            "if (!(gameData.producerData.robotMakers [nObjProducer].objFlags [0] ||" begin)
string(SUBSTRING "${producer_source}" ${begin} -1 tail)
string(FIND "${tail}" "// Wait until we have a free slot" end)
string(SUBSTRING "${tail}" 0 ${end} selection_gate)
set(extracted
    "${extracted}\nint ReferenceSelectionGate(int nObjProducer) {\n${selection_gate}\nreturn GetObjProducerObjType(nullptr, gameData.producerData.robotMakers[nObjProducer].objFlags, 100);\n}\n#undef RETURN\n#define RETURN return;\n"
)
file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/maths/vecmat.cpp" math_hash)
file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/gameio/loadgame.cpp" loadgame_hash)
if(NOT math_hash STREQUAL "4115afe844ad77a81e9f0de57ae1bbf577b06d5f49aed8b02d19002ff2021335"
   OR NOT loadgame_hash STREQUAL "5d7ff36d6121411e0250be94353bbe3268bdd02b9bd35c6161951d879fcecc55")
    message(FATAL_ERROR "Generator mask/distance reference changed")
endif()
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/maths/vecmat.cpp" "fix CFixVector::Mag (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/gameio/loadgame.cpp"
                      "void SetVertigoRobotFlags (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/gameio/loadgame.cpp"
                      "fix RobotDefaultShield (")
foreach(
    pair IN
    ITEMS "include/vecmat.h|83d7f73f89a2f588a0b56a08fb9e293b747cc65a5ed22bad6771d64319bdca0f"
          "include/maths.h|43adf417dd1c282a39c8fed02bdad9d05a7f3ad5850a39a87897fd533eb857cd"
          "physics/collide.cpp|d10f88f82e5d283077914881d1acb719f06785a86be22e50617d92d6fce47bc9"
          "physics/physics.cpp|2ca8d5c939f31da9d4686637ed3e4305bf89e208a575f2f7f6ea2b4d2e6fab76")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Generator contact reference changed: ${path}")
    endif()
endforeach()
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/maths.h" math_source)
string(FIND "${math_source}" "#define FixMul64" begin)
string(SUBSTRING "${math_source}" ${begin} -1 tail)
string(FIND "${tail}" "\n#else" end)
string(SUBSTRING "${tail}" 0 ${end} operations)
set(extracted "${extracted}\n#define XRound(value) (fix)(value)\n${operations}\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/include/vecmat.h"
                      "inline const fix CFixVector::Normalize (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/include/vecmat.h"
                      "inline const CFixVector& CFixVector::operator*= (const fix s)")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/include/vecmat.h"
                      "inline const CFixVector& CFixVector::operator*= (const float s)")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/maths/vecmat.cpp"
                      "const CFixVector CFixVector::Random (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/physics/physics.cpp"
                      "void CObject::ApplyForce (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/physics/collide.cpp"
                      "void CObject::Bump (CFixVector vForce, fix xDamage)")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/physics/collide.cpp"
                      "int32_t CObject::CollidePlayerAndObjProducer (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/physics/collide.cpp"
                      "int32_t CObject::CollideRobotAndObjProducer (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_generators.h" CONTENT "${extracted}")
add_executable(xl_generator_reference tests/reference_generators.cpp)
target_link_libraries(xl_generator_reference PRIVATE xl_inventory)
target_include_directories(xl_generator_reference PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(xl_generator_reference PRIVATE cxx_std_17)
add_test(
    NAME xl_generator_assignment_reference
    COMMAND
        ${CMAKE_COMMAND} -DFIXTURE=$<TARGET_FILE:xl_inventory_test>
        -DREFERENCE=$<TARGET_FILE:xl_generator_reference>
        -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/generator-assignment -P
        ${CMAKE_CURRENT_LIST_DIR}/generators.cmake)
