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
        message(FATAL_ERROR "Pumo Coldrom player requires ${required}")
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
    message(FATAL_ERROR "Coldrom output retention/reserve check failed")
endif()
execute_process(
    COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/pumo_coldrom_contract.py" --phase producer
            --producer "${PRODUCER}" --entry "${ENTRY}" --engine "${ENGINE}"
    RESULT_VARIABLE producer_result
    OUTPUT_VARIABLE producer_stdout
    ERROR_VARIABLE producer_stderr
    TIMEOUT 20)
if(NOT producer_result EQUAL 0)
    message(
        FATAL_ERROR "Coldrom earned entry producer invalid: ${producer_stdout}${producer_stderr}")
endif()

execute_process(
    COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/pumo_coldrom_contract.py" --phase templates
            --templates "${CMAKE_CURRENT_LIST_DIR}"
    RESULT_VARIABLE template_result
    OUTPUT_VARIABLE template_stdout
    ERROR_VARIABLE template_stderr
    TIMEOUT 20)
if(NOT template_result EQUAL 0)
    message(
        FATAL_ERROR "Coldrom bounded plan template invalid: ${template_stdout}${template_stderr}")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/pumo_player_phase.cmake")
set(owner_output "${OUTPUT}")
file(MAKE_DIRECTORY "${owner_output}")
file(WRITE "${owner_output}/engine-path.txt" "${ENGINE}\n")
function(coldrom_prepare name source save)
    file(MAKE_DIRECTORY "${OUTPUT}/preview-user")
    set(save_name "coldrom-${name}-earned-entry.sav")
    configure_file("${save}" "${OUTPUT}/preview-user/${save_name}" COPYONLY)
    file(SHA256 "${OUTPUT}/preview-user/${save_name}" resume_hash)
    file(READ "${CMAKE_CURRENT_LIST_DIR}/pumo_coldrom_${source}.json" plan)
    string(JSON plan SET "${plan}" resume_checkpoint "\"${save_name}\"")
    string(JSON plan SET "${plan}" resume_sha256 "\"${resume_hash}\"")
    file(WRITE "${OUTPUT}/${name}-plan.json" "${plan}\n")
endfunction()
function(coldrom_witness phase)
    execute_process(
        COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/pumo_coldrom_contract.py" --phase "${phase}"
                --owner "${owner_output}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE stdout
        ERROR_VARIABLE stderr
        TIMEOUT 20)
    file(WRITE "${owner_output}/${phase}-witness.txt" "${stdout}${stderr}")
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Coldrom ${phase} witness failed: ${stdout}${stderr}")
    endif()
endfunction()

set(OUTPUT "${owner_output}/fusion")
coldrom_prepare(fusion fusion "${ENTRY}")
run_pumo_player_phase(fusion 14 14 14 0)
coldrom_witness(fusion)

set(OUTPUT "${owner_output}/main")
coldrom_prepare(main main "${ENTRY}")
run_pumo_player_phase(main 14 5 55 1)
coldrom_witness(main)

# The versioned reactor save belongs to the one continuous main process. Never use the last level-14
# alias after a later save or an exploration checkpoint.
string(
    JSON
    reactor_step
    GET
    "${main_receipt}"
    checkpoint_history
    2
    step)
string(
    JSON
    reactor_file
    GET
    "${main_receipt}"
    checkpoint_history
    2
    file)
string(JSON reactor_hash GET "${main_receipt}" checkpoint_history 2 sha256)
string(
    JSON
    warm_step
    GET
    "${main_receipt}"
    checkpoint_history
    3
    step)
string(
    JSON
    warm_file
    GET
    "${main_receipt}"
    checkpoint_history
    3
    file)
string(JSON warm_hash GET "${main_receipt}" checkpoint_history 3 sha256)
if(NOT reactor_step EQUAL 36 OR NOT warm_step EQUAL 53)
    message(FATAL_ERROR "Coldrom main checkpoint producer steps changed")
endif()
set(reactor_save "${owner_output}/main/preview-user/${reactor_file}")
set(warm_save "${owner_output}/main/preview-user/${warm_file}")
file(SHA256 "${reactor_save}" actual_reactor_hash)
file(SHA256 "${warm_save}" actual_warm_hash)
if(NOT actual_reactor_hash STREQUAL reactor_hash OR NOT actual_warm_hash STREQUAL warm_hash)
    message(FATAL_ERROR "Coldrom main versioned checkpoint changed")
endif()

# Enforce save/load preservation before either continuation can perform an action. The saved state
# is taken from this main receipt, never from exploration constants.
function(coldrom_require_preserved_entry name index)
    file(READ "${OUTPUT}/${name}-plan.json" plan)
    string(JSON expected GET "${plan}" checkpoint_expect)
    foreach(
        path
        player.shields
        player.energy
        player.primary_weapon
        player.primary_weapon_flags
        player.vulcan_ammo
        player.has_blue_key
        player.has_red_key
        player.has_gold_key
        player.cloaked
        player.invulnerable
        player.lives
        player.score
        player.robots_killed_level
        reactor_countdown.active
        reactor_countdown.total_seconds
        reactor_countdown.timer
        simulation_rng.state_available
        simulation_rng.state)
        string(REPLACE "." ";" selectors "${path}")
        string(
            JSON
            kind
            TYPE
            "${main_receipt}"
            checkpoint_history
            ${index}
            state
            ${selectors})
        string(
            JSON
            value
            GET
            "${main_receipt}"
            checkpoint_history
            ${index}
            state
            ${selectors})
        if(kind STREQUAL "BOOLEAN")
            if(value)
                set(value true)
            else()
                set(value false)
            endif()
        elseif(NOT kind STREQUAL "NUMBER")
            message(FATAL_ERROR "Coldrom saved field ${path} was not scalar")
        endif()
        string(JSON expected SET "${expected}" "${path}" "${value}")
    endforeach()
    string(JSON plan SET "${plan}" checkpoint_expect "${expected}")
    file(WRITE "${OUTPUT}/${name}-plan.json" "${plan}\n")
endfunction()

set(OUTPUT "${owner_output}/cold")
coldrom_prepare(cold cold "${reactor_save}")
coldrom_require_preserved_entry(cold 2)
run_pumo_player_phase(cold 14 5 17 1)
coldrom_witness(cold)

set(OUTPUT "${owner_output}/gold")
coldrom_prepare(gold gold "${warm_save}")
coldrom_require_preserved_entry(gold 3)
run_pumo_player_phase(gold 5 5 8 0)
coldrom_witness(gold)

set(seen_run_ids)
foreach(phase fusion main cold gold)
    string(JSON native_run_id GET "${${phase}_receipt}" run_id)
    list(FIND seen_run_ids "${native_run_id}" duplicate_run_id)
    if(NOT duplicate_run_id EQUAL -1)
        message(FATAL_ERROR "Coldrom ${phase} did not use a distinct native invocation")
    endif()
    list(APPEND seen_run_ids "${native_run_id}")
endforeach()
message(STATUS "Pumo earned Coldrom and bounded Iquaxel branch gates passed")
