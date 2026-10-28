# wall.cpp and segment.cpp are pinned by the preceding wall reference includes
foreach(
    pair IN
    ITEMS "render/renderlib.cpp|d3eacfaf28c9687e7e37c3e47333b11d4a026000839231e8a71b443bc86d4252"
          "include/palette.h|3394f7accb1e7e53679fe94ad54256134acdf7a5a374d03756cab7c3f1f6d7e9")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Cloak reference changed: ${path}")
    endif()
endforeach()
file(
    STRINGS "${XL_REFERENCE_GEOMETRY_ROOT}/include/wall.h" constants
    REGEX
        "^#define[ \t]+(WALL_OPEN|WALL_CLOSED|WALL_CLOAKED|WALL_DOOR_CLOSED|WALL_DOOR_CLOAKING|WALL_DOOR_DECLOAKING|WID_CLOAKED_FLAG|WID_TRANSPCOLOR_FLAG|MAX_CLOAKING_WALLS)[ \t]+"
)
file(STRINGS "${XL_REFERENCE_GEOMETRY_ROOT}/include/palette.h" fade
     REGEX "^#define[ \t]+FADE_LEVELS[ \t]+")
list(APPEND constants ${fade})
list(JOIN constants "\n" constants_text)
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_cloak_constants.h"
     CONTENT "${constants_text}\n")
set(extracted "// Actual pinned XL cloak allocation, callers, frame functions and WallAlpha\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/wall.cpp"
                      "static CCloakingWall*\tAddCloakingWall (")
foreach(
    signature IN
    ITEMS "static void DeleteCloakingWall (" "CCloakingWall* FindCloakingWall ("
          "CCloakingWall* CWall::StartCloak (" "CCloakingWall* CWall::StartDecloak ("
          "bool DoCloakingWallFrame (" "void DoDecloakingWallFrame (")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/wall.cpp" "${signature}")
endforeach()
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/segment.cpp"
                      "void CSegment::StartCloak (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/segment.cpp"
                      "void CSegment::StartDecloak (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/render/renderlib.cpp" "float WallAlpha (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_cloak.h" CONTENT "${extracted}")
add_executable(xl_cloak_reference tests/reference_cloak.cpp)
target_include_directories(xl_cloak_reference PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(xl_cloak_reference PRIVATE cxx_std_17)
