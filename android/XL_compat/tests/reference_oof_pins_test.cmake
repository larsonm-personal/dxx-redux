# Binary I/O is essential: CMake file(WRITE) expands CRLF again on Windows
if(NOT EXISTS "${PYTHON}")
    message(FATAL_ERROR "OOF pin controls require the configured Python interpreter")
endif()
execute_process(
    COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/reference_oof_pins_test.py" --cmake
            "${CMAKE_COMMAND}" --reference "${REFERENCE}" --output "${OUTPUT}"
    RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "OOF source pin controls failed (${result})")
endif()
