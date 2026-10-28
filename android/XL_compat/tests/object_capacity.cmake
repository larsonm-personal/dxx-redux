file(SHA256 "${HOG}" original_hash)
if(NOT original_hash STREQUAL "4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303")
    message(FATAL_ERROR "Object controls require original Neptune")
endif()
file(MAKE_DIRECTORY "${OUTPUT}/missions" "${OUTPUT}/preview-user")
configure_file("${HOG}" "${OUTPUT}/missions/neptunxl.hog" COPYONLY)
configure_file("${DESCRIPTOR}" "${OUTPUT}/missions/neptunxl.mn2" COPYONLY)
execute_process(
    COMMAND "${ENGINE}" -object-capacity-test dispatch -hogdir "${DATA}" -extra-dir "${OUTPUT}"
            -userdir "${OUTPUT}/preview-user" -nosound -nomusic
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
    TIMEOUT 240)
file(WRITE "${OUTPUT}/controls.log" "${stdout}${stderr}")
if(NOT result EQUAL 0 OR NOT stdout MATCHES "XL object capacity native controls passed")
    message(FATAL_ERROR "Object controls failed (${result}): ${stdout}${stderr}")
endif()
if(REFERENCE)
    execute_process(
        COMMAND "${REFERENCE}" "${OUTPUT}/object-reclaim.txt" "${OUTPUT}/object-signatures.txt"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE stdout
        ERROR_VARIABLE stderr
        TIMEOUT 30)
    file(WRITE "${OUTPUT}/reference.log" "${stdout}${stderr}")
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Object reference controls failed (${result}): ${stdout}${stderr}")
    endif()
endif()
file(SHA256 "${HOG}" final_hash)
if(NOT original_hash STREQUAL final_hash)
    message(FATAL_ERROR "Original Neptune changed during object capacity controls")
endif()
