foreach(
    pair IN
    ITEMS "main/trigger.cpp|74f1284260fb4a4bb7396bc45eeb83868a7f2dfa5e3fdf0bcb767b94f1b12852"
          "main/segment.cpp|eece1c8ba624bd42e6e77b9f2a3dd9a54489338139ee102245672b2b7353e60d"
          "main/side.cpp|5f8552e6fb177b24a624c47c9c644029acf95eda9f7001624c7f28e7972b4989"
          "include/trigger.h|8f539027666ce768f467226381e82fd62c07a3cd3642fe97f857980091cf4dfd"
          "include/sounds.h|9bada81164bb79a1207ddf94045544760cb453a336bd24e90b8d18f8c4d9384d"
          "include/wall.h|08c59bf8c299ea05967f806b92b3d03b4226914ef747ae1ef5afa4154c70c2bd"
          "include/loadgamedata.h|6fd28b99680f6ccb1fa1a1e4ecfd17a4306ecb93825462e9a84af12896e1ed02")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Trigger flag reference differs: ${path}")
    endif()
endforeach()
set(constants "// Constants from pinned source headers\n")
foreach(header IN ITEMS trigger sounds wall loadgamedata)
    file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/${header}.h" source_header)
    foreach(
        token IN
        ITEMS TF_NO_MESSAGE
              TF_ONE_SHOT
              TF_DISABLED
              TF_PERMANENT
              TF_ALTERNATE
              TF_SET_ORIENT
              TF_SILENT
              TF_AUTOPLAY
              TT_OPEN_WALL
              TT_CLOSE_WALL
              TT_ILLUSORY_WALL
              SOUND_FORCEFIELD_OFF
              SOUND_FORCEFIELD_HUM
              SOUND_WALL_REMOVED
              WALL_OPEN
              WALL_CLOSED
              WALL_ILLUSION
              WALL_ILLUSION_OFF
              TMI_FORCE_FIELD)
        string(REGEX MATCH "#define[ \t]+${token}[ \t]+[^\r\n]+" definition "${source_header}")
        if(definition)
            string(APPEND constants "${definition}\n")
        endif()
    endforeach()
endforeach()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_trigger_flag_constants.h"
     CONTENT "${constants}")
set(extracted "// Actual pinned flag consumers and illusion primitives\n")
foreach(signature IN ITEMS "int32_t CTrigger::DoChangeWalls (" "void CTrigger::DoIllusionOff ("
                           "void CTrigger::DoIllusionOn (")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/trigger.cpp" "${signature}")
endforeach()
foreach(signature IN ITEMS "void CSegment::IllusionOff (" "void CSegment::IllusionOn (")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/segment.cpp" "${signature}")
endforeach()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_trigger_flags.h"
     CONTENT "${extracted}")
add_executable(xl_trigger_flags_reference tests/reference_trigger_flags.cpp)
set(extracted "// Pinned texture handler and side assignment; surrounding storage is an adapter\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/trigger.cpp"
                      "void CTrigger::DoChangeTexture (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/side.cpp" "void CSide::SetTextures (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/trigger.cpp"
                      "void CTrigger::PrintMessage (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_texture_actions.h"
     CONTENT "${extracted}")
target_sources(xl_trigger_flags_reference PRIVATE tests/reference_texture_actions.cpp)
include(tests/reference_countdown.cmake)
include(tests/reference_disarm.cmake)
target_include_directories(xl_trigger_flags_reference PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(xl_trigger_flags_reference PRIVATE cxx_std_17)
add_test(NAME xl_trigger_flag_reference COMMAND xl_trigger_flags_reference)
