# Physics and motion math inputs are pinned by reference_speed_boost.cmake
set(extracted "// Pinned XL fluid motion consumers\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/physics/physics.cpp"
                      "void CPhysSimData::Setup (void)")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/physics/physics.cpp"
                      "int32_t CObject::UpdateOffset (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/physics/physics.cpp"
                      "void CObject::ProcessDrag (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_water_actors.h"
     CONTENT "${extracted}")
add_executable(xl_water_actors_reference tests/reference_water_actors.cpp)
target_include_directories(xl_water_actors_reference PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(xl_water_actors_reference PRIVATE cxx_std_17)

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
