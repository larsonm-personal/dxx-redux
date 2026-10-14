# Both ordinary completion and an early error must close the actual audio device
foreach(expected_exit 0 7)
    execute_process(
        COMMAND "${HEADLESS_TEST_EXECUTABLE}" --headless-audio-lifecycle "${expected_exit}"
        RESULT_VARIABLE actual_exit
        OUTPUT_VARIABLE output
        ERROR_VARIABLE errors
        TIMEOUT 10)
    if(NOT "${actual_exit}" STREQUAL "${expected_exit}"
       OR NOT output MATCHES "Headless audio is closed at process teardown")
        message(
            FATAL_ERROR
                "Headless audio lifecycle failed: expected exit ${expected_exit}, got ${actual_exit}\n${output}\n${errors}"
        )
    endif()
endforeach()
