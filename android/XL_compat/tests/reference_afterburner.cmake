foreach(pair IN
        ITEMS "main/game.cpp|a1a71d37ed4efc1341a62ab23d8c1375afc144fd7792bec35a143da83745b089"
              "include/player.h|4e134411347c2dee70f22c7b60c63717390496022964a77c1eb8dc5b5ff15cf4")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Afterburner source changed: ${path}")
    endif()
endforeach()
set(extracted "// Actual pinned afterburner controller and sound lookup\n")
# The enclosing owner pins audio.cpp. This is the profile already used by classic_sample.cpp, not
# Redux's input-bank setting or device resampling rate
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/audio/audio.cpp" backend)
string(
    FIND "${backend}"
         "gameOpts->sound.audioSampleRate = SAMPLE_RATE_44K) / fSlowDown), m_info.nFormat = AUDIO_S16SYS"
         profile)
if(profile LESS 0)
    message(FATAL_ERROR "Qualified XL signed-16 output profile changed")
endif()
foreach(pair IN
        ITEMS "main/game.cpp|AFTERBURNER_LOOP_[A-Z]+" "include/player.h|PLAYER_FLAGS_AFTERBURNER"
              "include/sounds.h|SOUND_AFTERBURNER_[A-Z]+" "include/audio.h|SAMPLE_RATE_22K")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 pattern)
    file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" source)
    string(REGEX MATCHALL "#define[ \t]+${pattern}[ \t]+[^\n]+" definitions "${source}")
    if(NOT definitions)
        message(FATAL_ERROR "Missing afterburner constant: ${pattern}")
    endif()
    string(REPLACE ";" "\n" definitions "${definitions}")
    string(APPEND extracted "${definitions}\n")
endforeach()
extract_audio_owner("int32_t CAudio::FindObjectSound (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/game.cpp" "void DoAfterburnerStuff (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_afterburner.h"
     CONTENT "${extracted}")
