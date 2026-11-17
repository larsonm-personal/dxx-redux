file(SHA256 "${HOG}" original_hash)
if(NOT original_hash STREQUAL "4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303")
    message(FATAL_ERROR "Trigger flags require unchanged original Neptune")
endif()
file(MAKE_DIRECTORY "${OUTPUT}/missions" "${OUTPUT}/preview-user")
configure_file("${HOG}" "${OUTPUT}/missions/neptunxl.hog" COPYONLY)
execute_process(
    COMMAND "${ENGINE}" -trigger-flags-test flags -hogdir "${DATA}" -extra-dir "${OUTPUT}" -userdir
            "${OUTPUT}/preview-user" -nosound -nomusic
    RESULT_VARIABLE result
    OUTPUT_VARIABLE log
    ERROR_VARIABLE error
    TIMEOUT 80)
file(WRITE "${OUTPUT}/controls.log" "${log}${error}")
string(FIND "${log}" "XL trigger flag controls passed" witness)
if(NOT result EQUAL 0 OR witness LESS 0)
    message(FATAL_ERROR "Trigger flag controls failed (${result}): ${log}${error}")
endif()
if(EFFECT_REFERENCE)
    execute_process(
        COMMAND "${EFFECT_REFERENCE}" --camera-wall "${OUTPUT}/camera-wall.txt"
        RESULT_VARIABLE reference_result
        OUTPUT_VARIABLE reference_log
        ERROR_VARIABLE reference_error
        TIMEOUT 60)
    file(WRITE "${OUTPUT}/camera-wall-reference.log" "${reference_log}${reference_error}")
    if(NOT reference_result EQUAL 0)
        message(
            FATAL_ERROR "Wall camera source comparison failed: ${reference_log}${reference_error}")
    endif()
endif()
file(SHA256 "${HOG}" final_hash)
if(NOT final_hash STREQUAL original_hash)
    message(FATAL_ERROR "Original Neptune changed during trigger flag controls")
endif()
