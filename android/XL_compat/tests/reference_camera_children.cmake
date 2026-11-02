file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/weapons/fireweapon.cpp" children_hash)
if(NOT children_hash STREQUAL "37ae81f686e063ca7102bb9154368e67626e843ae898d82e8583167d31dbaac8")
    message(FATAL_ERROR "Camera smart children source changed")
endif()
set(extracted
    "// Actual hash-pinned smart target, direction and creation control flow\n${camera_math}\n")
foreach(signature IN ITEMS "static inline int32_t Rand (" "static inline int32_t RandShort ("
                           "static inline int32_t SRandShort (")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/include/maths.h" "${signature}")
endforeach()
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/maths/vecmat.cpp"
                      "fix CFixVector::Mag (void) const")
foreach(
    signature IN
    ITEMS
        "inline const fix CFixVector::Dist (const CFixVector& vec0, const CFixVector& vec1)"
        "inline const fix CFixVector::Normalize (CFixVector& v)"
        "inline const fix CFixVector::NormalizedDir (CFixVector& dest, const CFixVector& end, const CFixVector& start)"
        "inline const CFixVector CFixVector::operator* (const fix s) const")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/include/vecmat.h" "${signature}")
endforeach()
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/maths/vecmat.cpp"
                      "const CFixVector CFixVector::Random (")
string(REGEX MATCH "#define OBJ_LIST_TYPE[^\n]+" list_type "${object_header}")
string(REGEX MATCH "#define FORALL_OBJS[^\n]+" all_objects "${object_header}")
if(NOT list_type OR NOT all_objects)
    message(FATAL_ERROR "Missing source linked object iteration profile")
endif()
string(APPEND extracted "\n${list_type}\n${all_objects}\n")
foreach(
    signature IN
    ITEMS "void CObject::Link (tObjListRef&"
          "CObjectIterator::CObjectIterator (CObject*&"
          "CObject* CObjectIterator::Start ("
          "CObject* CObjectIterator::Head ("
          "bool CObjectIterator::Done ("
          "CObject* CObjectIterator::Next ("
          "CObject* CObjectIterator::Step (")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/objects/object.cpp" "${signature}")
endforeach()
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/objects/object.cpp" iteration_source)
string(REGEX MATCH "int32_t CObjectIterator::Size [(]void[)] [{][^}]+[}]" iterator_size
             "${iteration_source}")
if(NOT iterator_size)
    message(FATAL_ERROR "Missing source inline iterator size")
endif()
string(APPEND extracted "\n${iterator_size}\n")
foreach(signature IN ITEMS "int32_t CreateHomingWeapon (" "void CreateSmartChildren (")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/weapons/fireweapon.cpp" "${signature}")
endforeach()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_camera_children.h"
     CONTENT "${extracted}")
target_sources(xl_effect_reference PRIVATE tests/reference_camera_children.cpp)
