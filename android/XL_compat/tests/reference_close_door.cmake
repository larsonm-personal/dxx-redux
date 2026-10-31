file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/main/segment.cpp" segment_hash)
if(NOT segment_hash STREQUAL "eece1c8ba624bd42e6e77b9f2a3dd9a54489338139ee102245672b2b7353e60d")
    message(FATAL_ERROR "Close-door reference segment source changed")
endif()
# wall.cpp is pinned by reference_keyed_wall.cmake before this include
foreach(
    pair IN
    ITEMS "include/wall.h|08c59bf8c299ea05967f806b92b3d03b4226914ef747ae1ef5afa4154c70c2bd"
          "include/object.h|ed546edd2bbda56a84e9fef8543706707d47a08b48e36616852346b5a2f96127"
          "main/gamedata.cpp|782acd85fae6fa5fbb0162a9f05474ae449063d70bf566d172c9d47ae4ed1a12")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Door capacity source changed: ${path}")
    endif()
endforeach()
file(STRINGS "${XL_REFERENCE_GEOMETRY_ROOT}/include/wall.h" capacity
     REGEX "^#define[ \t]+MAX_DOORS[ \t]+")
list(JOIN capacity "\n" capacity_text)
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_door_capacity.h"
     CONTENT "${capacity_text}\n")
file(STRINGS "${XL_REFERENCE_GEOMETRY_ROOT}/include/object.h" object_types
     REGEX "^#define[ \t]+OBJ_(WEAPON|FIREBALL|EXPLOSION|EFFECT|MARKER)[ \t]+")
list(JOIN object_types "\n" object_types_text)
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_door_object_types.h"
     CONTENT "${object_types_text}\n")
file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/include/cstack.h" stack_hash)
if(NOT stack_hash STREQUAL "7ee46bb90556774f7d983b5737815813e5a6e96e4195be71f9f58dfc6705f8ee")
    message(FATAL_ERROR "Active-door stack reference changed")
endif()
set(extracted "")
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/cstack.h" stack_source)
foreach(name Grow Delete)
    string(FIND "${stack_source}" "inline bool ${name} (" start)
    if(start LESS 0)
        message(FATAL_ERROR "Missing stack function ${name}")
    endif()
    string(SUBSTRING "${stack_source}" ${start} -1 tail)
    string(FIND "${tail}" "\n\t\t\t}\n" end)
    if(end LESS 0)
        message(FATAL_ERROR "Missing stack function end ${name}")
    endif()
    math(EXPR length "${end} + 6")
    string(SUBSTRING "${tail}" 0 ${length} fragment)
    string(APPEND extracted "${fragment}\n")
endforeach()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_door_stack.h" CONTENT "${extracted}")
set(extracted "// Actual pinned XL allocation, lookup, close transition and caller\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/segment.cpp"
                      "int32_t CSegment::DoorIsBlocked (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/wall.cpp"
                      "static CActiveDoor* AddActiveDoor (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/wall.cpp" "CActiveDoor* FindActiveDoor (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/wall.cpp"
                      "CActiveDoor* CWall::CloseDoor (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/segment.cpp" "void CSegment::CloseDoor (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/wall.cpp"
                      "int32_t CWall::AnimateOpeningDoor (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/wall.cpp"
                      "int32_t CWall::AnimateClosingDoor (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_close_door.h" CONTENT "${extracted}")
add_executable(xl_close_door_reference tests/reference_close_door.cpp)
target_include_directories(xl_close_door_reference PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(xl_close_door_reference PRIVATE cxx_std_17)
