file(SHA256 "${SOUNDS}/boss01.wav" first)
file(SHA256 "${SOUNDS}/boss02.wav" second)
if(NOT first STREQUAL "f448d3506def983bd2ba3e9d79a24386930ff7aeba956f2eaed666b443201480"
   OR NOT second STREQUAL "91fa0e3fb5ff3187929e6511ebfebff002b8ef47c3225d5b99be42fd67571f3d")
    message(FATAL_ERROR "Original Dragon Pit WAV hashes differ")
endif()
execute_process(COMMAND "${TEST}" "${SOUNDS}/boss01.wav" "${SOUNDS}/boss02.wav"
                RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Original Dragon Pit WAV decoding failed")
endif()
