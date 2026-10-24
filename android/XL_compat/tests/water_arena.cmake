# Private bytes stay outside the repository; hashes pin original resources too
file(SHA256 "${HOG}" hog_hash)
file(SHA256 "${DESCRIPTOR}" descriptor_hash)
if(NOT hog_hash STREQUAL "adbfea019c1ac29ac922e15876c22ca6ce30eb02d598a98e6ef7dfdce2b2dd3e"
   OR NOT descriptor_hash STREQUAL
      "14150755f14e210b284aa03c45d75e1b2f96be7b567bbc324bf4986b96928a71")
    message(FATAL_ERROR "Water arena requires the audited original HOG and descriptor")
endif()
file(MAKE_DIRECTORY "${OUTPUT}/missions" "${OUTPUT}/preview-user")
configure_file("${HOG}" "${OUTPUT}/missions/rtarena.hog" COPYONLY)
configure_file("${DESCRIPTOR}" "${OUTPUT}/missions/rtarena.mn2" COPYONLY)
execute_process(
    COMMAND "${ENGINE}" -arena-test water -xl-water-arena -hogdir "${DATA}" -extra-dir "${OUTPUT}"
            -userdir "${OUTPUT}/preview-user" -mission rtarena -nosound -nomusic
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
    TIMEOUT 90)
file(WRITE "${OUTPUT}/water.log" "${stdout}${stderr}")
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Water arena failed (${result}): ${stdout}${stderr}")
endif()
file(SHA256 "${HOG}" final_hog_hash)
file(SHA256 "${DESCRIPTOR}" final_descriptor_hash)
if(NOT final_hog_hash STREQUAL hog_hash OR NOT final_descriptor_hash STREQUAL descriptor_hash)
    message(FATAL_ERROR "Original arena sources changed during the test")
endif()
message(STATUS "${stdout}")
