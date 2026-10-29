foreach(
    pair IN
    ITEMS "main/trigger.cpp|74f1284260fb4a4bb7396bc45eeb83868a7f2dfa5e3fdf0bcb767b94f1b12852"
          "include/trigger.h|8f539027666ce768f467226381e82fd62c07a3cd3642fe97f857980091cf4dfd"
          "include/audio.h|7a0977cfefbc5de6c4805665d9b251264732179b99eb5ccd8b908b192bf3779f"
          "include/maths.h|43adf417dd1c282a39c8fed02bdad9d05a7f3ad5850a39a87897fd533eb857cd"
          "include/piggy.h|4ecf8941d1c81166051e74c38de877f45557d163ad0c4f92d9bcdeee21033a34"
          "main/textdata.cpp|d6172ffd9c3e94162358228ce168dd005d03feba231f45165b97f9b4ff1c0c5c"
          "audio/audio.cpp|2add6d6d234d2d382ae895c8e49397ce2071510c5d8a2010b50461c144e2cea0"
          "gameio/loadsounds.cpp|997970aafdfbea0b8cba5c248ce9f8cfd559f0d94f329d2b3b7e4ea2c39b28e2"
          "main/gamefolders.cpp|1368f6dd0b6d3ec70149dff82930c46382ddd3ef9d4d2cb6f84390691370e666")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Named-audio reference changed: ${path}")
    endif()
endforeach()

file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/trigger.h" trigger_header)
string(REGEX MATCHALL "#define[ \t]+(TF_[A-Z_]+|TT_COUNTDOWN|TT_MESSAGE|TT_SOUND)[ \t]+[0-9]+"
             constants "${trigger_header}")
string(REPLACE ";" "\n" constants "${constants}")
foreach(pair IN ITEMS "maths.h|I2X" "maths.h|X2I" "maths.h|F2X" "audio.h|SOUNDCLASS_GENERIC"
                      "piggy.h|MAX_ADDON_SOUND_FILES")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 name)
    file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/${path}" header)
    string(REGEX MATCH "#define[ \t]+${name}[^\n]+" definition "${header}")
    if(NOT definition)
        message(FATAL_ERROR "Missing source audio constant: ${name}")
    endif()
    string(APPEND constants "\n${definition}")
endforeach()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_audio_constants.h"
     CONTENT "${constants}\n")

file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/gameio/loadsounds.cpp" loadsounds)
string(FIND "${loadsounds}" "tAddonSound addonSounds [MAX_ADDON_SOUND_FILES] = {" start)
if(start LESS 0)
    message(FATAL_ERROR "Missing source addon-sound table")
endif()
string(SUBSTRING "${loadsounds}" ${start} -1 tail)
string(FIND "${tail}" "};" end)
math(EXPR length "${end} + 2")
string(SUBSTRING "${tail}" 0 ${length} addons)
set(extracted
    "// Actual pinned audio behavior; host storage and mixer calls are adapters\n${addons}\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/textdata.cpp"
                      "tTextIndex *FindTextData (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/gameio/loadsounds.cpp"
                      "int32_t FindAddonSound (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/gameio/loadsounds.cpp"
                      "Mix_Chunk *LoadAddonSound (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/gameio/loadsounds.cpp"
                      "void FreeAddonSounds (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/gamefolders.cpp" "char* LevelFolder (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/audio/audio.cpp"
                      "fix CAudioChannel::Duration (")
foreach(
    signature IN
    ITEMS "int32_t CTrigger::DoPlaySound ("
          "bool CTrigger::IsDelayed ("
          "void TriggersFrameProcess ("
          "static void StartTriggeredSounds (CArray<CTrigger>& triggers)"
          "static void StopTriggeredSounds (CArray<CTrigger>& triggers)")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/trigger.cpp" "${signature}")
endforeach()
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/audio/audio.cpp" audio)
string(REGEX MATCHALL "Mix_PlayChannel \\(m_info.nChannel, m_info.pMixChunk, ([^;]+)\\)" loop_calls
             "${audio}")
list(LENGTH loop_calls loop_count)
if(NOT loop_count EQUAL 1)
    message(FATAL_ERROR "Named audio SDL loop call is missing or ambiguous")
endif()
string(REGEX REPLACE "Mix_PlayChannel \\(m_info.nChannel, m_info.pMixChunk, (.*)\\)" "\\1"
                     loop_expression "${loop_calls}")
string(
    APPEND extracted
    "\nint MixerLoops(int bLooping, int nLoopStart, int nLoopEnd) { return ${loop_expression}; }\n")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_named_audio.h"
     CONTENT "${extracted}")
set(extracted "// Pinned classic sound selection; storage adapters remain independent\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/gamefolders.cpp" "char* LevelFolder (")
foreach(signature IN ITEMS "int32_t LoadHiresSound (" "int32_t SetupSounds (" "void LoadSounds ("
                           "bool VerifyHiresSound (" "int32_t LoadD2Sounds (")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/gameio/loadsounds.cpp" "${signature}")
endforeach()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_classic_audio.h"
     CONTENT "${extracted}")
add_executable(xl_named_audio_source tests/reference_named_audio.cpp
                                     tests/reference_classic_audio.cpp)
target_link_libraries(xl_named_audio_source PRIVATE xl_inventory)
target_include_directories(xl_named_audio_source PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME xl_named_audio_source COMMAND xl_named_audio_source)
