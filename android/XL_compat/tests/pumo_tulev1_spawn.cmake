# Fresh original Sophodo warm arrival; bounded local assists, no full-flight claim.
foreach(
    required
    ENGINE
    DATA
    RESOURCES
    ENTRY
    PRODUCER
    OUTPUT
    PYTHON)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Tulev1 spawn requires ${required}")
    endif()
endforeach()
find_program(PWSH pwsh REQUIRED)
execute_process(
    COMMAND
        "${PWSH}" -NoProfile -File
        "${CMAKE_CURRENT_LIST_DIR}/../../helpers/retain-recent-artifacts.ps1" -Artifacts "${OUTPUT}"
        -Keep 1000
    RESULT_VARIABLE retained)
if(NOT retained EQUAL 0)
    message(FATAL_ERROR "Tulev1 spawn output retention failed")
endif()
execute_process(
    COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/pumo_tulev1_spawn_contract.py" producer
            "${PRODUCER}" "${ENTRY}" "${ENGINE}"
    RESULT_VARIABLE valid
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
    TIMEOUT 20)
if(NOT valid EQUAL 0)
    message(FATAL_ERROR "Tulev1 spawn source invalid: ${stdout}${stderr}")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/pumo_player_phase.cmake")
set(owner "${OUTPUT}")
set(earned_save "${ENTRY}")
set(seen_processes)
set(seen_nonces)
foreach(phase door127 door211 guided137 door138 load)
    set(OUTPUT "${owner}/${phase}")
    file(MAKE_DIRECTORY "${OUTPUT}/preview-user")
    set(input_name "tulev1-spawn-${phase}-input.sav")
    configure_file("${earned_save}" "${OUTPUT}/preview-user/${input_name}" COPYONLY)
    file(SHA256 "${OUTPUT}/preview-user/${input_name}" input_hash)
    file(READ "${CMAKE_CURRENT_LIST_DIR}/pumo_tulev1_spawn_${phase}.json" plan)
    string(JSON plan SET "${plan}" resume_checkpoint "\"${input_name}\"")
    string(JSON plan SET "${plan}" resume_sha256 "\"${input_hash}\"")
    file(WRITE "${OUTPUT}/${phase}-plan.json" "${plan}\n")
    if(phase STREQUAL "load")
        set(expected_steps 1)
    else()
        set(expected_steps 9)
    endif()
    run_pumo_player_phase(${phase} 7 7 ${expected_steps} 0)
    string(JSON process GET "${${phase}_receipt}" process_id)
    string(JSON nonce GET "${${phase}_receipt}" run_id)
    list(FIND seen_processes "${process}" reused_process)
    list(FIND seen_nonces "${nonce}" reused_nonce)
    if(NOT reused_process EQUAL -1 OR NOT reused_nonce EQUAL -1)
        message(FATAL_ERROR "Tulev1 spawn reused a process or nonce")
    endif()
    list(APPEND seen_processes "${process}")
    list(APPEND seen_nonces "${nonce}")
    execute_process(
        COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/pumo_tulev1_spawn_contract.py" phase
                "${phase}" "${OUTPUT}" "${ENGINE}" "${PRODUCER}"
        RESULT_VARIABLE valid
        OUTPUT_VARIABLE stdout
        ERROR_VARIABLE stderr
        TIMEOUT 20)
    if(NOT valid EQUAL 0)
        message(FATAL_ERROR "Tulev1 spawn ${phase} witness failed: ${stdout}${stderr}")
    endif()
    if(NOT phase STREQUAL "load")
        string(
            JSON
            saved_file
            GET
            "${${phase}_receipt}"
            checkpoint_history
            0
            file)
        string(JSON saved_hash GET "${${phase}_receipt}" checkpoint_history 0 sha256)
        set(earned_save "${OUTPUT}/preview-user/${saved_file}")
        file(SHA256 "${earned_save}" actual_hash)
        if(NOT actual_hash STREQUAL saved_hash)
            message(FATAL_ERROR "Tulev1 spawn ${phase} versioned save changed")
        endif()
    endif()
endforeach()
