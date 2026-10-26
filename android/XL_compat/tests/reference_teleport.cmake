foreach(
    pair IN
    ITEMS "maths/fixc.cpp|da4a96862a231c07720bb31ff42eddd39174e1708e6ae1678ded61375c5b1c2c"
          "maths/tables.cpp|107434ea4cd7ccbe4d756d4f89006bc566d00d5c5fdac785ff6c31fea79c0926"
          "main/trigger.cpp|74f1284260fb4a4bb7396bc45eeb83868a7f2dfa5e3fdf0bcb767b94f1b12852")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Teleport reference changed: ${path}")
    endif()
endforeach()
set(extracted "// Pinned reference functions; no production math included\n")
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/maths/tables.cpp" tables)
foreach(table IN ITEMS sincos_table asin_table acos_table)
    string(REGEX MATCH "[a-z0-9_]+ ${table} \\[\\] = \\{[^}]*\\};" fragment "${tables}")
    if(NOT fragment)
        message(FATAL_ERROR "Missing reference table: ${table}")
    endif()
    string(APPEND extracted "${fragment}\n")
endforeach()
foreach(signature IN ITEMS "void FixSinCos (" "fixang FixASin (" "fixang FixACos("
                           "fixang FixAtan2 (")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/maths/fixc.cpp" "${signature}")
endforeach()
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/maths/vecmat.cpp"
                      "fix CFixVector::Mag (void) const")
foreach(
    signature IN
    ITEMS
        "inline const fix CFixVector::Normalize (CFixVector& v)"
        "inline const fix CFixVector::Dot (const CFixVector& v0, const CFixVector& v1)"
        "inline const CAngleVector CFixVector::ToAnglesVecNorm (void) const"
        "inline const CAngleVector CFixVector::ToAnglesVec (void) const"
        "inline const CFixMatrix CFixMatrix::Create (fix sinp, fix cosp, fix sinb, fix cosb, fix sinh, fix cosh)"
        "inline const CFixMatrix CFixMatrix::Create (const CAngleVector& a)"
        "inline CFixVector CFixMatrix::operator* (const CFixVector& v)")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/include/vecmat.h" "${signature}")
endforeach()
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/trigger.cpp" "void TriggerSetOrient (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/trigger.cpp" "void TriggerSetObjOrient (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_teleport.h" CONTENT "${extracted}")
add_executable(xl_teleport_reference tests/reference_teleport.cpp)
target_include_directories(xl_teleport_reference PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(xl_teleport_reference PRIVATE cxx_std_17)
