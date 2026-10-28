foreach(
    pair IN
    ITEMS
        "objects/object.cpp|e71b0a906cd686549c783563ab93365d1bbf5cf05f08c1971ef08c7116c67a64"
        "include/object.h|ed546edd2bbda56a84e9fef8543706707d47a08b48e36616852346b5a2f96127"
        "include/segment.h|4f43198ba394971128c81373940b5a58283cbbbc2851ca6a6e4187e78382a667"
        "include/descent.h|f8c34f569c51541dc4d1e69ae3ac04972401b4c6cad99ac450bbc57a245f83e4"
        "main/gamedata.cpp|782acd85fae6fa5fbb0162a9f05474ae449063d70bf566d172c9d47ae4ed1a12"
        "objects/createobject.cpp|5d5a50c7c2fa3799f9e45c0e9e5a12afd3ebfbb165a1dbb0b792dd38adb8bbf3"
        "objects/updateobject.cpp|c4f3b9f1cd225f4dda99bb2a4e42af8546bc37ae36cc0bd19091dad6f48b01ac")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Object pool reference changed: ${path}")
    endif()
endforeach()
file(
    STRINGS "${XL_REFERENCE_GEOMETRY_ROOT}/include/object.h" constants
    REGEX
        "^#define[ \t]+(OBJ_(NONE|PLAYER|ROBOT|LIGHT|DEBRIS|FIREBALL|WEAPON|POWERUP|EXPLOSION)|OF_SHOULD_BE_DEAD|MAX_OBJECTS_D2X)[ \t]+"
)
file(STRINGS "${XL_REFERENCE_GEOMETRY_ROOT}/include/segment.h" capacity
     REGEX "^#define[ \t]+MAX_SEGMENTS_D2X[ \t]+")
list(APPEND constants ${capacity})
list(JOIN constants "\n" constants_text)
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_object_capacity_constants.h"
     CONTENT "${constants_text}\n")
file(STRINGS "${XL_REFERENCE_GEOMETRY_ROOT}/main/gamedata.cpp" capacity_assignment
     REGEX "^nMaxObjects = Max ")
list(LENGTH capacity_assignment assignment_count)
if(NOT assignment_count EQUAL 1)
    message(FATAL_ERROR "Cannot uniquely extract authored object capacity initialization")
endif()
list(GET capacity_assignment 0 capacity_line)
file(
    GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_object_capacity_init.h"
    CONTENT
        "static int reference_capacity(int count) { struct { struct { int count; } objects; } gameFileInfo{{count}}; int nMaxObjects; ${capacity_line} return nMaxObjects; }\n"
)
file(STRINGS "${XL_REFERENCE_GEOMETRY_ROOT}/objects/createobject.cpp" signature_assignment
     REGEX "^pObj->info.nSignature = gameData.objData.nNextSignature")
list(LENGTH signature_assignment signature_count)
if(NOT signature_count EQUAL 1)
    message(FATAL_ERROR "Cannot uniquely extract object lifetime assignment")
endif()
list(GET signature_assignment 0 signature_line)
file(
    GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_object_signature.h"
    CONTENT
        "static int reference_signature(int& next) { struct { struct { int nSignature; } info; } object{}; auto* pObj = &object; struct { struct { int nNextSignature; } objData; } gameData{{next}}; ${signature_line} next = gameData.objData.nNextSignature; return object.info.nSignature; }\n"
)
set(extracted "")
foreach(name FreeDebrisFilter FreeFireballFilter FreeFlareFilter FreeWeaponFilter FreeCandidates)
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/objects/object.cpp" "int32_t ${name} (")
endforeach()
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/objects/object.cpp"
                      "int32_t FreeObjectSlots (int32_t nRequested)\n{")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_object_capacity.h"
     CONTENT "${extracted}")
add_executable(xl_object_capacity_reference tests/reference_object_capacity.cpp)
target_include_directories(xl_object_capacity_reference PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(xl_object_capacity_reference PRIVATE cxx_std_17)
