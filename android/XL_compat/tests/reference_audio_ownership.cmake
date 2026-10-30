foreach(
    pair IN
    ITEMS "audio/objectaudio.cpp|e44484af297ce0c20c92feb00bfb2795316980434e3f1e1d85194c1d46b992e3"
          "include/cstack.h|7ee46bb90556774f7d983b5737815813e5a6e96e4195be71f9f58dfc6705f8ee")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Audio ownership source differs: ${path}")
    endif()
endforeach()
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/audio.h" header)
string(REGEX MATCHALL "#define[ \t]+(MAX_SOUND_OBJECTS|SOF_[A-Z_]+)[ \t]+[0-9]+" constants
             "${header}")
string(REPLACE ";" "\n" constants "${constants}")
set(extracted "// Actual source object and channel ownership consumers\n${constants}\n")
function(extract_audio_owner signature)
    file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/audio/objectaudio.cpp" source)
    string(FIND "${source}" "${signature}" start)
    if(start LESS 0)
        message(FATAL_ERROR "Missing source sound-owner function: ${signature}")
    endif()
    string(SUBSTRING "${source}" ${start} -1 tail)
    # DestroyObjectSound has a column-zero critical block inside its function
    string(FIND "${tail}"
                "\n//------------------------------------------------------------------------------"
                finish)
    if(finish LESS 0)
        message(FATAL_ERROR "Missing sound-owner function separator")
    endif()
    string(SUBSTRING "${tail}" 0 ${finish} fragment)
    set(extracted "${extracted}\n${fragment}" PARENT_SCOPE)
endfunction()
foreach(
    signature IN
    ITEMS "void CSoundObject::Stop ("
          "bool CSoundObject::Start ("
          "bool CAudio::SuspendObjectSound ("
          "void CAudio::DeleteSoundObject ("
          "int32_t CAudio::DestroyObjectSound ("
          "int32_t CAudio::DestroySegmentSound ("
          "void CAudio::EndSoundObject ("
          "void CAudio::StopObjectSounds ("
          "void CAudio::Cleanup (")
    extract_audio_owner("${signature}")
endforeach()
foreach(signature IN ITEMS "void CAudioChannel::SetPlaying (" "void CAudio::UnregisterChannel (")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/audio/audio.cpp" "${signature}")
endforeach()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_audio_ownership.h"
     CONTENT "${extracted}")
# Extract inline methods up to the next method, preserving the actual container operation
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/cstack.h" stack)
set(extracted "// Actual pinned stack methods; backing storage is independent\n")
foreach(
    pair IN
    ITEMS "inline bool Grow (|inline bool Push ("
          "inline bool Delete (|inline bool DeleteElement ("
          "inline void Shrink (|inline _T& Pop (")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 begin)
    list(GET parts 1 end)
    string(FIND "${stack}" "${begin}" start)
    string(FIND "${stack}" "${end}" finish)
    if(start LESS 0 OR finish LESS start)
        message(FATAL_ERROR "Missing source stack method boundary")
    endif()
    math(EXPR length "${finish} - ${start}")
    string(SUBSTRING "${stack}" ${start} ${length} fragment)
    string(APPEND extracted "${fragment}\n")
endforeach()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_audio_stack.h"
     CONTENT "${extracted}")
string(FIND "${header}" "inline int32_t ActiveObjects (" start)
string(FIND "${header}" "inline int32_t MaxChannels (" finish)
if(start LESS 0 OR finish LESS start)
    message(FATAL_ERROR "Missing actual active sound-object count")
endif()
math(EXPR length "${finish} - ${start}")
string(SUBSTRING "${header}" ${start} ${length} active)
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_audio_active.h"
     CONTENT "${active}\n")
target_sources(xl_named_audio_source PRIVATE tests/reference_audio_ownership.cpp)
