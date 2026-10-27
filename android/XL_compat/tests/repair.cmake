set(source_name neptunxl)
set(mode dispatch)
set(expected_hash 4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303)
set(reference_mode)
if(LEGACY)
    set(source_name obsrv-xl)
    set(mode legacy)
    set(reference_mode legacy)
    set(expected_hash e89acaad09cefa0c09fc4b487c56b6077477cfe20169f364fe54f627040b01e7)
endif()
file(SHA256 "${HOG}" original_hash)
if(NOT original_hash STREQUAL expected_hash)
    message(FATAL_ERROR "Repair center controls require the pinned original geometry")
endif()
file(MAKE_DIRECTORY "${OUTPUT}/missions" "${OUTPUT}/preview-user")
configure_file("${HOG}" "${OUTPUT}/missions/${source_name}.hog" COPYONLY)
configure_file("${DESCRIPTOR}" "${OUTPUT}/missions/${source_name}.mn2" COPYONLY)
execute_process(
    COMMAND "${ENGINE}" -repair-test ${mode} -hogdir "${DATA}" -extra-dir "${OUTPUT}" -userdir
            "${OUTPUT}/preview-user" -nosound -nomusic
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
    TIMEOUT 75)
file(WRITE "${OUTPUT}/controls.log" "${stdout}${stderr}")
string(FIND "${stdout}" "XL repair center controls passed" witness)
if(NOT result EQUAL 0 OR witness LESS 0)
    message(FATAL_ERROR "Repair center controls failed (${result}): ${stdout}${stderr}")
endif()
if(REFERENCE)
    execute_process(
        COMMAND "${REFERENCE}" "${OUTPUT}/repair-samples.txt" "${OUTPUT}/repair-frames.txt"
                ${reference_mode} RESULT_VARIABLE reference_result OUTPUT_VARIABLE reference_stdout
        ERROR_VARIABLE reference_stderr)
    file(WRITE "${OUTPUT}/reference.log" "${reference_stdout}${reference_stderr}")
    if(NOT reference_result EQUAL 0)
        message(
            FATAL_ERROR "Repair center reference mismatch: ${reference_stdout}${reference_stderr}")
    endif()
endif()
file(SHA256 "${HOG}" final_hash)
if(NOT final_hash STREQUAL original_hash)
    message(FATAL_ERROR "Original archive changed during repair segment controls")
endif()
