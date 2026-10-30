file(SHA256 "${HOG}" original_hash)
if(NOT original_hash STREQUAL "4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303")
    message(FATAL_ERROR "Countdowns require unchanged original Neptune geometry")
endif()
file(MAKE_DIRECTORY "${OUTPUT}/missions" "${OUTPUT}/preview-user")
configure_file("${HOG}" "${OUTPUT}/missions/neptunxl.hog" COPYONLY)
execute_process(
    COMMAND "${ENGINE}" -countdown-test production -hogdir "${DATA}" -extra-dir "${OUTPUT}" -userdir
            "${OUTPUT}/preview-user" -nosound -nomusic
    RESULT_VARIABLE result
    OUTPUT_VARIABLE log
    ERROR_VARIABLE error
    TIMEOUT 90)
file(WRITE "${OUTPUT}/runtime.log" "${log}${error}")
if(NOT result EQUAL 0 OR NOT log MATCHES "XL countdown runtime controls passed")
    message(FATAL_ERROR "Countdown controls failed (${result}): ${log}${error}")
endif()
foreach(version IN ITEMS 23 27)
    if(NOT log MATCHES "${version}/40 countdown: wall/master/object/autoplay")
        message(FATAL_ERROR "Countdown controls omitted layout ${version}")
    endif()
endforeach()
file(SHA256 "${HOG}" final_hash)
if(NOT final_hash STREQUAL original_hash)
    message(FATAL_ERROR "Original geometry changed during countdown controls")
endif()
