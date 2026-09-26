set(test_dir "${CMAKE_CURRENT_BINARY_DIR}/cd_sha1_test")
file(MAKE_DIRECTORY "${test_dir}")

# Separate data and audio sources exercise the tools' distinct sector read loops
string(REPEAT "Data0123456789ab" 4736 data)
string(REPEAT "Audio0123456789b" 44100 audio)
file(WRITE "${test_dir}/data.bin" "${data}")
file(WRITE "${test_dir}/audio.bin" "${audio}")
file(SHA1 "${test_dir}/data.bin" data_sha1)
file(SHA1 "${test_dir}/audio.bin" audio_sha1)
file(WRITE "${test_dir}/disc.cue"
     "FILE \"data.bin\" BINARY\n  TRACK 01 MODE1/2048\n    INDEX 01 00:00:00\n"
     "FILE \"audio.bin\" BINARY\n  TRACK 02 AUDIO\n    INDEX 01 00:00:00\n")

foreach(tool IN ITEMS EXTRACT_CD FINGERPRINT_CD)
    execute_process(
        COMMAND "${${tool}}" "${test_dir}/disc.cue"
        WORKING_DIRECTORY "${test_dir}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${tool} failed: ${output}${error}")
    endif()
    foreach(type IN ITEMS data audio)
        if(NOT output MATCHES "\"type\": \"${type}\", \"sha1\": \"${${type}_sha1}\"")
            message(FATAL_ERROR "${tool} ${type} SHA-1 differs from CMake: ${output}")
        endif()
    endforeach()
endforeach()

file(REMOVE_RECURSE "${test_dir}")
