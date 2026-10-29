file(SHA256 "${HOG}" source_hash)
if(NOT source_hash STREQUAL "4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303")
    message(FATAL_ERROR "Named audio controls require unchanged audited Neptune geometry")
endif()
file(MAKE_DIRECTORY "${OUTPUT}/missions" "${OUTPUT}/preview-user")
execute_process(COMMAND "${FIXTURE}" "${HOG}" "${OUTPUT}/missions" RESULT_VARIABLE result
                OUTPUT_VARIABLE log ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Cannot create named audio controls: ${log}${error}")
endif()
file(WRITE "${OUTPUT}/runtime.log" "")
# Each format starts with a fresh window/pause lifecycle in the headless host
foreach(ordinal IN ITEMS 1 2)
    if(ordinal EQUAL 1)
        set(version 23)
    else()
        set(version 27)
    endif()
    execute_process(
        COMMAND "${ENGINE}" -named-audio-test production -audio-level "${ordinal}" -hogdir "${DATA}"
                -extra-dir "${OUTPUT}" -userdir "${OUTPUT}/preview-user" -nomusic
        RESULT_VARIABLE result
        OUTPUT_VARIABLE log
        ERROR_VARIABLE error
        TIMEOUT 45)
    file(APPEND "${OUTPUT}/runtime.log" "${log}${error}")
    if(NOT result EQUAL 0 OR NOT log MATCHES "XL named audio runtime controls passed"
       OR NOT log MATCHES "${version}/40 sounds: wall/master/object/autoplay")
        message(FATAL_ERROR "Named audio version ${version} failed (${result}): ${log}${error}")
    endif()
endforeach()
file(SHA256 "${HOG}" final_hash)
if(NOT final_hash STREQUAL source_hash)
    message(FATAL_ERROR "Original geometry changed during named audio controls")
endif()
