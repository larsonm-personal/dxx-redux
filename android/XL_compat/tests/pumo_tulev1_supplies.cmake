# Incremental earned supplies after the registered courtyard guard owner.
foreach(
    required
    ENGINE
    DATA
    RESOURCES
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
        message(FATAL_ERROR "Tulev1 supplies require ${required}")
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
    message(FATAL_ERROR "Tulev1 supplies output retention failed")
endif()
execute_process(
    COMMAND
        "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/pumo_tulev1_supplies_contract.py" source
        "${COURTYARD}" "${ENGINE}" "${BLAST}" "${GUIDED_PRODUCER}" "${GUIDED_GUARD}" "${QUAD}"
        "${GUARD}" "${BLUE}"
    RESULT_VARIABLE valid
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
    TIMEOUT 45)
if(NOT valid EQUAL 0)
    message(FATAL_ERROR "Tulev1 supplies registered source invalid: ${stdout}${stderr}")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/pumo_player_phase.cmake")
set(owner "${OUTPUT}")
set(source_producer "${COURTYARD}/guard305/guard305-result.json")
file(READ "${source_producer}" source_receipt)
string(JSON source_pid GET "${source_receipt}" process_id)
string(JSON source_nonce GET "${source_receipt}" run_id)
set(seen_processes "${source_pid}")
set(seen_nonces "${source_nonce}")
foreach(stage switch6 cloak113 guided140)
    foreach(kind warm cold)
        set(phase "${stage}_${kind}")
        set(OUTPUT "${owner}/${phase}")
        file(MAKE_DIRECTORY "${OUTPUT}/preview-user")
        file(READ "${source_producer}" producer_receipt)
        string(JSON history_count LENGTH "${producer_receipt}" checkpoint_history)
        if(NOT history_count EQUAL 1)
            message(FATAL_ERROR "Tulev1 supplies ${phase} requires one immediate predecessor save")
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
        file(READ "${CMAKE_CURRENT_LIST_DIR}/pumo_tulev1_supplies_${phase}.json" plan)
        string(JSON plan SET "${plan}" resume_checkpoint "\"${source_file}\"")
        string(JSON plan SET "${plan}" resume_sha256 "\"${input_hash}\"")
        file(WRITE "${OUTPUT}/${phase}-plan.json" "${plan}\n")
        execute_process(
            COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/pumo_tulev1_supplies_contract.py" bind
                    "${phase}" "${OUTPUT}/${phase}-plan.json" "${source_producer}"
            RESULT_VARIABLE bound
            OUTPUT_VARIABLE stdout
            ERROR_VARIABLE stderr
            TIMEOUT 30)
        if(NOT bound EQUAL 0)
            message(FATAL_ERROR "Tulev1 supplies ${phase} binding failed: ${stdout}${stderr}")
        endif()
        if(kind STREQUAL "warm")
            if(stage STREQUAL "switch6")
                set(expected_steps 199)
            elseif(stage STREQUAL "cloak113")
                set(expected_steps 145)
            else()
                set(expected_steps 328)
            endif()
        else()
            set(expected_steps 3)
        endif()
        run_pumo_player_phase(${phase} 7 7 ${expected_steps} 0)
        string(JSON process GET "${${phase}_receipt}" process_id)
        string(JSON nonce GET "${${phase}_receipt}" run_id)
        list(FIND seen_processes "${process}" reused_process)
        list(FIND seen_nonces "${nonce}" reused_nonce)
        if(NOT reused_process EQUAL -1 OR NOT reused_nonce EQUAL -1)
            message(FATAL_ERROR "Tulev1 supplies reused process or nonce")
        endif()
        list(APPEND seen_processes "${process}")
        list(APPEND seen_nonces "${nonce}")
        execute_process(
            COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/pumo_tulev1_supplies_contract.py" verify
                    "${phase}" "${OUTPUT}" "${ENGINE}" "${source_producer}"
            RESULT_VARIABLE valid
            OUTPUT_VARIABLE stdout
            ERROR_VARIABLE stderr
            TIMEOUT 45)
        if(NOT valid EQUAL 0)
            message(FATAL_ERROR "Tulev1 supplies ${phase} witness failed: ${stdout}${stderr}")
        endif()
        set(source_producer "${OUTPUT}/${phase}-result.json")
    endforeach()
endforeach()
