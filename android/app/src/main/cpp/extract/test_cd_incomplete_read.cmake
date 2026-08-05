set(test_dir "${CMAKE_CURRENT_BINARY_DIR}/cd_incomplete_read_test")
file(MAKE_DIRECTORY "${test_dir}")
file(WRITE "${test_dir}/short.bin" "x")
file(WRITE "${test_dir}/short.cue"
     "FILE \"short.bin\" BINARY\n  TRACK 01 AUDIO\n    INDEX 01 00:00:00\n")

foreach(tool IN ITEMS EXTRACT_CD FINGERPRINT_CD)
    execute_process(
        COMMAND "${${tool}}" "${test_dir}/short.cue"
        WORKING_DIRECTORY "${test_dir}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error)
    if(result EQUAL 0)
        message(FATAL_ERROR "${tool} accepted an incomplete track: ${output}${error}")
    endif()
    if(output MATCHES "\"sha1\"")
        message(FATAL_ERROR "${tool} published a partial SHA-1: ${output}")
    endif()
    if(NOT output MATCHES "\"type\": \"audio\"")
        message(FATAL_ERROR "${tool} did not preserve the declared track type: ${output}")
    endif()
endforeach()

file(REMOVE_RECURSE "${test_dir}")
