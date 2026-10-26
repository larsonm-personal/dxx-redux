file(MAKE_DIRECTORY "${OUTPUT}")
execute_process(COMMAND "${FIXTURE}" --object-fixtures "${OUTPUT}" RESULT_VARIABLE result
                OUTPUT_VARIABLE log ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Object fixture controls failed: ${log}${error}")
endif()
foreach(version IN ITEMS 15 17 23)
    execute_process(COMMAND "${REFERENCE}" "${OUTPUT}/${version}.rl2" RESULT_VARIABLE result
                    OUTPUT_VARIABLE log ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Object reference ${version} failed: ${log}${error}")
    endif()
    message(STATUS "${log}")
endforeach()
