foreach(required ENGINE DATA RESOURCES OUTPUT)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Kritical Mass flight requires ${required}")
    endif()
endforeach()
find_program(PWSH pwsh REQUIRED)
execute_process(
    COMMAND
        "${PWSH}" -NoProfile -File
        "${CMAKE_CURRENT_LIST_DIR}/../../helpers/retain-recent-artifacts.ps1" -Artifacts
        "${OUTPUT}" COMMAND_ERROR_IS_FATAL ANY)
file(MAKE_DIRECTORY "${OUTPUT}/preview-user")
include("${CMAKE_CURRENT_LIST_DIR}/pumo_player_phase.cmake")
set(XL_PLAYER_MISSION kmtl-xl)
set(PUMO_PHASE_TIMEOUT 280)
set(identity
    [=[{
    "filename":"kmtl-xl",
    "descriptor_sha256":"4c37b4ddf86f7ba788aaebdabb370d104ca54f6f63dd4ce05ce24b63eff9138b",
    "hog_sha256":"e39edcd6340c56f50d584d31c7ca799005461a362eebba19d1da50d216b34022"
}]=])
set(save [=[{"action":"set_debug","field":"android_game_request","value":"quick_save"}]=])

# Each pair is the departure segment and portal side. The pilot flies every frame
function(fly)
    foreach(portal IN LISTS ARGN)
        if(NOT portal MATCHES "^([0-9]+)/([0-5])$")
            message(FATAL_ERROR "Invalid flight portal ${portal}")
        endif()
        list(
            APPEND
            steps
            "{\"action\":\"fly_through_side\",\"segment\":${CMAKE_MATCH_1},\"side\":${CMAKE_MATCH_2},\"max_frames\":700,\"fire_primary\":true}"
        )
    endforeach()
    set(steps "${steps}" PARENT_SCOPE)
endfunction()

function(run_flight name)
    list(JOIN steps "," actions)
    list(LENGTH steps count)
    set(plan
        "{\"initial_level\":1,\"simulation_fps\":60,\"mission\":${identity},\"steps\":[${actions}]}"
    )
    if(ARGC GREATER 1)
        string(JSON checkpoint GET "${${ARGV1}_receipt}" checkpoints 1)
        string(JSON file GET "${checkpoint}" file)
        string(JSON hash GET "${checkpoint}" sha256)
        string(JSON position GET "${checkpoint}" state position)
        string(JSON player GET "${checkpoint}" state player)
        string(JSON plan SET "${plan}" resume_checkpoint "\"${file}\"")
        string(JSON plan SET "${plan}" resume_sha256 "\"${hash}\"")
        string(JSON plan SET "${plan}" checkpoint_expect
               "{\"position\":${position},\"player\":${player}}")
    endif()
    file(WRITE "${OUTPUT}/${name}-plan.json" "${plan}\n")
    run_pumo_player_phase("${name}" 1 1 ${count} 0)
    if(ARGC GREATER 1)
        string(JSON prior_pid GET "${${ARGV1}_receipt}" process_id)
        string(JSON current_pid GET "${${name}_receipt}" process_id)
        if(prior_pid EQUAL current_pid)
            message(FATAL_ERROR "Kritical Mass flight did not start a fresh process")
        endif()
    endif()
    set(${name}_receipt "${${name}_receipt}" PARENT_SCOPE)
endfunction()

macro(checkpoint name)
    list(APPEND steps "${save}")
    run_flight(${name} ${previous})
    set(previous ${name})
    set(steps)
endmacro()

set(previous)
set(steps
    [=[{"action":"assert","expect":{"position.segment":286,"player.hostages_on_board":0,"cheats_enabled":false}}]=]
)
fly(286/2
    287/2
    288/1
    293/2
    294/4
    297/4
    325/4)
list(
    APPEND
    steps
    [=[{"action":"assert","expect":{"position.segment":300,"xl_pilot.wall_triggers.8.operated":true}}]=]
)
if(KMTL_RESCUE)
    checkpoint(illusion)
endif()
list(
    APPEND
    steps
    [=[{"action":"wait_ms","ms":2300}]=]
    [=[{"action":"assert","expect":{"xl_pilot.doorways.77.type":4,"xl_pilot.doorways.118.type":4}}]=]
)
if(KMTL_RESCUE)
    checkpoint(open_walls)
endif()
fly(300/4
    335/4
    336/4
    337/0
    386/0
    387/0
    388/0
    389/0
    390/5
    144/5
    157/5
    159/3
    158/3)
list(APPEND steps [=[{"action":"assert","expect":{"xl_pilot.wall_triggers.21.operated":true}}]=])
if(KMTL_RESCUE)
    checkpoint(return_door)
endif()
fly(156/3 147/4 146/4)
list(APPEND steps [=[{"action":"assert","expect":{"xl_pilot.wall_triggers.2.operated":true}}]=])
if(KMTL_RESCUE)
    checkpoint(unlock)
endif()
fly(145/4 143/4 270/4 269/4)
list(
    APPEND
    steps
    [=[{"action":"assert","expect":{"position.segment":271,"xl_pilot.wall_triggers.10.operated":false,"xl_pilot.doorways.117.flags":24,"xl_pilot.doorways.122.type":5,"player.invulnerable":false}}]=]
)
if(KMTL_RESCUE)
    fly(271/4)
    checkpoint(window)
    fly(376/5)
else()
    checkpoint(approach)
endif()

# Aim and fire through the grille, then fly through the door that the shot unlocks
list(
    APPEND
    steps
    [=[{"action":"approach_side","segment":271,"side":2,"distance":5,"max_frames":600,"look_at_side":{"segment":382,"side":0}}]=]
    [=[{"action":"send_button","button":100,"held":1}]=]
    [=[{"action":"wait_for","expect":{"xl_pilot.wall_triggers.10.operated":true,"xl_pilot.wall_triggers.10.shot":1,"xl_pilot.wall_triggers.10.actor":0},"timeout_ms":3000}]=]
    [=[{"action":"send_button","button":100,"held":0}]=])
if(KMTL_RESCUE)
    checkpoint(grille)
endif()
fly(271/5 269/0 266/0 262/4 264/4 385/4)
list(
    APPEND
    steps
    [=[{"action":"wait_ms","ms":1800}]=]
    [=[{"action":"assert","expect":{"position.segment":384,"xl_pilot.wall_triggers.11.operated":true,"xl_pilot.wall_triggers.11.shot":0,"xl_pilot.doorways.122.type":4,"player.invulnerable":false}}]=]
)
checkpoint(door)

if(KMTL_RESCUE)
    # Shoot the second switch through the floor grille to deactivate the force fields
    fly(384/4
        382/2
        383/4
        406/2
        407/2
        395/1
        426/1
        466/4
        467/4
        636/0
        635/0
        634/0
        633/0
        632/0)
    list(
        APPEND
        steps
        [=[{"action":"approach_side","segment":244,"side":3,"distance":6,"max_frames":600,"look_at_side":{"segment":476,"side":5}}]=]
        [=[{"action":"send_button","button":100,"held":1}]=]
        [=[{"action":"wait_for","expect":{"xl_pilot.wall_triggers.15.operated":true,"xl_pilot.wall_triggers.15.shot":1,"xl_pilot.wall_triggers.15.actor":0},"timeout_ms":3000}]=]
        [=[{"action":"send_button","button":100,"held":0}]=]
        [=[{"action":"wait_ms","ms":1700}]=])
    checkpoint(force_fields)
    fly(244/2
        632/2
        633/2
        634/2
        635/2
        636/2
        467/5
        466/3
        426/3
        395/4
        407/0
        472/3
        473/0)
    list(APPEND steps
         [=[{"action":"wait_for","expect":{"position.segment":484},"timeout_ms":1200}]=])
    fly(484/0)
    list(
        APPEND
        steps
        [=[{"action":"wait_ms","ms":1700}]=]
        [=[{"action":"assert","expect":{"xl_pilot.wall_triggers.16.operated":true,"player_dead":false}}]=]
    )
    checkpoint(prison_entry)
    fly(234/2 239/2 477/5 481/5 453/5)
    list(APPEND steps
         [=[{"action":"assert","expect":{"position.segment":458,"player.hostages_on_board":0}}]=])
    checkpoint(hostage_room)
    list(
        APPEND
        steps
        [=[{"action":"collect_pickup","kind":"hostage","max_frames":700}]=]
        [=[{"action":"assert","expect":{"player.hostages_on_board":1,"player_dead":false}}]=]
        [=[{"action":"collect_pickup","kind":"hostage","max_frames":700}]=]
        [=[{"action":"assert","expect":{"player.hostages_on_board":2,"player_dead":false}}]=])
    checkpoint(rescue)
    list(
        APPEND
        steps
        [=[{"action":"assert","expect":{"position.segment":458,"player.hostages_on_board":2,"xl_pilot.wall_triggers.16.operated":true}}]=]
    )
    fly(458/0 459/0 457/0 460/0 456/5)
    list(
        APPEND
        steps
        [=[{"action":"assert","expect":{"position.segment":461,"player.hostages_on_board":2,"player.lives":3,"player.invulnerable":false}}]=]
    )
    checkpoint(continued)
endif()
