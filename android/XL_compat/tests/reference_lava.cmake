foreach(
    pair IN
    ITEMS
        "include/wall.h|08c59bf8c299ea05967f806b92b3d03b4226914ef747ae1ef5afa4154c70c2bd"
        "include/loadgamedata.h|6fd28b99680f6ccb1fa1a1e4ecfd17a4306ecb93825462e9a84af12896e1ed02"
        "include/maths.h|43adf417dd1c282a39c8fed02bdad9d05a7f3ad5850a39a87897fd533eb857cd"
        "render/renderlib.cpp|d3eacfaf28c9687e7e37c3e47333b11d4a026000839231e8a71b443bc86d4252"
        "physics/collide.cpp|d10f88f82e5d283077914881d1acb719f06785a86be22e50617d92d6fce47bc9"
        "objects/updateobject.cpp|c4f3b9f1cd225f4dda99bb2a4e42af8546bc37ae36cc0bd19091dad6f48b01ac"
        "main/segment.cpp|eece1c8ba624bd42e6e77b9f2a3dd9a54489338139ee102245672b2b7353e60d"
        "main/side.cpp|5f8552e6fb177b24a624c47c9c644029acf95eda9f7001624c7f28e7972b4989")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Lava reference changed: ${path}")
    endif()
endforeach()
# Shared motion/math sources are pinned by the preceding oracle definitions
set(extracted "// Extracted XL fluid consumers; no production fluid code included\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/side.cpp" "int32_t CSide::Physics (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/segment.cpp"
                      "int32_t CSegment::Physics (fix&")
foreach(
    signature IN
    ITEMS "void CObject::ApplyDamageToPlayer (" "int32_t CObject::ApplyWallPhysics ("
          "int32_t CObject::CheckSegmentPhysics (" "void CObject::ScrapeOnWall ("
          "void CObject::Bump (CFixVector vForce")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/physics/collide.cpp" "${signature}")
endforeach()
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/objects/updateobject.cpp"
                      "int32_t CObject::CheckWallPhysics (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/maths/vecmat.cpp"
                      "const CFixVector CFixVector::Random (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/physics/physics.cpp"
                      "void CObject::ApplyForce (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_lava.h" CONTENT "${extracted}")
add_executable(xl_lava_reference tests/reference_lava.cpp)
target_include_directories(xl_lava_reference PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(xl_lava_reference PRIVATE cxx_std_17)
