foreach(
    pair IN
    ITEMS "main/textdata.cpp|d6172ffd9c3e94162358228ce168dd005d03feba231f45165b97f9b4ff1c0c5c"
          "main/trigger.cpp|74f1284260fb4a4bb7396bc45eeb83868a7f2dfa5e3fdf0bcb767b94f1b12852"
          "include/descent.h|f8c34f569c51541dc4d1e69ae3ac04972401b4c6cad99ac450bbc57a245f83e4")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Level text reference changed: ${path}")
    endif()
endforeach()
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/descent.h" types)
string(FIND "${types}" "typedef struct tTextIndex {" start)
string(SUBSTRING "${types}" ${start} -1 tail)
string(FIND "${tail}" "\n};" end)
math(EXPR length "${end} + 3")
string(SUBSTRING "${tail}" 0 ${length} text_types)
set(extracted "// Actual pinned text records and reader bodies\n${text_types}\n")
foreach(signature IN ITEMS "void FreeTextData (" "void QSortTextData (" "void LoadTextData ("
                           "tTextIndex *FindTextData (" "int32_t ShowGameMessage (")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/textdata.cpp" "${signature}")
endforeach()
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/trigger.cpp"
                      "int32_t CTrigger::DoShowMessage (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/trigger.cpp" "bool CTrigger::IsDelayed (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_level_text.h" CONTENT "${extracted}")
add_executable(xl_level_text_reference tests/reference_level_text.cpp)
target_link_libraries(xl_level_text_reference PRIVATE xl_inventory)
target_include_directories(xl_level_text_reference PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME xl_level_text_reference COMMAND xl_level_text_reference)
