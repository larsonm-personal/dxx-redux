file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/physics/physics.cpp" skybox_physics_hash)
if(NOT skybox_physics_hash STREQUAL
   "2ca8d5c939f31da9d4686637ed3e4305bf89e208a575f2f7f6ea2b4d2e6fab76")
    message(FATAL_ERROR "Skybox collision source differs from the audited revision")
endif()
set(extracted "// Pinned skybox collision body; storage and external queries are adapters\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/physics/physics.cpp"
                      "int32_t CObject::HandleWallCollision (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_skybox_collision.h"
     CONTENT "${extracted}")
target_sources(xl_effect_reference PRIVATE tests/reference_skybox.cpp)
