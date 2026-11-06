# Both source adapters are mandatory for this owner, even outside private selection
foreach(oracle IN ITEMS REFERENCE DAMAGE_REFERENCE)
    if(NOT ${oracle} OR NOT EXISTS "${${oracle}}")
        message(FATAL_ERROR "Modern runtime layouts require the pinned ${oracle} oracle")
    endif()
endforeach()
file(SHA256 "${HOG}" source_hash)
if(NOT source_hash STREQUAL "4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303")
    message(FATAL_ERROR "Runtime layout controls require unchanged audited Neptune geometry")
endif()
file(MAKE_DIRECTORY "${OUTPUT}/missions" "${OUTPUT}/preview-user")
execute_process(COMMAND "${FIXTURE}" --runtime-v21 "${HOG}" "${OUTPUT}/missions"
                RESULT_VARIABLE result OUTPUT_VARIABLE log ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Cannot create modern layout controls: ${log}${error}")
endif()
if(REFERENCE)
    execute_process(COMMAND "${REFERENCE}" "${OUTPUT}/missions/runtime-reader.hog"
                    RESULT_VARIABLE result OUTPUT_VARIABLE log ERROR_VARIABLE error)
    file(WRITE "${OUTPUT}/reference.log" "${log}${error}")
    if(NOT result EQUAL 0 OR NOT log MATCHES "5 levels, 465 segments, 2790 sides")
        message(FATAL_ERROR "Derived modern layouts differ from the source reader: ${log}${error}")
    endif()
endif()
execute_process(
    COMMAND "${ENGINE}" -runtime-layouts-test production -hogdir "${DATA}" -extra-dir "${OUTPUT}"
            -userdir "${OUTPUT}/preview-user" -nosound -nomusic
    RESULT_VARIABLE result
    OUTPUT_VARIABLE log
    ERROR_VARIABLE error
    TIMEOUT 150)
file(WRITE "${OUTPUT}/runtime.log" "${log}${error}")
if(NOT result EQUAL 0 OR NOT log MATCHES "Canonical modern runtime layouts passed")
    message(FATAL_ERROR "Modern runtime layouts failed (${result}): ${log}${error}")
endif()
foreach(version IN ITEMS 21 23 25 27)
    if(NOT log MATCHES "${version}/40 runtime layout: geometry/normals/UVLs")
        message(FATAL_ERROR "Modern runtime controls omitted version ${version}")
    endif()
endforeach()
file(SHA256 "${HOG}" final_hash)
if(NOT final_hash STREQUAL source_hash)
    message(FATAL_ERROR "Original geometry changed during runtime controls")
endif()

# Private damage controls use a separate package; Android fixture bytes stay intact
file(MAKE_DIRECTORY "${OUTPUT}/damage/missions" "${OUTPUT}/damage/preview-user")
execute_process(COMMAND "${FIXTURE}" --runtime-damage "${HOG}" "${OUTPUT}/damage/missions"
                RESULT_VARIABLE result OUTPUT_VARIABLE log ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Cannot create private derived segment damage controls: ${log}${error}")
endif()
execute_process(COMMAND "${REFERENCE}" "${OUTPUT}/damage/missions/runtime-reader.hog"
                RESULT_VARIABLE result OUTPUT_VARIABLE log ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Derived damage reader differs: ${log}${error}")
endif()
execute_process(
    COMMAND "${ENGINE}" -runtime-layouts-test segment-damage -hogdir "${DATA}" -extra-dir
            "${OUTPUT}/damage" -userdir "${OUTPUT}/damage/preview-user" -nosound -nomusic
    RESULT_VARIABLE result
    OUTPUT_VARIABLE log
    ERROR_VARIABLE error
    TIMEOUT 150)
file(WRITE "${OUTPUT}/damage/runtime.log" "${log}${error}")
if(NOT result EQUAL 0 OR NOT log MATCHES "Authored segment damage controls passed")
    message(FATAL_ERROR "Native segment damage failed (${result}): ${log}${error}")
endif()
execute_process(COMMAND "${DAMAGE_REFERENCE}" "${OUTPUT}/damage/segment-damage-samples.txt"
                RESULT_VARIABLE result OUTPUT_VARIABLE log ERROR_VARIABLE error)
file(WRITE "${OUTPUT}/damage/reference.log" "${log}${error}")
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Actual segment frames differ from pinned source: ${log}${error}")
endif()
