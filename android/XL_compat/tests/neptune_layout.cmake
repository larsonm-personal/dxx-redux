file(SHA256 "${HOG}" before_hash)
if(NOT before_hash STREQUAL "4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303")
    message(FATAL_ERROR "Neptune layout test requires the audited original HOG")
endif()
file(MAKE_DIRECTORY "${OUTPUT}/preview-user")
execute_process(
    COMMAND "${ENGINE}" -format23-test "${HOG}" -hogdir "${DATA}" -extra-dir "${OUTPUT}" -userdir
            "${OUTPUT}/preview-user" -nosound -nomusic
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
    TIMEOUT 60)
file(WRITE "${OUTPUT}/layout.log" "${stdout}${stderr}")
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Neptune native layout failed (${result}): ${stdout}${stderr}")
endif()
if(NOT
   stdout
   MATCHES
   "23/40 native layout: geometry, UVLs, walls, object identities/positions, triggers and stock recovery passed"
)
    message(FATAL_ERROR "Neptune layout coverage did not finish")
endif()
file(SHA256 "${HOG}" after_hash)
if(NOT after_hash STREQUAL before_hash)
    message(FATAL_ERROR "Original Neptune HOG changed during qualification")
endif()
