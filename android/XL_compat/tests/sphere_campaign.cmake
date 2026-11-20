find_program(PWSH pwsh REQUIRED)
execute_process(
    COMMAND
        "${PWSH}" -NoProfile -File
        "${CMAKE_CURRENT_LIST_DIR}/../../helpers/retain-recent-artifacts.ps1" -Artifacts
        "${OUTPUT}" COMMAND_ERROR_IS_FATAL ANY)
file(MAKE_DIRECTORY "${OUTPUT}/preview-user")
include("${CMAKE_CURRENT_LIST_DIR}/pumo_player_phase.cmake")
set(XL_PLAYER_MISSION sphere)
file(READ "${CMAKE_CURRENT_LIST_DIR}/sphere_red_key.json" base)
if(STAGE STREQUAL "shaft")
    set(initial 1)
    set(entry_receipt flight-result.json)
elseif(STAGE STREQUAL "storage" OR STAGE STREQUAL "carrier_supply")
    set(initial 2)
    set(entry_receipt shaft-3-result.json)
elseif(STAGE STREQUAL "carrier_key")
    set(initial 2)
    set(entry_receipt carrier_supply-17-result.json)
elseif(STAGE STREQUAL "morph" OR STAGE STREQUAL "gauss_supply")
    set(initial 2)
    set(entry_receipt carrier_key-15-result.json)
elseif(STAGE STREQUAL "red_resupply")
    set(initial 2)
    set(entry_receipt gauss_supply-22-result.json)
elseif(STAGE STREQUAL "red_carrier")
    set(initial 2)
    set(entry_receipt red_resupply-16-result.json)
elseif(STAGE STREQUAL "red_recovery")
    set(initial 2)
    set(entry_receipt red_carrier-14-result.json)
elseif(STAGE STREQUAL "living_escape")
    set(initial 2)
    set(entry_receipt earned_reactor-4-result.json)
elseif(STAGE STREQUAL "earned_reactor")
    set(initial 2)
    set(entry_receipt respawn_extra_life-19-result.json)
elseif(STAGE STREQUAL "level3_approach")
    set(initial 3)
    set(entry_receipt level3_supply-21-result.json)
elseif(STAGE STREQUAL "level3_blue")
    set(initial 3)
    set(entry_receipt level3_approach-17-result.json)
elseif(STAGE STREQUAL "level3_supply")
    set(initial 3)
    set(ENTRY "${OUTPUT}")
    configure_file("${LEVEL3_SAVE}" "${OUTPUT}/preview-user/sphere-level3.sav" COPYONLY)
    configure_file("${CMAKE_CURRENT_LIST_DIR}/sphere_level3_import.json"
                   "${OUTPUT}/level3_import-plan.json" COPYONLY)
    run_pumo_player_phase(level3_import 3 3 2 0)
    set(entry_receipt level3_import-result.json)
elseif(STAGE STREQUAL "respawn_extra_life")
    set(initial 2)
    set(ENTRY "${OUTPUT}")
    # This plan pins the ordinary Android DGSS and checks the pilot's earned state on load.
    configure_file("${RESPAWN_SAVE}" "${OUTPUT}/preview-user/sphere-respawn.sav" COPYONLY)
    configure_file("${CMAKE_CURRENT_LIST_DIR}/sphere_respawn_import.json"
                   "${OUTPUT}/respawn_import-plan.json" COPYONLY)
    run_pumo_player_phase(respawn_import 2 2 2 0)
    set(entry_receipt respawn_import-result.json)
elseif(STAGE STREQUAL "cloak")
    set(initial 2)
    set(entry_receipt storage-1-result.json)
else()
    message(FATAL_ERROR "Unknown Sphere campaign stage ${STAGE}")
endif()
string(JSON base SET "${base}" initial_level ${initial})
file(READ "${CMAKE_CURRENT_LIST_DIR}/sphere_${STAGE}.json" route)
file(READ "${ENTRY}/${entry_receipt}" entry)
string(JSON checkpoint_count LENGTH "${entry}" checkpoint_history)
math(EXPR latest "${checkpoint_count} - 1")
string(JSON checkpoint GET "${entry}" checkpoint_history ${latest})
string(JSON saved GET "${checkpoint}" file)
get_filename_component(filename "${saved}" NAME)
if(NOT saved STREQUAL filename OR saved STREQUAL "")
    message(FATAL_ERROR "Invalid Sphere entry checkpoint name")
endif()
if(NOT ENTRY STREQUAL OUTPUT)
    configure_file("${ENTRY}/preview-user/${saved}" "${OUTPUT}/preview-user/${saved}" COPYONLY)
endif()

string(JSON phases LENGTH "${route}")
math(EXPR last "${phases} - 1")
foreach(i RANGE 0 ${last} 1)
    string(JSON phase GET "${route}" ${i})
    set(steps)
    string(
        JSON
        controls
        ERROR_VARIABLE
        legacy_phase
        GET
        "${phase}"
        inputs)
    if(legacy_phase)
        string(JSON controls GET "${phase}" portals)
    endif()
    string(
        JSON
        flight
        ERROR_VARIABLE
        default_flight
        GET
        "${phase}"
        flight)
    if(default_flight)
        set(flight
            [=[{"action":"fly_through_side","max_frames":800,"fire_primary":true,"engage_robots":false}]=]
        )
    endif()
    string(JSON count LENGTH "${controls}")
    math(EXPR last_control "${count} - 1")
    foreach(j RANGE 0 ${last_control} 1)
        string(JSON input GET "${controls}" ${j})
        string(JSON type TYPE "${input}")
        if(type STREQUAL "ARRAY")
            string(JSON room GET "${input}" 0)
            string(JSON side GET "${input}" 1)
            string(JSON step SET "${flight}" segment ${room})
            string(JSON step SET "${step}" side ${side})
            string(JSON arity LENGTH "${input}")
            if(arity EQUAL 3)
                string(JSON fire GET "${input}" 2)
                if(fire)
                    string(JSON step SET "${step}" fire_primary true)
                else()
                    string(JSON step SET "${step}" fire_primary false)
                endif()
            endif()
        else()
            set(step "${input}")
        endif()
        list(APPEND steps "${step}")
    endforeach()
    set(transitions 0)
    string(
        JSON
        exit_step
        ERROR_VARIABLE
        missing
        GET
        "${phase}"
        exit)
    if(NOT missing)
        list(APPEND steps "${exit_step}")
        set(transitions 1)
    endif()
    string(JSON expect GET "${phase}" expect)
    if(STAGE STREQUAL "morph" AND i EQUAL 1)
        # Compare the exact identities earned on this run, both at restore and after flight.
        string(JSON saved_slots GET "${checkpoint}" state morph_slots)
        string(
            JSON
            base
            SET
            "${base}"
            checkpoint_expect
            morph_slots
            "${saved_slots}")
        file(READ "${OUTPUT}/morph-0-state.json" warm)
        string(JSON warm_slots GET "${warm}" morph_slots)
        string(JSON expect SET "${expect}" morph_slots "${warm_slots}")
    endif()
    list(
        APPEND
        steps
        "{\"action\":\"assert\",\"expect\":${expect}}"
        [=[{"action":"set_debug","field":"android_game_request","value":"quick_save","export_android":true}]=]
    )
    if(STAGE STREQUAL "morph" AND i EQUAL 0)
        string(
            JSON
            continuation
            GET
            "${route}"
            1
            inputs
            0)
        list(APPEND steps "${continuation}")
    endif()
    list(LENGTH steps count)
    list(JOIN steps "," controls)
    string(JSON plan SET "${base}" steps "[${controls}]")
    string(
        JSON
        messages
        ERROR_VARIABLE
        missing
        GET
        "${phase}"
        acknowledge_messages)
    if(NOT missing)
        string(JSON plan SET "${plan}" acknowledge_messages "${messages}")
    endif()
    string(
        JSON
        tracked
        ERROR_VARIABLE
        missing
        GET
        "${phase}"
        track_objects)
    if(NOT missing)
        string(JSON plan SET "${plan}" track_objects "${tracked}")
    endif()
    set(name "${STAGE}-${i}")
    write_resume_plan("${name}" "${plan}" "${checkpoint}")
    string(JSON destination GET "${expect}" current_level_num)
    run_pumo_player_phase("${name}" ${initial} ${destination} ${count} ${transitions})
    if(STAGE STREQUAL "living_escape" AND transitions)
        string(
            JSON
            activation
            GET
            "${${name}_receipt}"
            transitions
            0
            last_activation)
        string(JSON boundary GET "${activation}" observation_boundary)
        string(JSON timer GET "${activation}" reactor_countdown timer)
        string(JSON segment GET "${activation}" position segment)
        string(JSON lives GET "${activation}" player lives)
        string(JSON shields GET "${activation}" player shields)
        string(
            JSON
            trigger
            GET
            "${activation}"
            xl_pilot
            wall_triggers
            11)
        string(JSON operated GET "${trigger}" operated)
        string(JSON actor GET "${trigger}" actor)
        string(JSON type GET "${trigger}" type)
        if(NOT boundary STREQUAL "pre_exit"
           OR NOT timer EQUAL 229376
           OR NOT segment EQUAL 3834
           OR NOT lives EQUAL 4
           OR NOT shields GREATER 0
           OR NOT operated
           OR NOT actor EQUAL 0
           OR NOT type EQUAL 3)
            message(FATAL_ERROR "Sphere living escape lacked its earned player exit")
        endif()
    endif()
    string(JSON checkpoint GET "${${name}_receipt}" checkpoints ${destination})
endforeach()
