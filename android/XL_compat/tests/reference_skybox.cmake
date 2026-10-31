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

foreach(
    pair IN
    ITEMS "render/renderlib.cpp|d3eacfaf28c9687e7e37c3e47333b11d4a026000839231e8a71b443bc86d4252"
          "render/rendermine.cpp|f433b94da415fea1466c17af7feecb1bf05af84ec5831d2b741672fc70db50d8"
          "main/wall.cpp|05eaa2cab8dd7f5661afd210e0edc8dea4fcd2bb2075c00d3012bec395ab0c42"
          "include/palette.h|3394f7accb1e7e53679fe94ad54256134acdf7a5a374d03756cab7c3f1f6d7e9")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Skybox visibility source differs: ${path}")
    endif()
endforeach()
set(extracted "// Pinned visibility and skybox pass gate; external state uses adapters\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/render/renderlib.cpp"
                      "int32_t FaceIsVisible (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/render/rendermine.cpp"
                      "void RenderSkyBox (int32_t nWindow)\n{")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/wall.cpp"
                      "bool CWall::IsInvisible (void)")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_skybox_visibility.h"
     CONTENT "${extracted}")
target_sources(xl_effect_reference PRIVATE tests/reference_skybox_visibility.cpp)

set(extracted "// Pinned skybox object traversal; engine storage uses adapters\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/render/rendermine.cpp"
                      "void RenderSkyBoxObjects (void)\n{")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_skybox_objects.h"
     CONTENT "${extracted}")
target_sources(xl_effect_reference PRIVATE tests/reference_skybox_objects.cpp)
