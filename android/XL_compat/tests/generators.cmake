file(MAKE_DIRECTORY "${OUTPUT}")
execute_process(COMMAND "${FIXTURE}" --generator-fixtures "${OUTPUT}" RESULT_VARIABLE result
                OUTPUT_VARIABLE log ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Generator assignment controls failed: ${log}${error}")
endif()
execute_process(COMMAND "${REFERENCE}" "${OUTPUT}" RESULT_VARIABLE result
                OUTPUT_VARIABLE reference_log ERROR_VARIABLE error)
file(WRITE "${OUTPUT}/reference.log" "${log}${reference_log}${error}")
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Generator reference mismatch: ${reference_log}${error}")
endif()
