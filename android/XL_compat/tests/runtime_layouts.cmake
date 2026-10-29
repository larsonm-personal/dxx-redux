file(SHA256 "${HOG}" source_hash)
if(NOT source_hash STREQUAL "4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303")
    message(FATAL_ERROR "Runtime layout controls require unchanged audited Neptune geometry")
endif()
file(MAKE_DIRECTORY "${OUTPUT}/missions" "${OUTPUT}/preview-user")
execute_process(COMMAND "${FIXTURE}" --runtime "${HOG}" "${OUTPUT}/missions" RESULT_VARIABLE result
                OUTPUT_VARIABLE log ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Cannot create modern layout controls: ${log}${error}")
endif()
if(REFERENCE)
    execute_process(COMMAND "${REFERENCE}" "${OUTPUT}/missions/runtime-reader.hog"
                    RESULT_VARIABLE result OUTPUT_VARIABLE log ERROR_VARIABLE error)
    file(WRITE "${OUTPUT}/reference.log" "${log}${error}")
    if(NOT result EQUAL 0 OR NOT log MATCHES "4 levels, 372 segments, 2232 sides")
        message(FATAL_ERROR "Derived modern layouts differ from the source reader: ${log}${error}")
    endif()
endif()
execute_process(
    COMMAND "${ENGINE}" -runtime-layouts-test production -hogdir "${DATA}" -extra-dir "${OUTPUT}"
            -userdir "${OUTPUT}/preview-user" -nosound -nomusic
    RESULT_VARIABLE result
    OUTPUT_VARIABLE log
    ERROR_VARIABLE error
    TIMEOUT 100)
file(WRITE "${OUTPUT}/runtime.log" "${log}${error}")
if(NOT result EQUAL 0 OR NOT log MATCHES "Canonical modern runtime layouts passed")
    message(FATAL_ERROR "Modern runtime layouts failed (${result}): ${log}${error}")
endif()
foreach(version IN ITEMS 23 25 27)
    if(NOT log MATCHES "${version}/40 runtime layout: geometry/normals/UVLs")
        message(FATAL_ERROR "Modern runtime controls omitted version ${version}")
    endif()
endforeach()
file(SHA256 "${HOG}" final_hash)
if(NOT final_hash STREQUAL source_hash)
    message(FATAL_ERROR "Original geometry changed during runtime controls")
endif()
