foreach(required ENGINE DATA RESOURCES ENTRY OUTPUT)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Pumo Metillis player requires ${required}")
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
    message(FATAL_ERROR "Metillis output retention/reserve check failed")
endif()
file(MAKE_DIRECTORY "${OUTPUT}/preview-user")
configure_file("${ENTRY}" "${OUTPUT}/preview-user/metillis-entry.sav" COPYONLY)
file(SHA256 "${OUTPUT}/preview-user/metillis-entry.sav" entry_hash)
file(READ "${CMAKE_CURRENT_LIST_DIR}/pumo_metillis_main.json" plan)
string(JSON plan SET "${plan}" resume_checkpoint "\"metillis-entry.sav\"")
string(JSON plan SET "${plan}" resume_sha256 "\"${entry_hash}\"")
file(WRITE "${OUTPUT}/main-plan.json" "${plan}\n")

function(run_metillis name initial destination step_count transition_count)
    execute_process(
        COMMAND
            "${CMAKE_COMMAND}" -E env --unset=SDL_VIDEODRIVER SDL_AUDIODRIVER=dummy "${ENGINE}"
            -pumo-player-test "${OUTPUT}/${name}-plan.json" -pumo-resource-root "${RESOURCES}"
            -hogdir "${DATA}" -extra-dir "${OUTPUT}" -userdir "${OUTPUT}/preview-user" -nosound
            -nomusic
        RESULT_VARIABLE exit_code
        OUTPUT_VARIABLE stdout
        ERROR_VARIABLE stderr
        TIMEOUT 150)
    file(WRITE "${OUTPUT}/${name}-controls.log" "${stdout}${stderr}")
    if(NOT exit_code EQUAL 0)
        message(FATAL_ERROR "Metillis ${name} failed (${exit_code}): ${stdout}${stderr}")
    endif()
    file(READ "${OUTPUT}/player-result.json" receipt)
    string(JSON status GET "${receipt}" result)
    string(JSON steps GET "${receipt}" steps)
    string(JSON final GET "${receipt}" final_level)
    string(JSON resumed GET "${receipt}" resumed_earned_checkpoint)
    string(JSON count LENGTH "${receipt}" transitions)
    if(NOT status STREQUAL "PASS"
       OR NOT steps EQUAL step_count
       OR NOT final EQUAL destination
       OR NOT resumed
       OR NOT count EQUAL transition_count)
        message(FATAL_ERROR "Metillis ${name} progression evidence incomplete")
    endif()
    if(transition_count)
        string(
            JSON
            source
            GET
            "${receipt}"
            transitions
            0
            from)
        string(
            JSON
            target
            GET
            "${receipt}"
            transitions
            0
            to)
        if(NOT source EQUAL initial OR NOT target EQUAL destination)
            message(FATAL_ERROR "Metillis ${name} transition mismatch")
        endif()
    endif()
    foreach(member result state)
        configure_file("${OUTPUT}/player-${member}.json" "${OUTPUT}/${name}-${member}.json"
                       COPYONLY)
    endforeach()
    configure_file("${OUTPUT}/player-trace.jsonl" "${OUTPUT}/${name}-trace.jsonl" COPYONLY)
    set(${name}_receipt "${receipt}" PARENT_SCOPE)
endfunction()

run_metillis(main 4 5 85 1)
# Require the complete automatic corridor, master and 99-second authored countdown
foreach(
    index
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
    27
    28
    34
    35
    37
    39
    41
    42)
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
        message(FATAL_ERROR "Metillis trigger${index} was not ordinarily operated")
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
if(NOT countdown EQUAL 99)
    message(FATAL_ERROR "Metillis authored countdown was not activated")
endif()
configure_file("${OUTPUT}/preview-user/pumo-player-level4.sav"
               "${OUTPUT}/preview-user/metillis-reactor-earned.sav" COPYONLY)
file(SHA256 "${OUTPUT}/preview-user/metillis-reactor-earned.sav" reactor_hash)
string(JSON expected_hash GET "${main_receipt}" checkpoints 4 sha256)
if(NOT reactor_hash STREQUAL expected_hash)
    message(FATAL_ERROR "Metillis earned reactor checkpoint changed")
endif()
file(READ "${CMAKE_CURRENT_LIST_DIR}/pumo_metillis_alternate.json" plan)
string(JSON plan SET "${plan}" resume_checkpoint "\"metillis-reactor-earned.sav\"")
string(JSON plan SET "${plan}" resume_sha256 "\"${reactor_hash}\"")
set(expected
    "{\"current_level_num\":4,\"position.segment\":345,\"xl_pilot.remaining_objectives\":0,\"reactor_countdown.active\":true}"
)
foreach(field shields energy robots_killed_level has_blue_key has_red_key has_gold_key)
    string(
        JSON
        value
        GET
        "${main_receipt}"
        checkpoints
        4
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
file(WRITE "${OUTPUT}/alternate-plan.json" "${plan}\n")
run_metillis(alternate 4 14 14 1)
string(JSON main_process GET "${main_receipt}" process_id)
string(JSON alternate_process GET "${alternate_receipt}" process_id)
if(main_process EQUAL alternate_process)
    message(FATAL_ERROR "Metillis alternate did not run in a fresh process")
endif()
foreach(index 39 41 42)
    string(
        JSON
        operated
        GET
        "${alternate_receipt}"
        transitions
        0
        last_activation
        xl_pilot
        wall_triggers
        ${index}
        operated)
    if(NOT operated)
        message(FATAL_ERROR "Metillis alternate master dependency${index} did not operate")
    endif()
endforeach()

# The authored Fusion pickup opens Coldrom's reactor chamber. Selection must use ordinary ownership
# checks, including rejection of an unowned weapon
configure_file("${OUTPUT}/preview-user/pumo-player-level14.sav"
               "${OUTPUT}/preview-user/coldrom-entry.sav" COPYONLY)
file(SHA256 "${OUTPUT}/preview-user/coldrom-entry.sav" coldrom_hash)
file(READ "${CMAKE_CURRENT_LIST_DIR}/pumo_coldrom_pickup.json" plan)
string(JSON plan SET "${plan}" resume_checkpoint "\"coldrom-entry.sav\"")
string(JSON plan SET "${plan}" resume_sha256 "\"${coldrom_hash}\"")
string(
    JSON
    plan
    SET
    "${plan}"
    checkpoint_expect
    "{\"current_level_num\":14,\"position.segment\":228,\"player.shields\":100,\"player.energy\":100}"
)
file(WRITE "${OUTPUT}/coldrom-plan.json" "${plan}\n")
run_metillis(coldrom 14 14 15 0)
string(JSON coldrom_process GET "${coldrom_receipt}" process_id)
if(coldrom_process EQUAL alternate_process OR coldrom_process EQUAL main_process)
    message(FATAL_ERROR "Coldrom ownership/save checks did not use a fresh process")
endif()
