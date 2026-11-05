foreach(required ENGINE DATA RESOURCES ENTRY OUTPUT)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Pumo Sophodo player requires ${required}")
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
    message(FATAL_ERROR "Sophodo output retention/reserve check failed")
endif()
file(MAKE_DIRECTORY "${OUTPUT}/preview-user")
configure_file("${ENTRY}" "${OUTPUT}/preview-user/sophodo-entry.sav" COPYONLY)
file(SHA256 "${OUTPUT}/preview-user/sophodo-entry.sav" entry_hash)
file(READ "${CMAKE_CURRENT_LIST_DIR}/pumo_sophodo_main.json" plan)
string(JSON plan SET "${plan}" resume_checkpoint "\"sophodo-entry.sav\"")
string(JSON plan SET "${plan}" resume_sha256 "\"${entry_hash}\"")
file(WRITE "${OUTPUT}/main-plan.json" "${plan}\n")
include("${CMAKE_CURRENT_LIST_DIR}/pumo_player_phase.cmake")
run_pumo_player_phase(main 6 7 144 1)
foreach(
    index
    1
    2
    3
    4
    5
    6
    8
    10
    11
    12
    13
    14
    15
    16
    17
    18
    19
    20
    21
    22
    23
    24
    25
    26
    30
    31
    33
    36)
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
        message(FATAL_ERROR "Sophodo trigger${index} was not ordinarily operated")
    endif()
endforeach()
foreach(wall 80 81)
    string(
        JSON
        flags
        GET
        "${main_receipt}"
        transitions
        0
        last_activation
        xl_pilot
        doorways
        ${wall}
        flags)
    math(EXPR locked "${flags} & 8")
    if(locked)
        message(FATAL_ERROR "Sophodo source door${wall} remained locked after its switch")
    endif()
endforeach()
string(JSON aimed GET "${main_receipt}" fixed_view_aim_events)
if(aimed LESS 1)
    message(FATAL_ERROR "Sophodo scripted view-aim boundary was not recorded")
endif()
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
if(NOT countdown EQUAL 99 OR kills LESS 4)
    message(FATAL_ERROR "Sophodo countdown/combat evidence incomplete")
endif()
# Preserve the warm destination bytes before the cold process reuses its filename
configure_file("${OUTPUT}/preview-user/pumo-player-level7.sav"
               "${OUTPUT}/preview-user/sophodo-main-level7.sav" COPYONLY)
configure_file("${OUTPUT}/preview-user/pumo-player-level6.sav"
               "${OUTPUT}/preview-user/sophodo-reactor-earned.sav" COPYONLY)
file(SHA256 "${OUTPUT}/preview-user/sophodo-reactor-earned.sav" reactor_hash)
string(JSON expected_hash GET "${main_receipt}" checkpoints 6 sha256)
if(NOT reactor_hash STREQUAL expected_hash)
    message(FATAL_ERROR "Sophodo earned reactor checkpoint changed")
endif()
file(READ "${CMAKE_CURRENT_LIST_DIR}/pumo_sophodo_exit.json" plan)
string(JSON plan SET "${plan}" resume_checkpoint "\"sophodo-reactor-earned.sav\"")
string(JSON plan SET "${plan}" resume_sha256 "\"${reactor_hash}\"")
set(expected
    "{\"current_level_num\":6,\"position.segment\":548,\"xl_pilot.remaining_objectives\":0,\"reactor_countdown.active\":true}"
)
foreach(
    field
    shields
    energy
    robots_killed_level
    has_blue_key
    has_red_key
    has_gold_key
    cloaked
    primary_weapon_flags
    vulcan_ammo)
    string(
        JSON
        value
        GET
        "${main_receipt}"
        checkpoints
        6
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
string(
    JSON
    spawn_table
    GET
    "${main_receipt}"
    checkpoints
    6
    state
    xl_pilot
    spawn_points)
string(JSON expected SET "${expected}" "xl_pilot.spawn_count" 16)
string(JSON expected SET "${expected}" "xl_pilot.spawn_points" "${spawn_table}")
string(JSON plan SET "${plan}" checkpoint_expect "${expected}")
file(WRITE "${OUTPUT}/cold-plan.json" "${plan}\n")
run_pumo_player_phase(cold 6 7 11 1)
string(JSON main_process GET "${main_receipt}" process_id)
string(JSON cold_process GET "${cold_receipt}" process_id)
if(main_process EQUAL cold_process)
    message(FATAL_ERROR "Sophodo earned escape did not run in a fresh process")
endif()
string(JSON cold_aimed GET "${cold_receipt}" fixed_view_aim_events)
if(NOT cold_aimed EQUAL 0)
    message(FATAL_ERROR "Sophodo cold boost exit unexpectedly used scripted aiming")
endif()
