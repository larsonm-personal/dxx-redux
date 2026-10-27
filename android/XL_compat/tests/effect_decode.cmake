file(MAKE_DIRECTORY "${OUTPUT}")
execute_process(COMMAND "${FIXTURE}" --effect-fixtures "${OUTPUT}" RESULT_VARIABLE result
                OUTPUT_VARIABLE log ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Effect fixture generation failed: ${log}${error}")
endif()
execute_process(COMMAND "${REFERENCE}" "${OUTPUT}" RESULT_VARIABLE result OUTPUT_VARIABLE log
                ERROR_VARIABLE error)
file(WRITE "${OUTPUT}/reference.log" "${log}${error}")
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Effect reader comparison failed: ${log}${error}")
endif()
