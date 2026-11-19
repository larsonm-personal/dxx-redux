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
configure_file("${CMAKE_CURRENT_LIST_DIR}/pumo_hub_multiverse_flight.json"
               "${OUTPUT}/flight-plan.json" COPYONLY)
run_pumo_player_phase(flight 1 2 16 1)
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
string(JSON checkpoint GET "${flight_receipt}" checkpoints "2")
file(READ "${CMAKE_CURRENT_LIST_DIR}/pumo_hub_multiverse_cold.json" cold)
foreach(pair "file;resume_checkpoint" "sha256;resume_sha256")
    list(GET pair 0 source)
    list(GET pair 1 destination)
    string(JSON value GET "${checkpoint}" ${source})
    string(JSON cold SET "${cold}" ${destination} "\"${value}\"")
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
file(WRITE "${OUTPUT}/cold-plan.json" "${cold}\n")
run_pumo_player_phase(cold 2 2 8 0)
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
