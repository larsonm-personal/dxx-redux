foreach(
    pair IN
    ITEMS "main/findpath.cpp|3d8a80b1909c144e259106597a1c991547b99e9f3582fb83e2b36eab050cf61a"
          "main/dialheap.cpp|2a2ee40a0e964ed073b4153ed21aefec62d2dd9fe498545ccd186f8543de64d4"
          "include/dialheap.h|71e5e38b1ec97334139dbfa482257e6378be1024afa4579e46d3ab819c2d3487")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Audio route source changed: ${path}")
    endif()
endforeach()
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/dialheap.h" header)
string(FIND "${header}" "class CDialHeap {" start)
string(FIND "${header}" "\n};" finish)
if(start LESS 0 OR finish LESS start)
    message(FATAL_ERROR "Missing source Dial heap declaration")
endif()
math(EXPR length "${finish} - ${start} + 3")
string(SUBSTRING "${header}" ${start} ${length} declaration)
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_audio_heap_class.h"
     CONTENT "${declaration}\n")
set(extracted "// Actual pinned heap and all-destination path consumers\n")
foreach(
    signature IN
    ITEMS "bool CDialHeap::Create ("
          "void CDialHeap::Destroy ("
          "void CDialHeap::Reset ("
          "void CDialHeap::Setup ("
          "bool CDialHeap::Push ("
          "int32_t CDialHeap::Scan ("
          "int16_t CDialHeap::Pop ("
          "int16_t CDialHeap::RouteLength ("
          "int16_t CDialHeap::BuildRoute (")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/dialheap.cpp" "${signature}")
endforeach()
foreach(signature IN ITEMS "bool CDACSUniDirRouter::Create (" "fix CDACSUniDirRouter::BuildPath ("
                           "fix CDACSUniDirRouter::FindPath (" "fix CDACSUniDirRouter::Distance (")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/findpath.cpp" "${signature}")
endforeach()
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/audio/objectaudio.cpp"
                      "int32_t CAudio::Distance (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_audio_routes.h"
     CONTENT "${extracted}\n")
target_sources(xl_named_audio_source PRIVATE tests/reference_audio_routes.cpp)
