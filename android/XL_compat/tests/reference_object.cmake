foreach(
    pair IN
    ITEMS "main/trigger.cpp|74f1284260fb4a4bb7396bc45eeb83868a7f2dfa5e3fdf0bcb767b94f1b12852"
          "gameio/loadobjects.cpp|01fe3cbc1cf781d1180921c9e235593f1b79d68924c0d49a42502ef8c3ffcd3c"
          "objects/object.cpp|e71b0a906cd686549c783563ab93365d1bbf5cf05f08c1971ef08c7116c67a64")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Object trigger reference changed: ${path}")
    endif()
endforeach()
set(extracted "// Hash-pinned object Read and owner-reference loading\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/trigger.cpp" "void CTrigger::Read (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/trigger.cpp"
                      "inline int32_t CTrigger::DoExecObjTrigger (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/objects/object.cpp" "float CObject::Damage (")
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/gameio/loadobjects.cpp" loader)
string(FIND "${loader}" "gameData.trigData.m_nTriggers [1] = cf.ReadInt ();" start)
if(start LESS 0)
    message(FATAL_ERROR "Missing object section start")
endif()
string(SUBSTRING "${loader}" ${start} -1 tail)
string(FIND "${tail}" "\n\t\tBuildObjTriggerRef ();" end)
if(end LESS 0)
    message(FATAL_ERROR "Missing object section end")
endif()
string(SUBSTRING "${tail}" 0 ${end} section)
file(
    GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_object_read.h"
    CONTENT
        "${extracted}\nint ReadObjectSection(CFile& cf) {\nint32_t i;\n${section}\nreturn 0;\n}\n")
add_executable(xl_object_decode_reference tests/reference_object_decode.cpp)
target_link_libraries(xl_object_decode_reference PRIVATE xl_inventory)
target_include_directories(xl_object_decode_reference PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(xl_object_decode_reference PRIVATE cxx_std_17)
add_test(
    NAME xl_object_trigger_decode_reference
    COMMAND
        ${CMAKE_COMMAND} -DFIXTURE=$<TARGET_FILE:xl_inventory_test>
        -DREFERENCE=$<TARGET_FILE:xl_object_decode_reference>
        -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/object-decode -P
        ${CMAKE_CURRENT_LIST_DIR}/object_decode.cmake)
set(XL_GLASSHOUSE_HOG "" CACHE FILEPATH "Original GlassHouse HOG for object decode comparison")
if(XL_GLASSHOUSE_HOG)
    add_test(
        NAME xl_glasshouse_object_decode
        COMMAND
            ${CMAKE_COMMAND} -DHOG=${XL_GLASSHOUSE_HOG}
            -DREFERENCE=$<TARGET_FILE:xl_object_decode_reference> -P
            ${CMAKE_CURRENT_LIST_DIR}/glasshouse_object_decode.cmake)
endif()
