# Earned ordinary Guided switch12 contact and settled cold-save owner.
foreach(
    required
    ENGINE
    DATA
    RESOURCES
    PRODUCER
    QUAD
    GUARD
    BLUE
    OUTPUT
    PYTHON)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Tulev1 guided_switch12 requires ${required}")
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
    message(FATAL_ERROR "Tulev1 guided_switch12 output retention failed")
endif()
execute_process(
    COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/pumo_tulev1_guided_switch12_contract.py" source
            "${PRODUCER}" "${ENGINE}" "${QUAD}" "${GUARD}" "${BLUE}"
    RESULT_VARIABLE valid
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
    TIMEOUT 25)
if(NOT valid EQUAL 0)
    message(FATAL_ERROR "Tulev1 guided_switch12 source invalid: ${stdout}${stderr}")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/pumo_player_phase.cmake")
set(owner "${OUTPUT}")
file(READ "${PRODUCER}" producer_receipt)
string(
    JSON
    source_file
    GET
    "${producer_receipt}"
    checkpoint_history
    0
    file)
get_filename_component(source_root "${PRODUCER}" DIRECTORY)
set(earned_save "${source_root}/preview-user/${source_file}")
set(source_producer "${PRODUCER}")
string(JSON source_pid GET "${producer_receipt}" process_id)
string(JSON source_nonce GET "${producer_receipt}" run_id)
set(seen_processes "${source_pid}")
set(seen_nonces "${source_nonce}")
foreach(phase warm cold)
    set(OUTPUT "${owner}/${phase}")
    file(MAKE_DIRECTORY "${OUTPUT}/preview-user")
    set(input_name "${source_file}")
    configure_file("${earned_save}" "${OUTPUT}/preview-user/${input_name}" COPYONLY)
    file(SHA256 "${OUTPUT}/preview-user/${input_name}" input_hash)
    file(READ "${CMAKE_CURRENT_LIST_DIR}/pumo_tulev1_guided_switch12_${phase}.json" plan)
    string(JSON plan SET "${plan}" resume_checkpoint "\"${input_name}\"")
    string(JSON plan SET "${plan}" resume_sha256 "\"${input_hash}\"")
    file(WRITE "${OUTPUT}/${phase}-plan.json" "${plan}\n")
    if(phase STREQUAL "warm")
        execute_process(
            COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/pumo_tulev1_guided_switch12_contract.py"
                    bind "${OUTPUT}/${phase}-plan.json" "${source_producer}" RESULT_VARIABLE bound)
        if(NOT bound EQUAL 0)
            message(FATAL_ERROR "Tulev1 guided_switch12 source binding failed")
        endif()
        set(expected_steps 1861)
    else()
        set(expected_steps 1)
    endif()
    run_pumo_player_phase(${phase} 7 7 ${expected_steps} 0)
    string(JSON process GET "${${phase}_receipt}" process_id)
    string(JSON nonce GET "${${phase}_receipt}" run_id)
    list(FIND seen_processes "${process}" reused_process)
    list(FIND seen_nonces "${nonce}" reused_nonce)
    if(NOT reused_process EQUAL -1 OR NOT reused_nonce EQUAL -1)
        message(FATAL_ERROR "Tulev1 guided_switch12 reused a process or nonce")
    endif()
    list(APPEND seen_processes "${process}")
    list(APPEND seen_nonces "${nonce}")
    if(phase STREQUAL "cold")
        set(original_producer "${PRODUCER}")
    else()
        set(original_producer)
    endif()
    execute_process(
        COMMAND
            "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/pumo_tulev1_guided_switch12_contract.py"
            "${phase}" "${OUTPUT}" "${ENGINE}" "${source_producer}" ${original_producer} "${QUAD}"
            "${GUARD}" "${BLUE}"
        RESULT_VARIABLE valid
        OUTPUT_VARIABLE stdout
        ERROR_VARIABLE stderr
        TIMEOUT 30)
    if(NOT valid EQUAL 0)
        message(FATAL_ERROR "Tulev1 guided_switch12 ${phase} witness failed: ${stdout}${stderr}")
    endif()
    if(phase STREQUAL "warm")
        string(JSON history_count LENGTH "${warm_receipt}" checkpoint_history)
        if(NOT history_count EQUAL 1)
            message(FATAL_ERROR "Tulev1 guided_switch12 requires one settled versioned save")
        endif()
        string(
            JSON
            saved_file
            GET
            "${warm_receipt}"
            checkpoint_history
            0
            file)
        string(JSON saved_hash GET "${warm_receipt}" checkpoint_history 0 sha256)
        set(earned_save "${OUTPUT}/preview-user/${saved_file}")
        set(source_file "${saved_file}")
        file(SHA256 "${earned_save}" actual_hash)
        if(NOT actual_hash STREQUAL saved_hash)
            message(FATAL_ERROR "Tulev1 guided_switch12 settled save changed")
        endif()
        set(source_producer "${OUTPUT}/warm-result.json")
    endif()
endforeach()
