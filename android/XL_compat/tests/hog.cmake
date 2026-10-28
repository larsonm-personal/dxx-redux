file(SHA256 "${HOG}" original_hash)
if(NOT original_hash STREQUAL "4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303")
    message(FATAL_ERROR "HOG controls require original Neptune geometry/resources")
endif()
file(MAKE_DIRECTORY "${OUTPUT}/missions" "${OUTPUT}/preview-user")
configure_file("${HOG}" "${OUTPUT}/missions/neptunxl.hog" COPYONLY)
execute_process(
    COMMAND "${ENGINE}" -hog-test dispatch -hogdir "${DATA}" -extra-dir "${OUTPUT}" -userdir
            "${OUTPUT}/preview-user" -nosound -nomusic
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
    TIMEOUT 120)
file(WRITE "${OUTPUT}/controls.log" "${stdout}${stderr}")
string(FIND "${stdout}" "Extended HOG native lifecycle passed" witness)
if(NOT result EQUAL 0 OR witness LESS 0)
    message(FATAL_ERROR "HOG controls failed (${result}): ${stdout}${stderr}")
endif()
file(SHA256 "${HOG}" final_hash)
if(NOT final_hash STREQUAL original_hash)
    message(FATAL_ERROR "Original Neptune changed during HOG controls")
endif()
