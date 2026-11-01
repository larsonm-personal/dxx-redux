file(SHA256 "${HOG}" original_hash)
if(NOT original_hash STREQUAL "f0611f1428549373526dfca8ca5a1e1883ace910f4209a938698086d189919db")
    message(FATAL_ERROR "HXM controls require original Apocalyptic Factor XL")
endif()
file(MAKE_DIRECTORY "${OUTPUT}/missions" "${OUTPUT}/preview-user")
if(CAMERA_HOG)
    file(SHA256 "${CAMERA_HOG}" camera_hash)
    if(NOT camera_hash STREQUAL "df69c62aad949f2d92bc1bbea4b597623cd29d0a0f594cdcc3d66568d6ce482a")
        message(FATAL_ERROR "Camera controls require the original XL addon HOG")
    endif()
    file(MAKE_DIRECTORY "${OUTPUT}/data")
    configure_file("${CAMERA_HOG}" "${OUTPUT}/data/d2x-xl.hog" COPYONLY)
endif()
configure_file("${HOG}" "${OUTPUT}/missions/hxmtest.hog" COPYONLY)
file(WRITE "${OUTPUT}/missions/hxmtest.mn2"
     "d2x-name = Alarlof HXM control\nnum_levels = 1\nalarlof.rl2\n")
execute_process(
    COMMAND "${ENGINE}" -hxm-test dispatch -hogdir "${DATA}" -extra-dir "${OUTPUT}" -userdir
            "${OUTPUT}/preview-user" -nosound -nomusic
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
    TIMEOUT 240)
file(WRITE "${OUTPUT}/controls.log" "${stdout}${stderr}")
if(NOT result EQUAL 0 OR NOT stdout MATCHES "Original HXM normal startup")
    message(FATAL_ERROR "HXM native controls failed: ${result}: ${stdout}${stderr}")
endif()
if(CAMERA_HOG)
    if(NOT stdout MATCHES "Original camera HXM native append"
       OR NOT stdout MATCHES "Native camera object initialization"
       OR NOT stdout MATCHES "Native camera/player contact: 8 ordered dispatch")
        message(FATAL_ERROR "Camera resource controls were not exercised")
    endif()
    file(SHA256 "${CAMERA_HOG}" final_camera_hash)
    if(NOT final_camera_hash STREQUAL camera_hash)
        message(FATAL_ERROR "Original camera addon archive changed")
    endif()
endif()
file(SHA256 "${HOG}" final_hash)
if(NOT final_hash STREQUAL original_hash)
    message(FATAL_ERROR "Original HXM archive changed")
endif()
if(REFERENCE)
    execute_process(COMMAND "${REFERENCE}" "${HOG}" RESULT_VARIABLE result OUTPUT_VARIABLE stdout
                    ERROR_VARIABLE stderr)
    file(WRITE "${OUTPUT}/reference.log" "${stdout}${stderr}")
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "HXM source comparison failed: ${stdout}${stderr}")
    endif()
endif()
