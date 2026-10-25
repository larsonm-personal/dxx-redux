file(SHA256 "${HOG}" hog_hash)
file(SHA256 "${DESCRIPTOR}" descriptor_hash)
if(NOT hog_hash STREQUAL "914231998b80f3900508fa6847adb8d60b6b78670cf7afa1e20e90c9170e46e1"
   OR NOT descriptor_hash STREQUAL
      "26865fb664d93eaa54acaec9d857bff15b192e1dfcca1598229388f6f77e3d94")
    message(FATAL_ERROR "Konflict acceptance requires the unchanged original distribution")
endif()
file(MAKE_DIRECTORY "${OUTPUT}/missions" "${OUTPUT}/preview-user")
configure_file("${HOG}" "${OUTPUT}/missions/konflict.hog" COPYONLY)
configure_file("${DESCRIPTOR}" "${OUTPUT}/missions/konflict.mn2" COPYONLY)
execute_process(
    COMMAND
        "${ENGINE}" -konflict-test features -hogdir "${DATA}" -extra-dir "${OUTPUT}" -userdir
        "${OUTPUT}/preview-user" -mission konflict -nosound -nomusic -fixture-path "${OUTPUT}"
        -xl-test-navigation
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
    TIMEOUT 75)
file(WRITE "${OUTPUT}/features.log" "${stdout}${stderr}")
if(NOT result EQUAL 0
   OR NOT
      stdout
      MATCHES
      "Konflict: original descriptor, 17/36 startup, authored LGT, switched-light save/restore passed"
   OR NOT stdout MATCHES "Konflict: stock isolation passed"
   OR NOT stdout MATCHES "Konflict geometry/light records match original"
   OR NOT stdout MATCHES "owned lighting, geometry and RNG preserved"
   OR NOT stdout MATCHES "native reactor countdown and linked doors passed"
   OR NOT stdout MATCHES "controlled native exit dispatch completed"
   OR NOT stdout MATCHES "loose companion explicitly rejected"
   OR NOT stdout MATCHES "zero LGT disables emitters")
    message(FATAL_ERROR "Konflict feature check failed (${result}): ${stdout}${stderr}")
endif()
file(SHA256 "${HOG}" final_hog_hash)
file(SHA256 "${DESCRIPTOR}" final_descriptor_hash)
if(NOT final_hog_hash STREQUAL hog_hash OR NOT final_descriptor_hash STREQUAL descriptor_hash)
    message(FATAL_ERROR "Original Konflict sources changed")
endif()
message(STATUS "${stdout}")
