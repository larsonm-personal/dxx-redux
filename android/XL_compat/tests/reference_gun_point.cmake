foreach(
    pair IN
    ITEMS "objects/robot.cpp|a33aee40ef05eb35d41538c75190df09bb744cd484146c6a3c6afb7a807818a3"
          "maths/fixc.cpp|da4a96862a231c07720bb31ff42eddd39174e1708e6ae1678ded61375c5b1c2c"
          "maths/tables.cpp|107434ea4cd7ccbe4d756d4f89006bc566d00d5c5fdac785ff6c31fea79c0926")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Gun point reference changed: ${path}")
    endif()
endforeach()
# vecmat.h and maths.h hashes are checked by the owning HXM reference
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/maths.h" math_source)
set(extracted "// Pinned gun transforms, trig, fixed multiplication and vector arithmetic\n")
foreach(name IN ITEMS PI I2X F2X XRound FixMul)
    string(REGEX MATCH "#[ \t]*define[ \t]+${name}[ \t(][^\n]+" macro "${math_source}")
    if(NOT macro)
        message(FATAL_ERROR "Missing gun math macro: ${name}")
    endif()
    string(APPEND extracted "${macro}\n")
endforeach()
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/maths/tables.cpp" table_source)
string(REGEX MATCH "int16_t sincos_table \\[\\] = \\{[^}]+\\};" table "${table_source}")
if(NOT table)
    message(FATAL_ERROR "Missing source sine table")
endif()
string(APPEND extracted "${table}\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/maths/fixc.cpp" "void FixSinCos (")
foreach(
    signature IN
    ITEMS "inline const fix CFixVector::Dot (const CFixVector&"
          "inline const CFixVector& CFixVector::operator+= (const CFixVector&"
          "inline const CFixVector CFixVector::operator+ (const CFixVector&"
          "inline const CFixMatrix CFixMatrix::Create (fix sinp"
          "inline const CFixMatrix CFixMatrix::Create (const CAngleVector&"
          "inline CFixMatrix& CFixMatrix::Transpose (CFixMatrix& m)"
          "inline CFixVector CFixMatrix::operator* (const CFixVector&")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/include/vecmat.h" "${signature}")
endforeach()
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/objects/robot.cpp" "int32_t CalcGunPoint (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_gun_point.h" CONTENT "${extracted}")
target_sources(xl_hxm_reference PRIVATE tests/reference_gun_point.cpp)
