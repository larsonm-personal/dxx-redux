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
elseif(STAGE STREQUAL "storage")
    set(initial 2)
    set(entry_receipt shaft-3-result.json)
else()
    message(FATAL_ERROR "Unknown Sphere campaign stage ${STAGE}")
endif()
string(JSON base SET "${base}" initial_level ${initial})
file(READ "${CMAKE_CURRENT_LIST_DIR}/sphere_${STAGE}.json" route)
file(READ "${ENTRY}/${entry_receipt}" entry)
string(JSON checkpoint GET "${entry}" checkpoints ${initial})
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
    string(JSON portals LENGTH "${phase}" portals)
    math(EXPR final_portal "${portals} - 1")
    foreach(j RANGE 0 ${final_portal} 1)
        string(
            JSON
            room
            GET
            "${phase}"
            portals
            ${j}
            0)
        string(
            JSON
            side
            GET
            "${phase}"
            portals
            ${j}
            1)
        list(
            APPEND
            steps
            "{\"action\":\"fly_through_side\",\"segment\":${room},\"side\":${side},\"max_frames\":800,\"fire_primary\":true,\"engage_robots\":false}"
        )
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
