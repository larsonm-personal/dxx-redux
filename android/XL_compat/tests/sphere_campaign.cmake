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
configure_file("${ENTRY}/preview-user/${saved}" "${OUTPUT}/preview-user/${saved}" COPYONLY)

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
    list(
        APPEND
        steps
        "{\"action\":\"assert\",\"expect\":${expect}}"
        [=[{"action":"set_debug","field":"android_game_request","value":"quick_save","export_android":true}]=]
    )
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
    set(name "${STAGE}-${i}")
    write_resume_plan("${name}" "${plan}" "${checkpoint}")
    string(JSON destination GET "${expect}" current_level_num)
    run_pumo_player_phase("${name}" ${initial} ${destination} ${count} ${transitions})
    string(JSON checkpoint GET "${${name}_receipt}" checkpoints ${destination})
endforeach()
