# Assisted-local switch 13, dynamically sourced from registered switch 14 cold.
foreach(
    required
    ENGINE
    DATA
    RESOURCES
    SWITCH14
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
        message(FATAL_ERROR "Tulev1 switch13 requires ${required}")
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
    message(FATAL_ERROR "Tulev1 switch13 output retention failed")
endif()
execute_process(
    COMMAND
        "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/pumo_tulev1_switch13_contract.py" source
        "${SWITCH14}" "${ENGINE}" "${SUPPLIES}" "${COURTYARD}" "${BLAST}" "${GUIDED_PRODUCER}"
        "${GUIDED_GUARD}" "${QUAD}" "${GUARD}" "${BLUE}"
    RESULT_VARIABLE valid
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
    TIMEOUT 60)
if(NOT valid EQUAL 0)
    message(FATAL_ERROR "Tulev1 switch13 registered source invalid: ${stdout}${stderr}")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/pumo_player_phase.cmake")
set(OWNER "${OUTPUT}")
set(SOURCE_PRODUCER "${SWITCH14}/cold/cold-result.json")
file(READ "${SOURCE_PRODUCER}" source_receipt)
string(JSON source_pid GET "${source_receipt}" process_id)
string(JSON source_nonce GET "${source_receipt}" run_id)
set(SEEN_PROCESSES "${source_pid}")
set(SEEN_NONCES "${source_nonce}")
foreach(kind warm cold)
    set(OUTPUT "${OWNER}/${kind}")
    file(MAKE_DIRECTORY "${OUTPUT}/preview-user")
    file(READ "${SOURCE_PRODUCER}" producer_receipt)
    string(JSON history_count LENGTH "${producer_receipt}" checkpoint_history)
    if(NOT history_count EQUAL 1)
        message(FATAL_ERROR "Tulev1 switch13 ${kind} needs one immediate predecessor save")
    endif()
    string(
        JSON
        source_file
        GET
        "${producer_receipt}"
        checkpoint_history
        0
        file)
    get_filename_component(source_root "${SOURCE_PRODUCER}" DIRECTORY)
    set(EARNED_SAVE "${source_root}/preview-user/${source_file}")
    configure_file("${EARNED_SAVE}" "${OUTPUT}/preview-user/${source_file}" COPYONLY)
    file(SHA256 "${OUTPUT}/preview-user/${source_file}" input_hash)
    file(READ "${CMAKE_CURRENT_LIST_DIR}/pumo_tulev1_switch13_${kind}.json" plan)
    string(JSON plan SET "${plan}" resume_checkpoint "\"${source_file}\"")
    string(JSON plan SET "${plan}" resume_sha256 "\"${input_hash}\"")
    file(WRITE "${OUTPUT}/${kind}-plan.json" "${plan}\n")
    execute_process(
        COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/pumo_tulev1_switch13_contract.py" bind
                "${kind}" "${OUTPUT}/${kind}-plan.json" "${SOURCE_PRODUCER}"
        RESULT_VARIABLE bound
        OUTPUT_VARIABLE stdout
        ERROR_VARIABLE stderr
        TIMEOUT 30)
    if(NOT bound EQUAL 0)
        message(FATAL_ERROR "Tulev1 switch13 ${kind} binding failed: ${stdout}${stderr}")
    endif()
    if(kind STREQUAL "warm")
        set(EXPECTED_STEPS 19)
    else()
        set(EXPECTED_STEPS 3)
    endif()
    run_pumo_player_phase(${kind} 7 7 ${EXPECTED_STEPS} 0)
    string(JSON process GET "${${kind}_receipt}" process_id)
    string(JSON nonce GET "${${kind}_receipt}" run_id)
    list(FIND SEEN_PROCESSES "${process}" reused_process)
    list(FIND SEEN_NONCES "${nonce}" reused_nonce)
    if(NOT reused_process EQUAL -1 OR NOT reused_nonce EQUAL -1)
        message(FATAL_ERROR "Tulev1 switch13 reused process or nonce")
    endif()
    list(APPEND SEEN_PROCESSES "${process}")
    list(APPEND SEEN_NONCES "${nonce}")
    execute_process(
        COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/pumo_tulev1_switch13_contract.py" verify
                "${kind}" "${OUTPUT}" "${ENGINE}" "${SOURCE_PRODUCER}"
        RESULT_VARIABLE valid
        OUTPUT_VARIABLE stdout
        ERROR_VARIABLE stderr
        TIMEOUT 45)
    if(NOT valid EQUAL 0)
        message(FATAL_ERROR "Tulev1 switch13 ${kind} witness failed: ${stdout}${stderr}")
    endif()
    set(SOURCE_PRODUCER "${OUTPUT}/${kind}-result.json")
endforeach()
