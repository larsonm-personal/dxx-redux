foreach(
    pair IN
    ITEMS
        "effects/lightning_manager.cpp|490b7f1fbd77228f90d354f49de69656f82364f0157a88cdad8fe8a4df6ff416"
        "effects/lightning.cpp|e84717bacf25a1d1f3b4d8c3432f281bc76717d60eed43485d23764188bdb236"
        "effects/lightning_node.cpp|6e98c8c61a65cc6f6f61101123962759ad104c8451fb3f9399af2139d1340510"
        "effects/lightning_emitter.cpp|88abca90525cd419de63dabc4e1fdb728bebc64ec014f5468629497600470928"
        "include/lightning.h|fc7bdf7f0b7e42f3ef216841121becda3781969f3b3aeffeb89d4413dcb74caf"
        "include/vecmat.h|83d7f73f89a2f588a0b56a08fb9e293b747cc65a5ed22bad6771d64319bdca0f"
        "maths/vecmat.cpp|4115afe844ad77a81e9f0de57ae1bbf577b06d5f49aed8b02d19002ff2021335")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Lightning source changed: ${path}")
    endif()
endforeach()
set(extracted "// Pinned authored lightning conversion and lifetime transitions\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/effects/lightning_manager.cpp"
                      "int32_t CLightningManager::Create (tLightningInfo&")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/effects/lightning.cpp"
                      "void CLightning::Init (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/effects/lightning.cpp"
                      "int32_t CLightning::SetLife (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_lightning.h" CONTENT "${extracted}")
add_executable(xl_lightning_test tests/reference_lightning.cpp)
target_include_directories(xl_lightning_test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
                                                     "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(xl_lightning_test PRIVATE cxx_std_17)
target_link_libraries(xl_lightning_test PRIVATE xl_inventory)
add_test(NAME xl_lightning_source COMMAND xl_lightning_test)

set(extracted "// Actual pinned vector operations and style-0 path\n")
foreach(
    signature IN
    ITEMS "inline const CFixVector& CFixVector::operator+= (const CFixVector&"
          "inline const CFixVector& CFixVector::operator*= (const fix"
          "inline const CFixVector CFixVector::operator+ (const CFixVector&"
          "inline const CFixVector CFixVector::operator- (const CFixVector&"
          "inline const CFixVector CFixVector::operator* (const fix"
          "inline bool CFixVector::operator== (const CFixVector&"
          "inline const fix CFixVector::Dist ("
          "inline const fix CFixVector::Normalize ("
          "inline const CFixVector CFixVector::Perp ("
          "inline const CFixVector CFixVector::Normal ("
          "inline const fix CFixVector::Dot (const CFixVector&")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/include/vecmat.h" "${signature}")
endforeach()
foreach(
    signature IN
    ITEMS "const int32_t FindPointLineIntersection (CFixVector&" "fix FixQuadAdjust (int64_t q)"
          "fix CFixVector::Mag (" "void CFixVector::Check (" "const CFixVector CFixVector::Cross (")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/maths/vecmat.cpp" "${signature}")
endforeach()
foreach(
    signature IN
    ITEMS "void CLightningNode::Move (const CFixVector& vOffset"
          "void CLightningNode::Move (const CFixVector& vOldPos"
          "void CLightningNode::Setup ("
          "void CLightningNode::Animate ("
          "void CLightningNode::ComputeOffset ("
          "CFixVector CLightningNode::CreateErratic (")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/effects/lightning_node.cpp" "${signature}")
endforeach()
foreach(
    signature IN
    ITEMS "void CLightning::Move (CFixVector vNewPos, CFixVector vNewEnd"
          "void CLightning::Move (CFixVector vNewPos, int16_t"
          "void CLightning::Smoothe ("
          "void CLightning::ComputeOffsets ("
          "void CLightning::CreatePath ("
          "CFixVector *VmRandomVector ("
          "void CLightning::Setup ("
          "bool CLightning::Create ("
          "void CLightning::Animate ("
          "int32_t CLightning::SetLife (")
    # Setup has an unindented inner closing brace; use the source's function separator
    file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/effects/lightning.cpp" source)
    string(FIND "${source}" "${signature}" start)
    if(start LESS 0)
        message(FATAL_ERROR "Missing lightning function: ${signature}")
    endif()
    string(SUBSTRING "${source}" ${start} -1 tail)
    string(FIND "${tail}"
                "\n//------------------------------------------------------------------------------"
                end)
    if(end LESS 0)
        message(FATAL_ERROR "Missing lightning function separator: ${signature}")
    endif()
    string(SUBSTRING "${tail}" 0 ${end} fragment)
    string(APPEND extracted "\n${fragment}")
endforeach()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_lightning_path.h"
     CONTENT "${extracted}")
set(extracted "// Actual pinned emitter animation/lifetime ordering and update gate\n")
foreach(
    signature IN
    ITEMS "void CLightningEmitter::Move (CFixVector vNewPos, int16_t"
          "void CLightningEmitter::MoveForObject (" "void CLightningEmitter::Animate ("
          "int32_t CLightningEmitter::SetLife (" "int32_t CLightningEmitter::Update (")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/effects/lightning_emitter.cpp"
                          "${signature}")
endforeach()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_lightning_emitter.h"
     CONTENT "${extracted}")
target_sources(xl_lightning_test PRIVATE tests/reference_lightning_path.cpp)

set(extracted "// Actual pinned glow geometry and float vector operations\n")
foreach(
    signature IN
    ITEMS "inline CFloatVector3& CFloatVector3::Assign (const CFixVector&"
          "inline const float CFloatVector3::SqrMag ("
          "inline const float CFloatVector3::Mag ("
          "inline const CFloatVector3& CFloatVector3::operator+= (const CFloatVector3&"
          "inline const CFloatVector3& CFloatVector3::operator/= (const float"
          "inline const CFloatVector3 CFloatVector3::operator- (const CFloatVector3&"
          "inline CFloatVector& CFloatVector::Assign (const CFixVector&"
          "inline const float CFloatVector::SqrMag ("
          "inline const float CFloatVector::Mag ("
          "inline const float CFloatVector::Normalize ("
          "inline const CFloatVector CFloatVector::Avg ("
          "inline const CFloatVector CFloatVector::Cross ("
          "inline const CFloatVector CFloatVector::Perp ("
          "inline const CFloatVector CFloatVector::Normal ("
          "inline const CFloatVector& CFloatVector::operator+= (const CFloatVector&"
          "inline const CFloatVector& CFloatVector::operator*= (const float"
          "inline const CFloatVector& CFloatVector::operator/= (const float"
          "inline const CFloatVector CFloatVector::operator+ (const CFloatVector&"
          "inline const CFloatVector CFloatVector::operator- (const CFloatVector&"
          "inline const CFloatVector CFloatVector::operator* (const float"
          "inline const CFloatVector CFloatVector::operator/ (const float")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/include/vecmat.h" "${signature}")
endforeach()
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/effects/lightning.cpp" source)
foreach(
    signature IN
    ITEMS "static tTexCoord2f plasmaTexCoord" "float CLightning::ComputeDistScale ("
          "void CLightning::ComputeGlow (" "float CLightning::ComputeAvgDist ("
          "void CLightning::ComputeCore (" "void CLightning::Draw (")
    string(FIND "${source}" "${signature}" start)
    if(start LESS 0)
        message(FATAL_ERROR "Missing lightning glow source: ${signature}")
    endif()
    string(SUBSTRING "${source}" ${start} -1 tail)
    string(FIND "${tail}"
                "\n//------------------------------------------------------------------------------"
                end)
    if(end LESS 0)
        message(FATAL_ERROR "Missing lightning glow separator: ${signature}")
    endif()
    string(SUBSTRING "${tail}" 0 ${end} fragment)
    string(APPEND extracted "\n${fragment}")
endforeach()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_lightning_glow.h"
     CONTENT "${extracted}")
target_sources(xl_lightning_test PRIVATE tests/reference_lightning_render.cpp)
