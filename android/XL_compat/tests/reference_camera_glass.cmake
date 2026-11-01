foreach(
    pair IN
    ITEMS "main/wall.cpp|05eaa2cab8dd7f5661afd210e0edc8dea4fcd2bb2075c00d3012bec395ab0c42"
          "main/segment.cpp|eece1c8ba624bd42e6e77b9f2a3dd9a54489338139ee102245672b2b7353e60d"
          "main/findpath.cpp|3d8a80b1909c144e259106597a1c991547b99e9f3582fb83e2b36eab050cf61a"
          "include/findpath.h|b06461a59cda62699ac545b008d21a1270cdcc92af0fc1d973e4ba746747b3fd")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Camera glass route source changed: ${path}")
    endif()
endforeach()
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/findpath.h" header)
string(FIND "${header}" "class CPathNode {" start)
string(FIND "${header}" "class CDACSRouter :" finish)
math(EXPR length "${finish} - ${start}")
if(start LESS 0 OR length LESS 1)
    message(FATAL_ERROR "Missing source simple route declarations")
endif()
string(SUBSTRING "${header}" ${start} ${length} declarations)
set(extracted "// Actual pinned uncached bidirectional glass router\n${declarations}\n")
foreach(
    signature IN
    ITEMS "int32_t CScanInfo::Setup ("
          "void CSimpleHeap::Setup ("
          "int16_t CSimpleHeap::Expand ("
          "bool CSimpleUniDirHeap::Match ("
          "bool CSimpleBiDirHeap::Match ("
          "int32_t CRouter::SetSegment ("
          "fix CRouter::PathLength ("
          "fix CSimpleBiDirRouter::BuildPath ("
          "fix CSimpleBiDirRouter::FindPath (")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/findpath.cpp" "${signature}")
endforeach()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_camera_glass.h"
     CONTENT "${extracted}")
target_sources(xl_effect_reference PRIVATE tests/reference_camera_glass.cpp)
