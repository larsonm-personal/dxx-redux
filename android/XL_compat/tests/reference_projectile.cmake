foreach(
    pair IN
    ITEMS "physics/collide.cpp|d10f88f82e5d283077914881d1acb719f06785a86be22e50617d92d6fce47bc9"
          "include/object.h|ed546edd2bbda56a84e9fef8543706707d47a08b48e36616852346b5a2f96127"
          "objects/object.cpp|e71b0a906cd686549c783563ab93365d1bbf5cf05f08c1971ef08c7116c67a64"
          "include/fireweapon.h|894ca0c457a70156e159f7175ca7cf321677c6b04a02525c7c23ed181371ceb4")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Projectile reference changed: ${path}")
    endif()
endforeach()
file(STRINGS "${XL_REFERENCE_GEOMETRY_ROOT}/include/object.h" constants
     REGEX "^#define[ \t]+(MAX_HIT_OBJECTS|PF_PERSISTENT|OF_SHOULD_BE_DEAD|OBJ_WEAPON)[ \t]+")
list(JOIN constants "\n" constants_text)
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_projectile_constants.h"
     CONTENT "${constants_text}\n")
set(extracted "")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/physics/collide.cpp" "int32_t FindHitObject (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/physics/collide.cpp" "int32_t AddHitObject (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/physics/collide.cpp"
                      "void CObject::MaybeKillWeapon (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_projectile.h" CONTENT "${extracted}")
add_executable(xl_projectile_reference tests/reference_projectile.cpp)
target_include_directories(xl_projectile_reference PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(xl_projectile_reference PRIVATE cxx_std_17)
add_test(NAME xl_projectile_reference COMMAND xl_projectile_reference)
