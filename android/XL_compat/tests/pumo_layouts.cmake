file(SHA256 "${HOG}" source_hash)
if(NOT source_hash STREQUAL "6e3e723e549897188a8fdc19db8b04630de9d428d2b31f8e3f48af108915b043")
    message(FATAL_ERROR "Pumo reader comparison requires the unchanged original HOG")
endif()
execute_process(COMMAND "${LAYOUT}" "${HOG}" RESULT_VARIABLE result OUTPUT_VARIABLE log
                ERROR_VARIABLE error)
if(NOT result EQUAL 0 OR NOT log MATCHES
                         "14 levels, 19131 segments, 114786 sides, 1577494 comparisons")
    message(FATAL_ERROR "Incomplete Pumo source geometry/extras/fog comparison: ${log}${error}")
endif()
execute_process(COMMAND "${OBJECT}" "${HOG}" RESULT_VARIABLE result OUTPUT_VARIABLE objects
                ERROR_VARIABLE error)
if(NOT result EQUAL 0 OR NOT objects MATCHES
                         "14 levels, 3182 object ends/identities and 795 effect payloads matched")
    message(FATAL_ERROR "Incomplete Pumo source object comparison: ${objects}${error}")
endif()
file(SHA256 "${HOG}" final_hash)
if(NOT final_hash STREQUAL source_hash)
    message(FATAL_ERROR "Original Pumo HOG changed during comparison")
endif()
message(STATUS "${log}${objects}Original bytes unchanged; no gameplay admission granted")
