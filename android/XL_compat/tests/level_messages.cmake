file(SHA256 "${HOG}" source_hash)
if(NOT source_hash STREQUAL "4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303")
    message(FATAL_ERROR "Message controls require unchanged audited Neptune geometry")
endif()
file(MAKE_DIRECTORY "${OUTPUT}/missions" "${OUTPUT}/preview-user")
execute_process(COMMAND "${FIXTURE}" "${HOG}" "${OUTPUT}/missions" RESULT_VARIABLE result
                OUTPUT_VARIABLE log ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Cannot create message controls: ${log}${error}")
endif()
execute_process(
    COMMAND "${ENGINE}" -level-message-test production -hogdir "${DATA}" -extra-dir "${OUTPUT}"
            -userdir "${OUTPUT}/preview-user" -nosound -nomusic
    RESULT_VARIABLE result
    OUTPUT_VARIABLE log
    ERROR_VARIABLE error
    TIMEOUT 90)
file(WRITE "${OUTPUT}/runtime.log" "${log}${error}")
if(NOT result EQUAL 0 OR NOT log MATCHES "XL message runtime controls passed")
    message(FATAL_ERROR "Message runtime failed (${result}): ${log}${error}")
endif()
foreach(version IN ITEMS 23 27)
    if(NOT log MATCHES "${version}/40 messages: wall/master/object/autoplay dispatch")
        message(FATAL_ERROR "Message controls omitted version ${version}")
    endif()
endforeach()
file(SHA256 "${HOG}" final_hash)
if(NOT final_hash STREQUAL source_hash)
    message(FATAL_ERROR "Original geometry changed during message controls")
endif()
