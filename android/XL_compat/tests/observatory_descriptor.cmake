file(SHA256 "${HOG}" hog_hash)
file(SHA256 "${DESCRIPTOR}" descriptor_hash)
if(NOT hog_hash STREQUAL "e89acaad09cefa0c09fc4b487c56b6077477cfe20169f364fe54f627040b01e7"
   OR NOT descriptor_hash STREQUAL
      "8a2946c7f786f4834e1c9e303a8d4248b419b89479a750d0d6c1917fbe1b6317")
    message(FATAL_ERROR "Descriptor test requires original Observatory HOG and descriptor")
endif()
file(MAKE_DIRECTORY "${OUTPUT}/missions" "${OUTPUT}/preview-user")
configure_file("${HOG}" "${OUTPUT}/missions/obsrv-xl.hog" COPYONLY)
configure_file("${DESCRIPTOR}" "${OUTPUT}/missions/obsrv-xl.mn2" COPYONLY)
execute_process(
    COMMAND "${ENGINE}" -descriptor-test observatory -hogdir "${DATA}" -extra-dir "${OUTPUT}"
            -userdir "${OUTPUT}/preview-user" -nosound -nomusic
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
    TIMEOUT 60)
file(WRITE "${OUTPUT}/descriptor.log" "${stdout}${stderr}")
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Descriptor test failed (${result}): ${stdout}${stderr}")
endif()
file(SHA256 "${HOG}" final_hog_hash)
file(SHA256 "${DESCRIPTOR}" final_descriptor_hash)
if(NOT final_hog_hash STREQUAL hog_hash OR NOT final_descriptor_hash STREQUAL descriptor_hash)
    message(FATAL_ERROR "Original descriptor sources changed")
endif()
message(STATUS "${stdout}")
