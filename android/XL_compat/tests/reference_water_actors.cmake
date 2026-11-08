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
