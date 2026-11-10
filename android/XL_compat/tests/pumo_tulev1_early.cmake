# Bounded, assisted early Tulev1 qualification from a freshly earned Sophodo warm save.
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
        message(FATAL_ERROR "Tulev1 early player requires ${required}")
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
    message(FATAL_ERROR "Tulev1 early output retention/reserve check failed")
endif()
set(owner "${OUTPUT}")
execute_process(
    COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/pumo_tulev1_early_contract.py" producer
            "${PRODUCER}" "${ENTRY}" "${ENGINE}"
    RESULT_VARIABLE valid
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
    TIMEOUT 20)
if(NOT valid EQUAL 0)
    message(FATAL_ERROR "Tulev1 early Sophodo producer invalid: ${stdout}${stderr}")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/pumo_player_phase.cmake")
set(earned_save "${ENTRY}")
set(previous_process "")
foreach(
    phase
    switch
    combat
    cloak
    red_key
    red_door
    master
    guided_inventory)
    set(OUTPUT "${owner}/${phase}")
    file(MAKE_DIRECTORY "${OUTPUT}/preview-user")
    set(input_name "tulev1-${phase}-earned-input.sav")
    configure_file("${earned_save}" "${OUTPUT}/preview-user/${input_name}" COPYONLY)
    file(SHA256 "${OUTPUT}/preview-user/${input_name}" input_hash)
    file(READ "${CMAKE_CURRENT_LIST_DIR}/pumo_tulev1_early_${phase}.json" plan)
    string(JSON plan SET "${plan}" resume_checkpoint "\"${input_name}\"")
    string(JSON plan SET "${plan}" resume_sha256 "\"${input_hash}\"")
    file(WRITE "${OUTPUT}/${phase}-plan.json" "${plan}\n")
    if(phase STREQUAL "cloak")
        set(expected_steps 7)
    elseif(phase STREQUAL "master")
        set(expected_steps 12)
    else()
        set(expected_steps 8)
    endif()
    run_pumo_player_phase(${phase} 7 7 ${expected_steps} 0)
    string(JSON process GET "${${phase}_receipt}" process_id)
    if("${process}" STREQUAL "${previous_process}")
        message(FATAL_ERROR "Tulev1 early phases did not use separate processes")
    endif()
    set(previous_process "${process}")
    execute_process(
        COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/pumo_tulev1_early_contract.py" phase
                "${phase}" "${OUTPUT}" "${ENGINE}" "${PRODUCER}"
        RESULT_VARIABLE valid
        OUTPUT_VARIABLE stdout
        ERROR_VARIABLE stderr
        TIMEOUT 20)
    if(NOT valid EQUAL 0)
        message(FATAL_ERROR "Tulev1 early ${phase} witness failed: ${stdout}${stderr}")
    endif()
    string(
        JSON
        history_file
        GET
        "${${phase}_receipt}"
        checkpoint_history
        0
        file)
    string(JSON history_hash GET "${${phase}_receipt}" checkpoint_history 0 sha256)
    set(earned_save "${OUTPUT}/preview-user/${history_file}")
    file(SHA256 "${earned_save}" actual_hash)
    if(NOT actual_hash STREQUAL history_hash)
        message(FATAL_ERROR "Tulev1 early ${phase} versioned save changed")
    endif()
endforeach()

set(OUTPUT "${owner}/guided_load")
file(MAKE_DIRECTORY "${OUTPUT}/preview-user")
set(input_name "tulev1-guided-load-earned-input.sav")
configure_file("${earned_save}" "${OUTPUT}/preview-user/${input_name}" COPYONLY)
file(SHA256 "${OUTPUT}/preview-user/${input_name}" input_hash)
file(READ "${CMAKE_CURRENT_LIST_DIR}/pumo_tulev1_early_guided_load.json" plan)
string(JSON plan SET "${plan}" resume_checkpoint "\"${input_name}\"")
string(JSON plan SET "${plan}" resume_sha256 "\"${input_hash}\"")
file(WRITE "${OUTPUT}/guided_load-plan.json" "${plan}\n")
run_pumo_player_phase(guided_load 7 7 1 0)
string(JSON process GET "${guided_load_receipt}" process_id)
if("${process}" STREQUAL "${previous_process}")
    message(FATAL_ERROR "Tulev1 guided load did not use a fresh process")
endif()
execute_process(
    COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/pumo_tulev1_early_contract.py" load "${OUTPUT}"
            "${ENGINE}" "${owner}/guided_inventory"
    RESULT_VARIABLE valid
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
    TIMEOUT 20)
if(NOT valid EQUAL 0)
    message(FATAL_ERROR "Tulev1 guided inventory restored-load witness failed: ${stdout}${stderr}")
endif()
