# Assisted-local switch 14, dynamically sourced from registered Guided140 cold.
foreach(
    required
    ENGINE
    DATA
    RESOURCES
    SUPPLIES
    COURTYARD
    BLAST
    GUIDED_PRODUCER
    GUIDED_GUARD
    QUAD
    GUARD
    BLUE
    OUTPUT
    PYTHON)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Tulev1 switch14 requires ${required}")
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
    message(FATAL_ERROR "Tulev1 switch14 output retention failed")
endif()
execute_process(
    COMMAND
        "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/pumo_tulev1_switch14_contract.py" source
        "${SUPPLIES}" "${ENGINE}" "${COURTYARD}" "${BLAST}" "${GUIDED_PRODUCER}" "${GUIDED_GUARD}"
        "${QUAD}" "${GUARD}" "${BLUE}"
    RESULT_VARIABLE valid
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
    TIMEOUT 60)
if(NOT valid EQUAL 0)
    message(FATAL_ERROR "Tulev1 switch14 registered source invalid: ${stdout}${stderr}")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/pumo_player_phase.cmake")
set(owner "${OUTPUT}")
set(source_producer "${SUPPLIES}/guided140_cold/guided140_cold-result.json")
file(READ "${source_producer}" source_receipt)
string(JSON source_pid GET "${source_receipt}" process_id)
string(JSON source_nonce GET "${source_receipt}" run_id)
set(seen_processes "${source_pid}")
set(seen_nonces "${source_nonce}")
foreach(kind warm cold)
    set(OUTPUT "${owner}/${kind}")
    file(MAKE_DIRECTORY "${OUTPUT}/preview-user")
    file(READ "${source_producer}" producer_receipt)
    string(JSON history_count LENGTH "${producer_receipt}" checkpoint_history)
    if(NOT history_count EQUAL 1)
        message(FATAL_ERROR "Tulev1 switch14 ${kind} needs one immediate predecessor save")
    endif()
    string(
        JSON
        source_file
        GET
        "${producer_receipt}"
        checkpoint_history
        0
        file)
    get_filename_component(source_root "${source_producer}" DIRECTORY)
    set(earned_save "${source_root}/preview-user/${source_file}")
    configure_file("${earned_save}" "${OUTPUT}/preview-user/${source_file}" COPYONLY)
    file(SHA256 "${OUTPUT}/preview-user/${source_file}" input_hash)
    file(READ "${CMAKE_CURRENT_LIST_DIR}/pumo_tulev1_switch14_${kind}.json" plan)
    string(JSON plan SET "${plan}" resume_checkpoint "\"${source_file}\"")
    string(JSON plan SET "${plan}" resume_sha256 "\"${input_hash}\"")
    file(WRITE "${OUTPUT}/${kind}-plan.json" "${plan}\n")
    execute_process(
        COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/pumo_tulev1_switch14_contract.py" bind
                "${kind}" "${OUTPUT}/${kind}-plan.json" "${source_producer}"
        RESULT_VARIABLE bound
        OUTPUT_VARIABLE stdout
        ERROR_VARIABLE stderr
        TIMEOUT 30)
    if(NOT bound EQUAL 0)
        message(FATAL_ERROR "Tulev1 switch14 ${kind} binding failed: ${stdout}${stderr}")
    endif()
    if(kind STREQUAL "warm")
        set(expected_steps 23)
    else()
        set(expected_steps 3)
    endif()
    run_pumo_player_phase(${kind} 7 7 ${expected_steps} 0)
    string(JSON process GET "${${kind}_receipt}" process_id)
    string(JSON nonce GET "${${kind}_receipt}" run_id)
    list(FIND seen_processes "${process}" reused_process)
    list(FIND seen_nonces "${nonce}" reused_nonce)
    if(NOT reused_process EQUAL -1 OR NOT reused_nonce EQUAL -1)
        message(FATAL_ERROR "Tulev1 switch14 reused process or nonce")
    endif()
    list(APPEND seen_processes "${process}")
    list(APPEND seen_nonces "${nonce}")
    execute_process(
        COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/pumo_tulev1_switch14_contract.py" verify
                "${kind}" "${OUTPUT}" "${ENGINE}" "${source_producer}"
        RESULT_VARIABLE valid
        OUTPUT_VARIABLE stdout
        ERROR_VARIABLE stderr
        TIMEOUT 45)
    if(NOT valid EQUAL 0)
        message(FATAL_ERROR "Tulev1 switch14 ${kind} witness failed: ${stdout}${stderr}")
    endif()
    set(source_producer "${OUTPUT}/${kind}-result.json")
endforeach()
