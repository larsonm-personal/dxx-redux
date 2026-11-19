foreach(required ENGINE DATA RESOURCES OUTPUT)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Pumo hub flight requires ${required}")
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
set(PUMO_PHASE_TIMEOUT 360)
set(hub_plan "${CMAKE_CURRENT_LIST_DIR}/pumo_hub_multiverse_flight.json")
if(MULTIVERSE_EXIT)
    file(READ "${hub_plan}" flight)
    file(READ "${CMAKE_CURRENT_LIST_DIR}/pumo_multiverse_exit_steps.json" extension)
    # Continue immediately after the hub save. The short route waits for its cold phase here
    string(JSON flight REMOVE "${flight}" steps 15)
    string(JSON step_count LENGTH "${extension}")
    math(EXPR last "${step_count} - 1")
    foreach(index RANGE 0 ${last} 1)
        string(JSON step GET "${extension}" ${index})
        math(EXPR destination_index "${index} + 15")
        string(
            JSON
            flight
            SET
            "${flight}"
            steps
            ${destination_index}
            "${step}")
    endforeach()
    file(WRITE "${OUTPUT}/flight-plan.json" "${flight}\n")
    run_pumo_player_phase(flight 1 3 135 2)
    string(
        JSON
        intermediate
        GET
        "${flight_receipt}"
        transitions
        0
        to)
    if(NOT intermediate EQUAL 2)
        message(FATAL_ERROR "Pumo Multiverse route missed ordinal 2")
    endif()
    set(destination 3)
    set(cold_plan pumo_multiverse_exit_cold.json)
    set(cold_steps 6)
else()
    configure_file("${hub_plan}" "${OUTPUT}/flight-plan.json" COPYONLY)
    run_pumo_player_phase(flight 1 2 16 1)
    set(destination 2)
    set(cold_plan pumo_hub_multiverse_cold.json)
    set(cold_steps 8)
endif()
string(JSON flight_pid GET "${flight_receipt}" process_id)
string(
    JSON
    operations
    GET
    "${flight_receipt}"
    transitions
    0
    last_activation
    xl_pilot
    operation_counts)
foreach(trigger 1 6 0)
    string(JSON count GET "${operations}" "${trigger}")
    if(NOT count EQUAL 1)
        message(FATAL_ERROR "Hub trigger ${trigger} did not operate once")
    endif()
endforeach()
string(JSON checkpoint GET "${flight_receipt}" checkpoints "${destination}")
file(READ "${CMAKE_CURRENT_LIST_DIR}/${cold_plan}" cold)
function(write_resume_plan phase template checkpoint)
    set(cold "${template}")
    foreach(pair "file;resume_checkpoint" "sha256;resume_sha256")
        list(GET pair 0 source)
        list(GET pair 1 plan_field)
        string(JSON value GET "${checkpoint}" ${source})
        string(JSON cold SET "${cold}" ${plan_field} "\"${value}\"")
    endforeach()
    foreach(field position player)
        string(JSON saved GET "${checkpoint}" state ${field})
        string(
            JSON
            cold
            SET
            "${cold}"
            checkpoint_expect
            ${field}
            "${saved}")
    endforeach()
    file(WRITE "${OUTPUT}/${phase}-plan.json" "${cold}\n")
endfunction()
write_resume_plan(cold "${cold}" "${checkpoint}")
run_pumo_player_phase(cold ${destination} ${destination} ${cold_steps} 0)
string(JSON cold_pid GET "${cold_receipt}" process_id)
if(flight_pid EQUAL cold_pid)
    message(FATAL_ERROR "Pumo restore must use a new process")
endif()
foreach(phase flight cold)
    foreach(field fixed_view_aim_events position_assist_events)
        string(JSON count GET "${${phase}_receipt}" ${field})
        if(NOT count EQUAL 0)
            message(FATAL_ERROR "Pumo ${phase} used ${field}")
        endif()
    endforeach()
endforeach()

if(MAJHA_EXIT)
    file(READ "${CMAKE_CURRENT_LIST_DIR}/pumo_majha_exit_steps.json" route)
    set(previous_pid "${cold_pid}")
    foreach(
        phase
        blue_key
        supplies
        red_key
        reactor_approach
        reactor
        escape
        continue_play)
        string(JSON steps GET "${route}" "${phase}")
        string(JSON step_count LENGTH "${steps}")
        string(JSON template SET "${cold}" steps "${steps}")
        if(phase STREQUAL "continue_play")
            string(JSON template SET "${template}" initial_level 4)
            string(
                JSON
                template
                SET
                "${template}"
                checkpoint_expect
                current_level_num
                4)
        endif()
        write_resume_plan("${phase}" "${template}" "${checkpoint}")
        if(phase STREQUAL "escape")
            run_pumo_player_phase("${phase}" 3 4 "${step_count}" 1)
        elseif(phase STREQUAL "continue_play")
            run_pumo_player_phase("${phase}" 4 4 "${step_count}" 0)
        else()
            run_pumo_player_phase("${phase}" 3 3 "${step_count}" 0)
        endif()
        set(receipt "${${phase}_receipt}")
        string(JSON pid GET "${receipt}" process_id)
        foreach(field fixed_view_aim_events position_assist_events)
            string(JSON count GET "${receipt}" "${field}")
            if(NOT count EQUAL 0)
                message(FATAL_ERROR "Pumo ${phase} used ${field}")
            endif()
        endforeach()
        if(pid STREQUAL previous_pid)
            message(FATAL_ERROR "Pumo ${phase} did not cold-start from its earned checkpoint")
        endif()
        set(previous_pid "${pid}")
        if(phase STREQUAL "escape")
            string(JSON checkpoint GET "${receipt}" checkpoints 4)
        elseif(NOT phase STREQUAL "continue_play")
            string(JSON checkpoint GET "${receipt}" checkpoints 3)
        endif()
        if(phase STREQUAL "reactor")
            string(
                JSON
                countdown
                GET
                "${checkpoint}"
                state
                reactor_countdown
                active)
            if(NOT countdown)
                message(
                    FATAL_ERROR "The earned Majha save must include the running reactor countdown")
            endif()
        endif()
    endforeach()
endif()
