# Independent ordinary Guided input successor to the versioned early inventory save.
foreach(required ENGINE DATA RESOURCES PRODUCER OUTPUT PYTHON)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Guided input requires ${required}")
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
    message(FATAL_ERROR "Guided input output retention failed")
endif()
set(owner "${OUTPUT}")
execute_process(
    COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/pumo_guided_input_contract.py" source
            "${PRODUCER}" "${ENGINE}"
    RESULT_VARIABLE valid
    OUTPUT_VARIABLE source_clock
    ERROR_VARIABLE stderr
    TIMEOUT 20)
if(NOT valid EQUAL 0 OR NOT source_clock MATCHES "^[0-9]+[\r\n]*$")
    message(FATAL_ERROR "Guided input inventory source invalid: ${source_clock}${stderr}")
endif()
string(STRIP "${source_clock}" source_clock)
math(EXPR rearm_clock "${source_clock} + 65536")
file(READ "${PRODUCER}" source_receipt)
string(
    JSON
    source_save
    GET
    "${source_receipt}"
    checkpoint_history
    0
    file)
string(JSON source_sha GET "${source_receipt}" checkpoint_history 0 sha256)
get_filename_component(source_root "${PRODUCER}" DIRECTORY)
set(source_path "${source_root}/preview-user/${source_save}")
include("${CMAKE_CURRENT_LIST_DIR}/pumo_player_phase.cmake")
set(previous_process "")
foreach(phase IN ITEMS neutral yaw)
    set(OUTPUT "${owner}/${phase}")
    file(MAKE_DIRECTORY "${OUTPUT}/preview-user")
    set(input_name "tulev1-guided_inventory-level7.sav")
    configure_file("${source_path}" "${OUTPUT}/preview-user/${input_name}" COPYONLY)
    file(SHA256 "${OUTPUT}/preview-user/${input_name}" input_hash)
    if(NOT input_hash STREQUAL source_sha)
        message(FATAL_ERROR "Guided input copied source save changed")
    endif()
    file(READ "${CMAKE_CURRENT_LIST_DIR}/pumo_guided_input_${phase}.json" plan)
    string(JSON plan SET "${plan}" resume_checkpoint "\"${input_name}\"")
    string(JSON plan SET "${plan}" resume_sha256 "\"${input_hash}\"")
    string(
        JSON
        plan
        SET
        "${plan}"
        steps
        5
        expect
        game_time_raw
        ${rearm_clock})
    file(WRITE "${OUTPUT}/${phase}-plan.json" "${plan}\n")
    run_pumo_player_phase(${phase} 7 7 24 0)
    string(JSON process GET "${${phase}_receipt}" process_id)
    if("${process}" STREQUAL "${previous_process}")
        message(FATAL_ERROR "Guided input pair reused a process")
    endif()
    set(previous_process "${process}")
endforeach()
execute_process(
    COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/pumo_guided_input_contract.py" pair
            "${owner}/neutral" "${owner}/yaw" "${PRODUCER}" "${ENGINE}"
    RESULT_VARIABLE valid
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
    TIMEOUT 20)
if(NOT valid EQUAL 0)
    message(FATAL_ERROR "Guided input pair invalid: ${stdout}${stderr}")
endif()
