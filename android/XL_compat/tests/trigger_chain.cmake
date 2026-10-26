file(SHA256 "${HOG}" original_hash)
if(NOT original_hash STREQUAL "4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303")
    message(FATAL_ERROR "Chain controls require the original Neptune geometry")
endif()
file(MAKE_DIRECTORY "${OUTPUT}/missions" "${OUTPUT}/preview-user")
configure_file("${HOG}" "${OUTPUT}/missions/neptunxl.hog" COPYONLY)
execute_process(
    COMMAND
        "${ENGINE}" -trigger-chain-test dispatch -hogdir "${DATA}" -extra-dir "${OUTPUT}" -userdir
        "${OUTPUT}/preview-user" -nosound -nomusic -fixture-path "${OUTPUT}" -xl-test-navigation
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
    TIMEOUT 75)
file(WRITE "${OUTPUT}/controls.log" "${stdout}${stderr}")
string(FIND "${stdout}" "XL wall trigger chain controls passed" witness)
if(NOT result EQUAL 0 OR witness LESS 0)
    message(FATAL_ERROR "Trigger chain controls failed (${result}): ${stdout}${stderr}")
endif()
if(REFERENCE)
    execute_process(
        COMMAND "${REFERENCE}" "${OUTPUT}/trigger-counters.txt" RESULT_VARIABLE reference_result
        OUTPUT_VARIABLE reference_stdout ERROR_VARIABLE reference_stderr)
    file(WRITE "${OUTPUT}/reference.log" "${reference_stdout}${reference_stderr}")
    if(NOT reference_result EQUAL 0)
        message(FATAL_ERROR "Trigger reference mismatch: ${reference_stdout}${reference_stderr}")
    endif()
endif()
file(SHA256 "${HOG}" final_hash)
if(NOT final_hash STREQUAL original_hash)
    message(FATAL_ERROR "Original Neptune changed during chain controls")
endif()
