foreach(
    pair IN
    ITEMS "main/segment.cpp|eece1c8ba624bd42e6e77b9f2a3dd9a54489338139ee102245672b2b7353e60d"
          "main/side.cpp|5f8552e6fb177b24a624c47c9c644029acf95eda9f7001624c7f28e7972b4989"
          "gameio/loadobjects.cpp|01fe3cbc1cf781d1180921c9e235593f1b79d68924c0d49a42502ef8c3ffcd3c"
          "include/cfile.h|34d3db395260649003a848015d3ac4dbac516779012f34c4315b530626016191")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Modern layout reference changed: ${path}")
    endif()
endforeach()
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/main/segment.cpp" segment_source)
string(REGEX MATCH "uint16_t sideVertIndex[^;]+;" side_table "${segment_source}")
if(NOT side_table)
    message(FATAL_ERROR "Missing pinned side vertex table")
endif()
set(extracted "// Actual pinned segment, side, extras and fog readers\n${side_table}\n")
foreach(
    method IN
    ITEMS GetCornerIndex
          ReadFunction
          ReadVerts
          ReadChildren
          ReadExtras
          Read
          RemapVertices)
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/segment.cpp"
                          "void CSegment::${method} (")
endforeach()
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/side.cpp" "int32_t CSide::Read (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/side.cpp" "void CSide::ReadWallNum (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/side.cpp" "void CSide::RemapVertices (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/gameio/loadobjects.cpp"
                      "void CFogData::Init (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/gameio/loadobjects.cpp"
                      "void CFogData::Read (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/gameio/loadobjects.cpp"
                      "static int32_t ReadGameFileInfo (")
# The source shifts signed negative UV values. Multiplication is equivalent for all serialized int16
# values and defined in C++17, including UBSan builds
string(REPLACE "fix (cf.ReadShort ()) << 5" "fix (cf.ReadShort ()) * 32" extracted "${extracted}")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_layouts.h" CONTENT "${extracted}")
add_executable(xl_layout_reference tests/reference_layouts.cpp)
target_link_libraries(xl_layout_reference PRIVATE xl_inventory)
target_include_directories(xl_layout_reference PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(xl_layout_reference PRIVATE cxx_std_17)
add_test(NAME xl_modern_layout_reference COMMAND xl_layout_reference)
set(XL_MODERN_LAYOUT_HOGS "" CACHE STRING "Original version-25/27 HOGs for full reader comparison")
if(XL_MODERN_LAYOUT_HOGS)
    add_test(NAME xl_original_modern_layouts COMMAND xl_layout_reference ${XL_MODERN_LAYOUT_HOGS})
    set_tests_properties(xl_original_modern_layouts PROPERTIES TIMEOUT 120)
endif()
set(XL_PUMO_HOG "" CACHE FILEPATH
                         "Original Pumo Mines HOG for complete layout and object-reader comparison")
if(XL_PUMO_HOG)
    add_test(
        NAME xl_pumo_layouts
        COMMAND
            ${CMAKE_COMMAND} -DHOG=${XL_PUMO_HOG} -DLAYOUT=$<TARGET_FILE:xl_layout_reference>
            -DOBJECT=$<TARGET_FILE:xl_effect_reference> -P
            ${CMAKE_CURRENT_LIST_DIR}/pumo_layouts.cmake)
    set_tests_properties(xl_pumo_layouts PROPERTIES TIMEOUT 120)
endif()
