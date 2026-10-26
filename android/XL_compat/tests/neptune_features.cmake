file(SHA256 "${HOG}" hog_hash)
file(SHA256 "${DESCRIPTOR}" descriptor_hash)
if(NOT hog_hash STREQUAL "4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303"
   OR NOT descriptor_hash STREQUAL
      "f5d3cc0a2f56acbafafc282883bf0e5078aff60706d8bf0fd03f4c5dc311c753")
    message(FATAL_ERROR "Neptune runtime requires the unchanged original HOG and descriptor")
endif()
file(MAKE_DIRECTORY "${OUTPUT}/missions" "${OUTPUT}/preview-user")
configure_file("${HOG}" "${OUTPUT}/missions/neptunxl.hog" COPYONLY)
configure_file("${DESCRIPTOR}" "${OUTPUT}/missions/neptunxl.mn2" COPYONLY)
execute_process(
    COMMAND
        "${ENGINE}" -neptune-test features -hogdir "${DATA}" -extra-dir "${OUTPUT}" -userdir
        "${OUTPUT}/preview-user" -mission neptunxl -nosound -nomusic -fixture-path "${OUTPUT}"
        -xl-test-navigation
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
    TIMEOUT 75)
file(WRITE "${OUTPUT}/features.log" "${stdout}${stderr}")
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Neptune native features failed (${result}): ${stdout}${stderr}")
endif()
foreach(
    witness IN
    ITEMS "switched-light save/restore passed"
          "controlled native exit dispatch completed"
          "stock-to-save restoration passed"
          "native reactor countdown and linked doors passed"
          "all three fuel stations and absent repair delivery passed"
          "authored door interactions passed"
          "all eleven authored powerup kinds collected")
    string(FIND "${stdout}" "${witness}" found)
    if(found LESS 0)
        message(FATAL_ERROR "Neptune test did not reach required witness: ${witness}")
    endif()
endforeach()
file(SHA256 "${HOG}" final_hog)
file(SHA256 "${DESCRIPTOR}" final_descriptor)
if(NOT final_hog STREQUAL hog_hash OR NOT final_descriptor STREQUAL descriptor_hash)
    message(FATAL_ERROR "Original Neptune files changed during qualification")
endif()
