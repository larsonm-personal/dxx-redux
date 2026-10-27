file(SHA256 "${HOG}" original_hash)
if(NOT original_hash STREQUAL "4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303")
    message(FATAL_ERROR "Equipment controls require unchanged original Neptune geometry")
endif()
file(MAKE_DIRECTORY "${OUTPUT}/missions" "${OUTPUT}/preview-user")
configure_file("${HOG}" "${OUTPUT}/missions/neptunxl.hog" COPYONLY)
execute_process(
    COMMAND "${ENGINE}" -equipment-test production -hogdir "${DATA}" -extra-dir "${OUTPUT}" -userdir
            "${OUTPUT}/preview-user" -nosound -nomusic
    RESULT_VARIABLE result
    OUTPUT_VARIABLE log
    ERROR_VARIABLE error
    TIMEOUT 80)
file(WRITE "${OUTPUT}/controls.log" "${log}${error}")
string(FIND "${log}" "XL equipment controls passed" witness)
if(NOT result EQUAL 0 OR witness LESS 0)
    message(FATAL_ERROR "Equipment controls failed (${result}): ${log}${error}")
endif()
if(REFERENCE)
    execute_process(
        COMMAND "${REFERENCE}" "${OUTPUT}/equipment-samples.txt"
                "${OUTPUT}/equipment-initialization.txt" RESULT_VARIABLE result OUTPUT_VARIABLE log
        ERROR_VARIABLE error)
    file(WRITE "${OUTPUT}/reference.log" "${log}${error}")
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Equipment reference mismatch: ${log}${error}")
    endif()
endif()
file(SHA256 "${HOG}" final_hash)
if(NOT final_hash STREQUAL original_hash)
    message(FATAL_ERROR "Original Neptune changed during equipment controls")
endif()
