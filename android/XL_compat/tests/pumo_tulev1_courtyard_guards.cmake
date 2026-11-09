# Two original courtyard guards, each followed by a zero-frame cold load.
foreach(
    required
    ENGINE
    DATA
    RESOURCES
    PRODUCER
    GUIDED_PRODUCER
    GUIDED_GUARD
    QUAD
    GUARD
    BLUE
    OUTPUT
    PYTHON)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Tulev1 courtyard guards require ${required}")
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
    message(FATAL_ERROR "Tulev1 courtyard output retention failed")
endif()
execute_process(
    COMMAND
        "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/pumo_tulev1_courtyard_guards_contract.py" source
        "${PRODUCER}" "${ENGINE}" "${GUIDED_PRODUCER}" "${GUIDED_GUARD}" "${QUAD}" "${GUARD}"
        "${BLUE}"
    RESULT_VARIABLE valid
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
    TIMEOUT 35)
if(NOT valid EQUAL 0)
    message(FATAL_ERROR "Tulev1 courtyard source invalid: ${stdout}${stderr}")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/pumo_player_phase.cmake")
set(owner "${OUTPUT}")
set(source_producer "${PRODUCER}")
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
string(JSON source_pid GET "${producer_receipt}" process_id)
string(JSON source_nonce GET "${producer_receipt}" run_id)
set(seen_processes "${source_pid}")
set(seen_nonces "${source_nonce}")
foreach(phase guard256 guard256_cold guard305 guard305_cold)
    set(OUTPUT "${owner}/${phase}")
    file(MAKE_DIRECTORY "${OUTPUT}/preview-user")
    configure_file("${earned_save}" "${OUTPUT}/preview-user/${source_file}" COPYONLY)
    file(SHA256 "${OUTPUT}/preview-user/${source_file}" input_hash)
    if(phase MATCHES "^guard256")
        set(actor 256)
    else()
        set(actor 305)
    endif()
    if(phase MATCHES "_cold$")
        set(kind cold)
        set(expected_steps 1)
    else()
        set(kind warm)
        if(actor EQUAL 256)
            set(expected_steps 45)
        else()
            set(expected_steps 20)
        endif()
    endif()
    file(READ "${CMAKE_CURRENT_LIST_DIR}/pumo_tulev1_courtyard_guard${actor}_${kind}.json" plan)
    string(JSON plan SET "${plan}" resume_checkpoint "\"${source_file}\"")
    string(JSON plan SET "${plan}" resume_sha256 "\"${input_hash}\"")
    file(WRITE "${OUTPUT}/${phase}-plan.json" "${plan}\n")
    if(kind STREQUAL "warm")
        execute_process(
            COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/pumo_tulev1_courtyard_guards_contract.py"
                    bind "${actor}" "${OUTPUT}/${phase}-plan.json" "${source_producer}"
            RESULT_VARIABLE bound
            OUTPUT_VARIABLE stdout
            ERROR_VARIABLE stderr
            TIMEOUT 25)
        if(NOT bound EQUAL 0)
            message(FATAL_ERROR "Tulev1 courtyard ${phase} binding failed: ${stdout}${stderr}")
        endif()
    endif()
    run_pumo_player_phase(${phase} 7 7 ${expected_steps} 0)
    string(JSON process GET "${${phase}_receipt}" process_id)
    string(JSON nonce GET "${${phase}_receipt}" run_id)
    list(FIND seen_processes "${process}" reused_process)
    list(FIND seen_nonces "${nonce}" reused_nonce)
    if(NOT reused_process EQUAL -1 OR NOT reused_nonce EQUAL -1)
        message(FATAL_ERROR "Tulev1 courtyard reused process or nonce")
    endif()
    list(APPEND seen_processes "${process}")
    list(APPEND seen_nonces "${nonce}")
    execute_process(
        COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/pumo_tulev1_courtyard_guards_contract.py"
                verify "${phase}" "${OUTPUT}" "${ENGINE}" "${source_producer}"
        RESULT_VARIABLE valid
        OUTPUT_VARIABLE stdout
        ERROR_VARIABLE stderr
        TIMEOUT 35)
    if(NOT valid EQUAL 0)
        message(FATAL_ERROR "Tulev1 courtyard ${phase} witness failed: ${stdout}${stderr}")
    endif()
    if(kind STREQUAL "warm")
        string(JSON history_count LENGTH "${${phase}_receipt}" checkpoint_history)
        if(NOT history_count EQUAL 1)
            message(FATAL_ERROR "Tulev1 courtyard requires one settled versioned save")
        endif()
        string(
            JSON
            source_file
            GET
            "${${phase}_receipt}"
            checkpoint_history
            0
            file)
        string(JSON saved_hash GET "${${phase}_receipt}" checkpoint_history 0 sha256)
        set(earned_save "${OUTPUT}/preview-user/${source_file}")
        file(SHA256 "${earned_save}" actual_hash)
        if(NOT actual_hash STREQUAL saved_hash)
            message(FATAL_ERROR "Tulev1 courtyard settled save changed")
        endif()
        set(source_producer "${OUTPUT}/${phase}-result.json")
    endif()
endforeach()
