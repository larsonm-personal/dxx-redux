foreach(required ENGINE DATA RESOURCES ENTRY OUTPUT)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Pumo Iquaxel player requires ${required}")
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
    message(FATAL_ERROR "Iquaxel output retention/reserve check failed")
endif()
file(MAKE_DIRECTORY "${OUTPUT}/preview-user")
configure_file("${ENTRY}" "${OUTPUT}/preview-user/iquaxel-entry.sav" COPYONLY)
file(SHA256 "${OUTPUT}/preview-user/iquaxel-entry.sav" entry_hash)
file(READ "${CMAKE_CURRENT_LIST_DIR}/pumo_iquaxel_main.json" plan)
string(JSON plan SET "${plan}" resume_checkpoint "\"iquaxel-entry.sav\"")
string(JSON plan SET "${plan}" resume_sha256 "\"${entry_hash}\"")
file(WRITE "${OUTPUT}/main-plan.json" "${plan}\n")
include("${CMAKE_CURRENT_LIST_DIR}/pumo_player_phase.cmake")
run_pumo_player_phase(main 5 6 75 1)
foreach(
    index
    4
    7
    13
    17
    18
    20
    25
    26
    27
    28
    29
    30
    31
    32
    33
    34
    35
    36
    37
    38
    39
    40
    42
    43
    44)
    string(
        JSON
        operated
        GET
        "${main_receipt}"
        transitions
        0
        last_activation
        xl_pilot
        wall_triggers
        ${index}
        operated)
    if(NOT operated)
        message(FATAL_ERROR "Iquaxel trigger${index} was not ordinarily operated")
    endif()
endforeach()
foreach(index 7 13 17 18 20)
    string(
        JSON
        shot
        GET
        "${main_receipt}"
        transitions
        0
        last_activation
        xl_pilot
        wall_triggers
        ${index}
        shot)
    if((index EQUAL 7 AND NOT shot EQUAL 1) OR (NOT index EQUAL 7 AND NOT shot EQUAL 0))
        message(FATAL_ERROR "Iquaxel trigger${index} activation mode differs from original route")
    endif()
endforeach()
string(
    JSON
    countdown
    GET
    "${main_receipt}"
    transitions
    0
    last_activation
    reactor_countdown
    total_seconds)
string(
    JSON
    kills
    GET
    "${main_receipt}"
    transitions
    0
    last_activation
    player
    robots_killed_level)
if(NOT countdown EQUAL 99 OR kills LESS 1)
    message(FATAL_ERROR "Iquaxel countdown/combat evidence incomplete")
endif()
# Preserve the warm destination bytes before the cold process reuses its filename
configure_file("${OUTPUT}/preview-user/pumo-player-level6.sav"
               "${OUTPUT}/preview-user/iquaxel-main-level6.sav" COPYONLY)
configure_file("${OUTPUT}/preview-user/pumo-player-level5.sav"
               "${OUTPUT}/preview-user/iquaxel-reactor-earned.sav" COPYONLY)
file(SHA256 "${OUTPUT}/preview-user/iquaxel-reactor-earned.sav" reactor_hash)
string(JSON expected_hash GET "${main_receipt}" checkpoints 5 sha256)
if(NOT reactor_hash STREQUAL expected_hash)
    message(FATAL_ERROR "Iquaxel earned reactor checkpoint changed")
endif()
file(READ "${CMAKE_CURRENT_LIST_DIR}/pumo_iquaxel_exit.json" plan)
string(JSON plan SET "${plan}" resume_checkpoint "\"iquaxel-reactor-earned.sav\"")
string(JSON plan SET "${plan}" resume_sha256 "\"${reactor_hash}\"")
set(expected
    "{\"current_level_num\":5,\"position.segment\":486,\"xl_pilot.remaining_objectives\":0,\"reactor_countdown.active\":true}"
)
foreach(field shields energy robots_killed_level has_blue_key has_red_key has_gold_key)
    string(
        JSON
        value
        GET
        "${main_receipt}"
        checkpoints
        5
        state
        player
        ${field})
    if(value STREQUAL "ON")
        set(value true)
    elseif(value STREQUAL "OFF")
        set(value false)
    endif()
    string(JSON expected SET "${expected}" "player.${field}" "${value}")
endforeach()
string(JSON plan SET "${plan}" checkpoint_expect "${expected}")
file(WRITE "${OUTPUT}/cold-plan.json" "${plan}\n")
run_pumo_player_phase(cold 5 6 10 1)
string(JSON main_process GET "${main_receipt}" process_id)
string(JSON cold_process GET "${cold_receipt}" process_id)
if(main_process EQUAL cold_process)
    message(FATAL_ERROR "Iquaxel earned escape did not run in a fresh process")
endif()
