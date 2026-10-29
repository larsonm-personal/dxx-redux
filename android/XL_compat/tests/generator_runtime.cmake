file(SHA256 "${HOG}" original_hash)
if(NOT original_hash STREQUAL "4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303")
    message(FATAL_ERROR "Generator controls require unchanged original Neptune geometry")
endif()
file(MAKE_DIRECTORY "${OUTPUT}/missions" "${OUTPUT}/preview-user")
configure_file("${HOG}" "${OUTPUT}/missions/neptunxl.hog" COPYONLY)
if(NOT LAYOUT_VERSION)
    set(LAYOUT_VERSION 23)
endif()
execute_process(
    COMMAND
        "${ENGINE}" -generator-assignment-test mapping -layout-version "${LAYOUT_VERSION}" -hogdir
        "${DATA}" -extra-dir "${OUTPUT}" -userdir "${OUTPUT}/preview-user" -nosound -nomusic
    RESULT_VARIABLE result
    OUTPUT_VARIABLE log
    ERROR_VARIABLE error
    TIMEOUT 80)
file(WRITE "${OUTPUT}/controls.log" "${log}${error}")
string(FIND "${log}" "XL generator assignment runtime passed" witness)
if(NOT result EQUAL 0 OR witness LESS 0)
    message(FATAL_ERROR "Generator runtime failed (${result}): ${log}${error}")
endif()
if(REFERENCE)
    execute_process(COMMAND "${REFERENCE}" --runtime "${OUTPUT}" RESULT_VARIABLE result
                    OUTPUT_VARIABLE reference_log ERROR_VARIABLE error)
    file(WRITE "${OUTPUT}/reference-runtime.log" "${reference_log}${error}")
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Generator runtime reference mismatch: ${reference_log}${error}")
    endif()
endif()
file(SHA256 "${HOG}" final_hash)
if(NOT final_hash STREQUAL original_hash)
    message(FATAL_ERROR "Original Neptune changed during generator controls")
endif()
