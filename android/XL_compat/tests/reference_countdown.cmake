foreach(
    pair IN
    ITEMS "objects/reactor.cpp|9e011a86ff48f745fd2d6e0fb2beb0e539014a75894fbac93a7a114ea86b7b2c"
          "main/gametime.cpp|758b4a92b3800c0407c659f2829792e64cdabba7a818b27a64e6e267446d269b"
          "include/descent.h|f8c34f569c51541dc4d1e69ae3ac04972401b4c6cad99ac450bbc57a245f83e4"
          "include/game.h|842fd1c6fcb220a40b43aedb2aea8e2148b92a7736d3ff75c98c04b571e86d42")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Countdown source differs: ${path}")
    endif()
endforeach()
set(constants "// Actual pinned countdown constants\n")
foreach(header IN ITEMS sounds descent game)
    file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/${header}.h" source)
    foreach(
        token IN
        ITEMS SOUND_COUNTDOWN_0_SECS
              SOUND_COUNTDOWN_13_SECS
              SOUND_COUNTDOWN_29_SECS
              SOUND_CONTROL_CENTER_WARNING_SIREN
              SOUND_MINE_BLEW_UP
              DEFAULT_CONTROL_CENTER_EXPLOSION_TIME
              DIFFICULTY_LEVEL_COUNT)
        string(REGEX MATCH "#define[ \t]+${token}[ \t]+[^\r\n]+" definition "${source}")
        if(definition)
            string(APPEND constants "${definition}\n")
        endif()
    endforeach()
endforeach()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_countdown_constants.h"
     CONTENT "${constants}")
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/objects/reactor.cpp" source)
string(REGEX MATCH "int32_t nAlanPavlishReactorTimes[^;]+;" defaults "${source}")
if(NOT defaults)
    message(FATAL_ERROR "Countdown defaults missing")
endif()
set(extracted "// Actual pinned countdown functions\n${defaults}\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/objects/reactor.cpp" "void InitCountdown (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/objects/reactor.cpp"
                      "void DoCountdownFrame (void)\n{")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/gametime.cpp" "void GetSlowTicks (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_countdown.h" CONTENT "${extracted}")
target_sources(xl_trigger_flags_reference PRIVATE tests/reference_countdown.cpp)
