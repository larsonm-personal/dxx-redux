file(SHA256 "${HOG}" original_hash)
if(NOT original_hash STREQUAL "f0611f1428549373526dfca8ca5a1e1883ace910f4209a938698086d189919db")
    message(FATAL_ERROR "15/34 controls require original Apocalyptic Factor XL")
endif()
file(MAKE_DIRECTORY "${OUTPUT}/missions" "${OUTPUT}/preview-user")
configure_file("${HOG}" "${OUTPUT}/missions/orig34.hog" COPYONLY)
file(WRITE "${OUTPUT}/missions/orig34.mn2"
     "d2x-name = Derived format 15-34\nnum_levels = 1\nalarlof.rl2\n")
execute_process(
    COMMAND "${ENGINE}" -format34-test dispatch -hogdir "${DATA}" -extra-dir "${OUTPUT}" -userdir
            "${OUTPUT}/preview-user" -nosound -nomusic
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
    TIMEOUT 240)
file(WRITE "${OUTPUT}/controls.log" "${stdout}${stderr}")
if(NOT result EQUAL 0 OR NOT stdout MATCHES "XL 15/34 native geometry")
    message(FATAL_ERROR "15/34 native controls failed: ${result}: ${stdout}${stderr}")
endif()
file(SHA256 "${HOG}" final_hash)
if(NOT final_hash STREQUAL original_hash)
    message(FATAL_ERROR "Original 15/34 archive changed")
endif()

if(REFERENCE)
    execute_process(COMMAND "${REFERENCE}" "${HOG}" RESULT_VARIABLE result OUTPUT_VARIABLE stdout
                    ERROR_VARIABLE stderr)
    file(WRITE "${OUTPUT}/reference.log" "${stdout}${stderr}")
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "15/34 source comparison failed: ${stdout}${stderr}")
    endif()
endif()
