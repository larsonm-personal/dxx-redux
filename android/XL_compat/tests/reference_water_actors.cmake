# Physics and motion math inputs are pinned by reference_speed_boost.cmake
set(extracted "// Pinned XL fluid motion consumers\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/physics/physics.cpp"
                      "void CPhysSimData::Setup (void)")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/physics/physics.cpp"
                      "int32_t CObject::UpdateOffset (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/physics/physics.cpp"
                      "void CObject::ProcessDrag (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/physics/physics.cpp"
                      "void CObject::ComputeMovedTime (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/physics/physics.cpp"
                      "void CObject::FinishPhysicsSim (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_water_actors.h"
     CONTENT "${extracted}")
add_executable(xl_water_actors_reference tests/reference_water_actors.cpp)
target_include_directories(xl_water_actors_reference PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(xl_water_actors_reference PRIVATE cxx_std_17)
add_test(NAME xl_contact_control_flow_reference COMMAND xl_water_actors_reference
                                                        --contact-control-flow)
set_tests_properties(xl_contact_control_flow_reference PROPERTIES TIMEOUT 10)

file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/objects/updateobject.cpp" update_hash)
if(NOT update_hash STREQUAL "c4f3b9f1cd225f4dda99bb2a4e42af8546bc37ae36cc0bd19091dad6f48b01ac")
    message(FATAL_ERROR "Object lifetime source differs from pinned audit")
endif()
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/objects/updateobject.cpp" update_source)
string(FIND "${update_source}" "if ((info.xLifeLeft != IMMORTAL_TIME)" lifetime_start)
if(lifetime_start LESS 0)
    message(FATAL_ERROR "Missing pinned lifetime start")
endif()
string(SUBSTRING "${update_source}" ${lifetime_start} -1 lifetime_tail)
string(FIND "${lifetime_tail}" "gameStates.render.bDropAfterburnerBlob" lifetime_end)
if(lifetime_start LESS 0 OR lifetime_end LESS 0)
    message(FATAL_ERROR "Missing pinned lifetime fragment")
endif()
string(SUBSTRING "${lifetime_tail}" 0 ${lifetime_end} lifetime_fragment)
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_slow_lifetime.h"
     CONTENT "${lifetime_fragment}")

# The enclosing physics file is hash-pinned by reference_speed_boost.cmake
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/physics/physics.cpp" rotation_source)
string(FIND "${rotation_source}"
            "turnAngles.v.coord.b = fixang (FixMul (mType.physInfo.rotVel.v.coord.z, t));"
            rotation_start)
if(rotation_start LESS 0)
    message(FATAL_ERROR "Missing pinned rotation start")
endif()
string(SUBSTRING "${rotation_source}" ${rotation_start} -1 rotation_tail)
string(FIND "${rotation_tail}" "if (!IsMultiGame)" rotation_start)
string(FIND "${rotation_tail}" "mRotate = CFixMatrix::Create (turnAngles);" rotation_end)
if(rotation_start LESS 0 OR rotation_end LESS rotation_start)
    message(FATAL_ERROR "Missing pinned rotation boundaries")
endif()
math(EXPR rotation_length "${rotation_end} - ${rotation_start}")
string(SUBSTRING "${rotation_tail}" ${rotation_start} ${rotation_length} rotation_fragment)
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_slow_rotation.h"
     CONTENT "${rotation_fragment}")

set(extracted "")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/physics/physics.cpp"
                      "void PhysicsSetRotVelAndSaturate (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_slow_saturate.h"
     CONTENT "${extracted}")
string(FIND "${rotation_source}" "delta_p = (dest_angles.v.coord.p - cur_angles.v.coord.p);"
            turn_start)
if(turn_start LESS 0)
    message(FATAL_ERROR "Missing pinned turning start")
endif()
string(SUBSTRING "${rotation_source}" ${turn_start} -1 turn_tail)
string(FIND "${turn_tail}" "RETURN" turn_end)
if(turn_end LESS 0)
    message(FATAL_ERROR "Missing pinned turning end")
endif()
string(SUBSTRING "${turn_tail}" 0 ${turn_end} turn_fragment)
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_slow_turn.h"
     CONTENT "${turn_fragment}")

# Preserve both the caller's early return and the complete reconstruction guard The adapters supply
# post-callback death state, not an upstream collision engine
function(extract_contact_fragment source scope first last output)
    string(FIND "${source}" "${scope}" scope_start)
    if(scope_start LESS 0)
        message(FATAL_ERROR "Missing contact scope: ${scope}")
    endif()
    string(SUBSTRING "${source}" ${scope_start} -1 scope_tail)
    string(FIND "${scope_tail}" "${first}" first_pos)
    if(first_pos LESS 0)
        message(FATAL_ERROR "Missing contact fragment start: ${output}")
    endif()
    string(SUBSTRING "${scope_tail}" ${first_pos} -1 tail)
    string(FIND "${tail}" "${last}" last_pos)
    if(last_pos LESS 1)
        message(FATAL_ERROR "Missing contact fragment end: ${output}")
    endif()
    string(SUBSTRING "${tail}" 0 ${last_pos} fragment)
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${output}" CONTENT "${fragment}")
endfunction()
extract_contact_fragment(
    "${rotation_source}" "int32_t CObject::ProcessWallCollision ("
    "if (info.nFlags & OF_SHOULD_BE_DEAD)" "if (info.nType == OBJ_DEBRIS)"
    xl_reference_contact_death.h)
extract_contact_fragment(
    "${rotation_source}" "void CObject::DoPhysicsSim (void)"
    "if (bRetry < 0) {\n\t\tFinishPhysicsSim (simData);" "if (!bRetry)"
    xl_reference_contact_return.h)
extract_contact_fragment(
    "${rotation_source}" "void CObject::FixPosition (" "if (((simData.hitResult.nType == HIT_WALL)"
    "if (mType.physInfo.flags & PF_LEVELLING)" xl_reference_contact_reconstruct.h)
file(READ "${CMAKE_CURRENT_LIST_DIR}/../../../d2/main/physics.c" redux_physics)
set_property(
    DIRECTORY APPEND
    PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/../../../d2/main/physics.c"
             "${XL_REFERENCE_GEOMETRY_ROOT}/physics/physics.cpp")
extract_contact_fragment(
    "${redux_physics}" "void do_physics_sim(object *obj)" "int reconstruct_velocity ="
    "if (!obj_stopped && !bounced" xl_redux_contact_predicate.h)

file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/ai/aiframe.cpp" ai_hash)
if(NOT ai_hash STREQUAL "9fdc24b85cdfc7282830a0cc6e5a52fce9390151ecb4f157080e919c9c9046fd")
    message(FATAL_ERROR "AI timer reference differs from pinned source")
endif()
set(extracted "")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/ai/aiframe.cpp" "int32_t AINextFireHandler (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_slow_ai.h" CONTENT "${extracted}")
file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/ai/aimove.cpp" aimove_hash)
if(NOT aimove_hash STREQUAL "2aef988961c51464669d280d9073e2eceb98a8da45041a43d503b667005b5941")
    message(FATAL_ERROR "AI turn reference differs from pinned source")
endif()
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/ai/aimove.cpp" aimove_source)
foreach(value IN ITEMS dot new_scale)
    set(needle "${value} = (fix) (${value} / gameStates.gameplay.slowmo [0].fSpeed);")
    string(FIND "${aimove_source}" "${needle}" scaled_start)
    if(scaled_start LESS 0)
        message(FATAL_ERROR "Missing pinned AI scaling expression")
    endif()
    string(LENGTH "${needle}" scaled_length)
    string(SUBSTRING "${aimove_source}" ${scaled_start} ${scaled_length} scaled)
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_slow_ai_${value}.h"
         CONTENT "if (!IsMultiGame) { ${scaled} }\n")
endforeach()

foreach(
    pair IN
    ITEMS "weapons/omega.cpp|76f003622dcbac54dbd533fd4dfa8537c7ceceed0c75c5310c83b7b075cfb61d"
          "weapons/fireweapon.cpp|37ae81f686e063ca7102bb9154368e67626e843ae898d82e8583167d31dbaac8")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Slow weapon reference differs: ${path}")
    endif()
endforeach()
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/weapons/omega.cpp" omega_source)
string(REGEX MATCH "#define[ \t]+OMEGA_ENERGY_RATE[^\n]*" omega_rate "${omega_source}")
string(REGEX MATCH "#define[ \t]+OMEGA_CHARGE_SCALE[^\n]*" omega_scale "${omega_source}")
if(NOT omega_rate OR NOT omega_scale)
    message(FATAL_ERROR "Missing pinned Omega constants")
endif()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_slow_omega_constants.h"
     CONTENT "${omega_rate}\n${omega_scale}\n")

set(extracted "")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/weapons/omega.cpp" "fix OmegaEnergy (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_slow_omega_energy.h"
     CONTENT "${extracted}")
set(needle
    "(fix) (gameData.timeData.xFrame / OMEGA_CHARGE_SCALE / gameStates.gameplay.slowmo [0].fSpeed)")
string(FIND "${omega_source}" "${needle}" omega_start)
if(omega_start LESS 0)
    message(FATAL_ERROR "Missing pinned Omega charge arithmetic")
endif()
string(LENGTH "${needle}" omega_length)
string(SUBSTRING "${omega_source}" ${omega_start} ${omega_length} omega_step)
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_slow_omega_step.h"
     CONTENT "return ${omega_step};\n")
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/weapons/fireweapon.cpp" weapon_source)
set(needle "((fix) (frameTime * 16 / gameStates.gameplay.slowmo [0].fSpeed))")
string(FIND "${weapon_source}" "${needle}" homing_start)
if(homing_start LESS 0)
    message(FATAL_ERROR "Missing pinned homing orientation arithmetic")
endif()
string(LENGTH "${needle}" homing_length)
string(SUBSTRING "${weapon_source}" ${homing_start} ${homing_length} homing_step)
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_slow_homing_step.h"
     CONTENT "return ${homing_step};\n")

file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/weapons/controlweapon.cpp" control_hash)
if(NOT control_hash STREQUAL "1caa0d8a33f755260693a6083ac973593bf0533f2063815de72e66504d75d4b6")
    message(FATAL_ERROR "Firing deadline reference differs from pinned source")
endif()
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/weapons/controlweapon.cpp" control_source)
foreach(kind IN ITEMS primary flare)
    if(kind STREQUAL "primary")
        set(start_text "if (!IsMultiGame &&")
    else()
        set(start_text "if ((xNextFlareFireTime >")
    endif()
    string(FIND "${control_source}" "${start_text}" start)
    if(start LESS 0)
        message(FATAL_ERROR "Missing reference deadline start")
    endif()
    string(SUBSTRING "${control_source}" ${start} -1 tail)
    string(FIND "${tail}" "return 1;" end)
    if(end LESS 0)
        message(FATAL_ERROR "Missing reference deadline end")
    endif()
    math(EXPR length "${end} + 9")
    string(SUBSTRING "${tail}" 0 ${length} fragment)
    string(REPLACE "gameData.laserData.xLastFiredTime" "last" fragment "${fragment}")
    string(REPLACE "gameData.laserData.xNextFireTime" "next" fragment "${fragment}")
    string(REPLACE "gameData.timeData.xGame" "now" fragment "${fragment}")
    string(REPLACE "gameStates.gameplay.slowmo [0].fSpeed" "speed" fragment "${fragment}")
    string(REPLACE "IsMultiGame" "false" fragment "${fragment}")
    string(REPLACE "xNextFlareFireTime" "next" fragment "${fragment}")
    string(REPLACE "LOCALPLAYER.Energy () >= WI_EnergyUsage (FLARE_ID)" "enough" fragment
                   "${fragment}")
    string(REPLACE "FLARE_BIG_DELAY" "I2X (2)" fragment "${fragment}")
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_slow_${kind}_deadline.h"
         CONTENT "${fragment}\n")
endforeach()
