foreach(
    pair IN
    ITEMS "render/cameras.cpp|607e48f8ee93ff3edacfb44ab824c2ee67bf7e8c38c7aff06459b61957ef4ac1"
          "include/cameras.h|3b54e3cac755c8ee4963774f53cd50fe81166c18b8697b496098a4e5ea0f4882"
          "include/cockpit.h|98a1627a9b7608ab481b82e53a36d4fb00829a69a35af4dfb84300cf3ccb59a3"
          "include/object.h|ed546edd2bbda56a84e9fef8543706707d47a08b48e36616852346b5a2f96127"
          "gameio/loadobjects.cpp|01fe3cbc1cf781d1180921c9e235593f1b79d68924c0d49a42502ef8c3ffcd3c"
          "objects/object.cpp|e71b0a906cd686549c783563ab93365d1bbf5cf05f08c1971ef08c7116c67a64"
          "include/aistruct.h|25950b637d9c7308a63d386bbf28d4a7613c14474e0373b0cf5a8cee2a3ccf1f")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Camera reference differs from audited revision: ${path}")
    endif()
endforeach()
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/object.h" object_header)
string(REGEX MATCHALL "#define[ \t]+OF_(EXPLODING|SHOULD_BE_DEAD|DESTROYED)[ \t]+[0-9]+" flags
             "${object_header}")
string(REPLACE ";" "\n" flags "${flags}")
set(extracted "// Actual pinned camera readiness, reset and manager selection\n${flags}\n")
foreach(signature IN ITEMS "int32_t CCamera::Ready (" "void CCamera::Reset ("
                           "int32_t CCameraManager::Render (" "void CCamera::Align (")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/render/cameras.cpp" "${signature}")
endforeach()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_cameras.h" CONTENT "${extracted}")
target_sources(xl_effect_reference PRIVATE tests/reference_cameras.cpp)

file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/cockpit.h" cockpit_header)
string(REGEX MATCH "#define[ \t]+CM_FULL_SCREEN[ \t]+[0-9]+" full_screen "${cockpit_header}")
if(NOT full_screen)
    message(FATAL_ERROR "Missing source full-screen cockpit constant")
endif()
set(extracted "// Actual ordinary camera creation and capture transaction\n${full_screen}\n")
foreach(signature IN ITEMS "void CCamera::Init (" "int32_t CCamera::CreateBuffer ("
                           "int32_t CCamera::Create (" "int32_t CCamera::Render (")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/render/cameras.cpp" "${signature}")
endforeach()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_camera_capture.h"
     CONTENT "${extracted}")

set(extracted "// Hash-pinned camera manager topology with allocation/view setup adapters\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/render/cameras.cpp"
                      "int32_t CCameraManager::Create (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/gameio/loadobjects.cpp"
                      "int32_t CmpObjTriggers (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/gameio/loadobjects.cpp"
                      "void BuildObjTriggerRef (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_camera_bindings.h"
     CONTENT "${extracted}")
target_sources(xl_effect_reference PRIVATE tests/reference_camera_bindings.cpp)

file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/objects/robot.cpp" robot_hash)
if(NOT robot_hash STREQUAL "a33aee40ef05eb35d41538c75190df09bb744cd484146c6a3c6afb7a807818a3")
    message(FATAL_ERROR "Camera initialization source changed")
endif()
set(extracted "// Actual pinned camera object initialization and old-model size ordering\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/objects/robot.cpp" "void InitCamBots (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/objects/object.cpp"
                      "fix CObject::ModelRadius (")
string(REGEX MATCH "inline void AdjustSize \\([^}]+}" adjust_size "${object_header}")
if(NOT adjust_size)
    message(FATAL_ERROR "Missing pinned inline camera size adjustment")
endif()
string(APPEND extracted "\n${adjust_size}\n")
string(REPLACE "inline void AdjustSize (int32_t i = 0, fix scale = 0)"
               "void CObject::AdjustSize (int32_t i, fix scale)" extracted "${extracted}")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_camera_objects.h"
     CONTENT "${extracted}")
target_sources(xl_effect_reference PRIVATE tests/reference_camera_objects.cpp)

set(extracted "// Actual hash-pinned camera heading controller; matrix adapters observe calls\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/render/cameras.cpp" "void CCamera::Rotate (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_camera_rotation.h"
     CONTENT "${extracted}")

foreach(
    pair IN
    ITEMS "physics/physics.cpp|2ca8d5c939f31da9d4686637ed3e4305bf89e208a575f2f7f6ea2b4d2e6fab76"
          "include/vecmat.h|83d7f73f89a2f588a0b56a08fb9e293b747cc65a5ed22bad6771d64319bdca0f"
          "maths/vecmat.cpp|4115afe844ad77a81e9f0de57ae1bbf577b06d5f49aed8b02d19002ff2021335"
          "maths/fixc.cpp|da4a96862a231c07720bb31ff42eddd39174e1708e6ae1678ded61375c5b1c2c"
          "maths/tables.cpp|107434ea4cd7ccbe4d756d4f89006bc566d00d5c5fdac785ff6c31fea79c0926")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Camera force reference differs: ${path}")
    endif()
endforeach()
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/maths.h" math_source)
set(camera_math
    "#undef FixMul\n#undef FixDiv\n#undef FixMulDiv\n#undef XRound\n#undef I2X\n#undef X2I\n#undef F2X\n#undef X2F\n"
)
foreach(
    name IN
    ITEMS XRound
          FixMul
          FixDiv
          FixMulDiv
          I2X
          X2I
          F2X
          X2F)
    string(REGEX MATCH "#[ \t]*define ${name}\\([^\n]+" math_macro "${math_source}")
    if(NOT math_macro)
        message(FATAL_ERROR "Missing pinned camera math macro: ${name}")
    endif()
    string(APPEND camera_math "${math_macro}\n")
endforeach()
set(extracted
    "// Actual release camera-contact force consumer and vector scaling\n${camera_math}\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/include/vecmat.h"
                      "inline const CFixVector CFixVector::operator* (const fix s) const")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/physics/physics.cpp"
                      "void CObject::ApplyForce (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/include/vecmat.h"
                      "inline const CFixVector& CFixVector::operator*= (const float s)")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_camera_force.h"
     CONTENT "${extracted}")

file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/physics/collide.cpp" collision_hash)
file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/include/maths.h" random_hash)
if(NOT collision_hash STREQUAL "d10f88f82e5d283077914881d1acb719f06785a86be22e50617d92d6fce47bc9"
   OR NOT random_hash STREQUAL "43adf417dd1c282a39c8fed02bdad9d05a7f3ad5850a39a87897fd533eb857cd")
    message(FATAL_ERROR "Camera Gauss reference differs from pinned source")
endif()
set(extracted "// Actual pinned signed RNG and Gauss collision branch\n${camera_math}\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/include/maths.h"
                      "static inline int32_t SRandShort (")
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/physics/collide.cpp" collision_source)
string(FIND "${collision_source}" "if (pRobot && (!pRobotInfo ||" gauss_start)
string(SUBSTRING "${collision_source}" ${gauss_start} -1 gauss_tail)
string(FIND "${gauss_tail}" "\n\t}\nMaybeKillWeapon" gauss_end)
if(gauss_start LESS 0 OR gauss_end LESS 0)
    message(FATAL_ERROR "Missing pinned camera Gauss branch")
endif()
string(SUBSTRING "${gauss_tail}" 0 ${gauss_end} gauss_body)
string(APPEND extracted
       "\nvoid spin(CObject *pRobot) {\ntRobotInfo *pRobotInfo = nullptr;\n${gauss_body}\n}\n")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_camera_gauss.h"
     CONTENT "${extracted}")

file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/effects/fireball.cpp" splash_hash)
if(NOT splash_hash STREQUAL "e91c43142cbb1f85c0274aac19e97c95d62e95bc5aa31c1d2537db2113387bf4")
    message(FATAL_ERROR "Camera splash reference differs from pinned source")
endif()
set(extracted "// Actual pinned splash position and radial geometry\n${camera_math}\n")
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/maths/tables.cpp" splash_tables)
foreach(table IN ITEMS asin_table acos_table)
    string(REGEX MATCH "[a-z0-9_]+ ${table} \\[\\] = \\{[^}]*\\};" fragment "${splash_tables}")
    if(NOT fragment)
        message(FATAL_ERROR "Missing source splash angle table: ${table}")
    endif()
    string(APPEND extracted "${fragment}\n")
endforeach()
foreach(signature IN ITEMS "fixang FixASin (" "fixang FixACos(" "fixang FixAtan2 (")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/maths/fixc.cpp" "${signature}")
endforeach()
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/maths/vecmat.cpp"
                      "fix CFixVector::Mag (void) const")
foreach(
    signature IN
    ITEMS
        "inline const fix CFixVector::Dist (const CFixVector& vec0, const CFixVector& vec1)"
        "inline const fix CFixVector::Normalize (CFixVector& v)"
        "inline const fix CFixVector::NormalizedDir (CFixVector& dest, const CFixVector& end, const CFixVector& start)"
        "inline const CAngleVector CFixVector::ToAnglesVecNorm (void) const"
        "inline const CAngleVector CFixVector::ToAnglesVec (void) const"
        "inline const CFixVector& CFixVector::operator*= (const fix s)"
        "inline const CFixVector& CFixVector::operator/= (const fix s)")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/include/vecmat.h" "${signature}")
endforeach()
foreach(signature IN ITEMS "void PhysicsSetRotVelAndSaturate (" "void CObject::TurnTowardsVector ("
                           "void CObject::ApplyRotForce (")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/physics/physics.cpp" "${signature}")
endforeach()
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/effects/fireball.cpp"
                      "CObject* CObject::ExplodeSplashDamageWeapon (")
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/effects/fireball.cpp" splash_source)
string(FIND "${splash_source}" "dist = CFixVector::Dist (OBJPOS (pObj)->vPos, vImpact);"
            radial_start)
string(SUBSTRING "${splash_source}" ${radial_start} -1 radial_tail)
string(FIND "${radial_tail}" "\n\tif (nType == OBJ_WEAPON)" radial_end)
if(radial_start LESS 0 OR radial_end LESS 0)
    message(FATAL_ERROR "Missing pinned radial geometry block")
endif()
string(SUBSTRING "${radial_tail}" 0 ${radial_end} radial_body)
string(
    APPEND
    extracted
    "\nbool radial(CObject *pObj, CFixVector vImpact, fix xMaxDamage, fix xMaxDistance, fix xMaxForce) {\nCObject *pExplObj = nullptr; fix dist, damage, force; CFixVector vForce, vHit;\nfor (int once = 0; once < 1; ++once) {\n${radial_body}\nradial_result = {dist, damage, force, vForce}; return true;\n} return false;\n}\n"
)
# Compile exactly the source pre-damage guards, before unrelated death handling
string(APPEND extracted "\n${flags}\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/physics/collide.cpp"
                      "bool CObject::Indestructible (")
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/physics/collide.cpp" damage_source)
string(FIND "${damage_source}" "int32_t CObject::ApplyDamageToRobot (" damage_start)
string(SUBSTRING "${damage_source}" ${damage_start} -1 damage_tail)
string(FIND "${damage_tail}" "if ((bIsBoss = IsBoss ()))" damage_end)
if(damage_start LESS 0 OR damage_end LESS 0)
    message(FATAL_ERROR "Missing original robot damage preconditions")
endif()
string(SUBSTRING "${damage_tail}" 0 ${damage_end} damage_prefix)
string(APPEND extracted "\n${damage_prefix}return 1;\n}\n")
file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/include/vclip.h" motion_clip_hash)
if(NOT motion_clip_hash STREQUAL "995af8528e7838cdc5439c632beca3b4c2a5eef3996acb6147979a89644fb1ef")
    message(FATAL_ERROR "Explosion motion clip constants changed")
endif()
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/vclip.h" motion_clips)
string(REGEX MATCHALL
             "#define[ \t]+ANIM_(MORPHING_ROBOT|POWERUP_DISAPPEARANCE|AFTERBURNER_BLOB)[ \t]+[0-9]+"
             motion_constants "${motion_clips}")
string(REPLACE ";" "\n" motion_constants "${motion_constants}")
string(APPEND extracted "\n${motion_constants}\n")
string(REGEX MATCHALL "#define[ \t]+RT_(EXPLBLAST|SHOCKWAVE|SHRAPNELS|FIREBALL)[ \t]+[0-9]+"
             motion_render_constants "${object_header}")
string(REPLACE ";" "\n" motion_render_constants "${motion_render_constants}")
string(APPEND extracted "\n${motion_render_constants}\n")
string(FIND "${splash_source}" "if ((nVClip != ANIM_MORPHING_ROBOT)" motion_init_start)
string(SUBSTRING "${splash_source}" ${motion_init_start} -1 motion_init_tail)
string(FIND "${motion_init_tail}" "pExplObj->cType.explInfo.nSpawnTime" motion_init_end)
string(SUBSTRING "${motion_init_tail}" 0 ${motion_init_end} motion_init)
string(FIND "${splash_source}" "//See if we should die of old age" motion_frame_start)
string(SUBSTRING "${splash_source}" ${motion_frame_start} -1 motion_frame_tail)
string(FIND "${motion_frame_tail}" "//See if we should create a secondary explosion"
            motion_frame_end)
if(motion_init_start LESS 0 OR motion_init_end LESS 0 OR motion_frame_start LESS 0
   OR motion_frame_end LESS 0)
    message(FATAL_ERROR "Missing source explosion motion consumers")
endif()
string(SUBSTRING "${motion_frame_tail}" 0 ${motion_frame_end} motion_frame)
string(
    APPEND
    extracted
    "\nvoid initialize_motion(CObject *pExplObj, int nVClip) {\n${motion_init}\n}\nvoid CObject::MoveExplosion() {\n${motion_frame}\n}\n"
)
# Flash robot stun is extracted verbatim, including the explosion-object skip counter
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/include/maths.h"
                      "static inline int32_t SRandShort (")
string(FIND "${splash_source}" "if (flash && !pObj->IsBoss ())" flash_start)
string(SUBSTRING "${splash_source}" ${flash_start} -1 flash_tail)
string(FIND "${flash_tail}" "#if 1" flash_end)
if(flash_start LESS 0 OR flash_end LESS 0)
    message(FATAL_ERROR "Missing source flash stun block")
endif()
string(SUBSTRING "${flash_tail}" 0 ${flash_end} flash_body)
string(
    APPEND
    extracted
    "\nvoid flash_robot(CObject *pObj, CObject *pExplObj, CFixVector vForce, int flash) {\n${flash_body}\n}\n"
)
string(REPLACE "void CObject::ApplyRotForce (" "void CObject::ApplyRotForceSource (" extracted
               "${extracted}")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_camera_splash.h"
     CONTENT "${extracted}")
target_sources(xl_effect_reference PRIVATE tests/reference_camera_splash.cpp)

# Full camera matrix path, independently compiled from the pinned vector implementation
set(extracted "// Actual camera matrix composition and repair\n${camera_math}\n")
foreach(name IN ITEMS F2X mul64)
    string(REGEX MATCH "#[ \t]*define ${name}\\([^\n]+" matrix_macro "${math_source}")
    if(NOT matrix_macro)
        message(FATAL_ERROR "Missing camera matrix macro: ${name}")
    endif()
    string(APPEND extracted "${matrix_macro}\n")
endforeach()
string(REGEX MATCH "[a-z0-9_]+ sincos_table \\[\\] = \\{[^}]*\\};" matrix_sine_table
             "${splash_tables}")
if(NOT matrix_sine_table)
    message(FATAL_ERROR "Missing camera matrix sine table")
endif()
string(APPEND extracted "${matrix_sine_table}\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/maths/fixc.cpp" "void FixSinCos (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/maths/vecmat.cpp"
                      "fix CFixVector::Mag (void) const")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/maths/vecmat.cpp" "fix FixQuadAdjust (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/maths/vecmat.cpp"
                      "const CFixVector CFixVector::Cross (")
foreach(
    signature IN
    ITEMS
        "inline const fix CFixVector::Normalize (CFixVector& v)"
        "inline const fix CFixVector::Dot (const CFixVector& v0, const CFixVector& v1)"
        "inline const CFixMatrix CFixMatrix::Create (fix sinp, fix cosp, fix sinb, fix cosb, fix sinh, fix cosh)"
        "inline const CFixMatrix CFixMatrix::Create (const CAngleVector& a)"
        "inline void CFixMatrix::CheckAndFix (void)")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/include/vecmat.h" "${signature}")
endforeach()
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/vecmat.h" camera_vector_source)
string(REGEX MATCH "inline CFixMatrix CFixMatrix::operator[^\n]+return Mul[^\n]+" matrix_operator
             "${camera_vector_source}")
if(NOT matrix_operator)
    message(FATAL_ERROR "Missing inline camera matrix multiply")
endif()
string(APPEND extracted "\n${matrix_operator}\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/maths/vecmat.cpp"
                      "CFixMatrix CFixMatrix::Mul (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/maths/vecmat.cpp"
                      "const CFixMatrix CFixMatrix::CreateFU (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/render/cameras.cpp" "void CCamera::Rotate (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_camera_matrix.h"
     CONTENT "${extracted}")
target_sources(xl_effect_reference PRIVATE tests/reference_camera_matrix.cpp)

# Monitor surface selection; actual release helper, ordinary-camera profile only
file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/render/renderlib.cpp" monitor_hash)
if(NOT monitor_hash STREQUAL "d3eacfaf28c9687e7e37c3e47333b11d4a026000839231e8a71b443bc86d4252")
    message(FATAL_ERROR "Camera monitor source changed")
endif()
set(extracted "// Actual source monitor selection and lookup gate\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/render/renderlib.cpp"
                      "int32_t IsMonitorFace (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/render/renderlib.cpp"
                      "int32_t SetupMonitorFace (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_camera_monitor.h"
     CONTENT "${extracted}")
target_sources(xl_effect_reference PRIVATE tests/reference_camera_monitor.cpp)

# Actual camera-view effects dispatcher and renderer predicates
foreach(
    pair IN
    ITEMS "render/rendermine.cpp|f433b94da415fea1466c17af7feecb1bf05af84ec5831d2b741672fc70db50d8"
          "render/rendersetup.cpp|3632952f30e5f569793eb0fbcf1ea2b70154afaf5875fd6c2ece90275470cac6")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Camera view source changed: ${path}")
    endif()
endforeach()
set(extracted "// Actual effect dispatcher and verbatim renderer predicate statements\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/render/rendermine.cpp" "void RenderEffects (")
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/render/rendersetup.cpp" view_source)
string(REGEX MATCH "gameStates.render.bDoCameras = extraGameInfo[^;]+;" monitor_gate
             "${view_source}")
string(
    REGEX
        MATCH
        "return !gameStates.render.cameras.bActive && \\(gameData.objData.pViewer->info.nType != OBJ_ROBOT\\);"
        map_gate
        "${view_source}")
if(NOT monitor_gate OR NOT map_gate)
    message(FATAL_ERROR "Missing source camera monitor/automap predicates")
endif()
string(
    APPEND extracted
    "\nvoid source_monitor_gate() { ${monitor_gate} }\nint source_automap_gate() { ${map_gate} }\n")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_camera_view.h"
     CONTENT "${extracted}")
target_sources(xl_effect_reference PRIVATE tests/reference_camera_view.cpp)

file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/2d/palette.cpp" palette_hash)
if(NOT palette_hash STREQUAL "7965378da2b783848c5f068416c3b4650a15dc902340ea0af1da4791f3f79419")
    message(FATAL_ERROR "Camera flash palette source changed")
endif()
set(extracted "// Actual source flash palette timing and compositing\n${camera_math}\n")
foreach(
    signature IN
    ITEMS "void CPaletteManager::StartEffect (float"
          "void CPaletteManager::StartEffect (int32_t"
          "void CPaletteManager::StopEffect (void)"
          "void CPaletteManager::BumpEffect (int32_t"
          "void CPaletteManager::BumpEffect (float"
          "void CPaletteManager::UpdateEffect ("
          "void CPaletteManager::FadeEffect (")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/2d/palette.cpp" "${signature}")
endforeach()
file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/include/palette.h" palette_header_hash)
if(NOT palette_header_hash STREQUAL
   "3394f7accb1e7e53679fe94ad54256134acdf7a5a374d03756cab7c3f1f6d7e9")
    message(FATAL_ERROR "Camera flash palette header changed")
endif()
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/palette.h" palette_header)
foreach(method IN ITEMS FadeScale SetFadeDuration RedEffect GreenEffect BlueEffect SetFadeDelay)
    string(REGEX MATCH "inline [a-zA-Z0-9_]+ ${method} [(][^\n]+" fragment "${palette_header}")
    if(NOT fragment)
        message(FATAL_ERROR "Missing source palette method ${method}")
    endif()
    string(REPLACE "${method} (" "CPaletteManager::${method} (" fragment "${fragment}")
    string(REPLACE "bool bFade = false" "bool bFade" fragment "${fragment}")
    string(APPEND extracted "\n${fragment}\n")
endforeach()
string(FIND "${splash_source}" "if (flash && (pObj->info.nId == N_LOCALPLAYER))" pulse_start)
string(SUBSTRING "${splash_source}" ${pulse_start} -1 pulse_tail)
string(FIND "${pulse_tail}" "if (pParent && IsMultiGame" pulse_end)
if(pulse_start LESS 0 OR pulse_end LESS 0)
    message(FATAL_ERROR "Missing source player flash pulse")
endif()
string(SUBSTRING "${pulse_tail}" 0 ${pulse_end} pulse_body)
string(
    APPEND
    extracted
    "\nvoid player_pulse(CObject *pObj, CObject *pParent, int force, int flash) {\n${pulse_body}\n}\n"
)
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_camera_flash.h"
     CONTENT "${extracted}")
target_sources(xl_effect_reference PRIVATE tests/reference_camera_flash.cpp)

include(${CMAKE_CURRENT_LIST_DIR}/reference_camera_children.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/reference_camera_glass.cmake)

# Full static bump implementation uses the already pinned collision and vector sources
set(extracted "// Actual pinned static contact and force composition\n")
foreach(
    signature IN
    ITEMS "inline const CFixVector CFixVector::operator* (const fix s) const"
          "inline const CFixVector CFixVector::operator/ (const fix d) const"
          "inline const fix CFixVector::Dot (const CFixVector&"
          "inline CFixVector& CFixVector::Neg (void)"
          "inline const CFixVector& CFixVector::operator*= (const float s)")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/include/vecmat.h" "${signature}")
endforeach()
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/physics/collide.cpp" static_source)
string(FIND "${static_source}" "class CBumpForces {" static_class_start)
string(SUBSTRING "${static_source}" ${static_class_start} -1 static_tail)
string(FIND "${static_tail}" "\n};" static_class_end)
if(static_class_start LESS 0 OR static_class_end LESS 0)
    message(FATAL_ERROR "Missing pinned bump force class")
endif()
math(EXPR static_class_end "${static_class_end} + 3")
string(SUBSTRING "${static_tail}" 0 ${static_class_end} static_class)
string(APPEND extracted "\n${static_class}\n")
foreach(
    signature IN
    ITEMS "void CObject::Bump (CObject* pOtherObj, CFixVector vForce, CFixVector vRotForce"
          "inline void CBumpForces::Compute (" "inline void CBumpForces::Bump ("
          "int32_t BumpTwoObjects (")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/physics/collide.cpp" "${signature}")
endforeach()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_static_contact.h"
     CONTENT "${extracted}")
