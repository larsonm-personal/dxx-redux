file(SHA256 "${HOG}" original_hash)
if(NOT original_hash STREQUAL "4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303")
    message(FATAL_ERROR "Object event controls require unchanged original Neptune geometry")
endif()
file(MAKE_DIRECTORY "${OUTPUT}/missions" "${OUTPUT}/preview-user")
configure_file("${HOG}" "${OUTPUT}/missions/neptunxl.hog" COPYONLY)
execute_process(
    COMMAND "${ENGINE}" -object-trigger-test dispatch -hogdir "${DATA}" -extra-dir "${OUTPUT}"
            -userdir "${OUTPUT}/preview-user" -nosound -nomusic
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
    TIMEOUT 80)
file(WRITE "${OUTPUT}/controls.log" "${stdout}${stderr}")
string(FIND "${stdout}" "XL object event controls passed" witness)
if(NOT result EQUAL 0 OR witness LESS 0)
    message(FATAL_ERROR "Object event controls failed (${result}): ${stdout}${stderr}")
endif()
if(REFERENCE)
    execute_process(COMMAND "${REFERENCE}" --events "${OUTPUT}/object-event-samples.txt"
                    RESULT_VARIABLE result OUTPUT_VARIABLE log ERROR_VARIABLE error)
    file(WRITE "${OUTPUT}/reference-events.log" "${log}${error}")
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Object event reference mismatch: ${log}${error}")
    endif()
endif()
file(SHA256 "${HOG}" final_hash)
if(NOT final_hash STREQUAL original_hash)
    message(FATAL_ERROR "Original Neptune changed during object controls")
endif()
