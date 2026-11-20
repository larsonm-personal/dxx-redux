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
string(JSON base SET "${base}" initial_level 3)
set(save [=[{"action":"set_debug","field":"android_game_request","value":"quick_save"}]=])
string(JSON plan SET "${base}" steps "[{\"action\":\"wait_ms\",\"ms\":1000},${save}]")
file(WRITE "${OUTPUT}/start-plan.json" "${plan}")
run_pumo_player_phase(start 3 3 2 0)
string(JSON checkpoint GET "${start_receipt}" checkpoints 3)

set(steps)
foreach(
    portal IN
    ITEMS "991,0"
          "989,4"
          "988,4"
          "1016,4"
          "1017,3"
          "1013,4"
          "1010,4"
          "333,4"
          "0,4"
          "1,4")
    string(REPLACE "," ";" pair "${portal}")
    list(GET pair 0 room)
    list(GET pair 1 side)
    list(
        APPEND
        steps
        "{\"action\":\"fly_through_side\",\"segment\":${room},\"side\":${side},\"max_frames\":800,\"fire_primary\":true}"
    )
endforeach()
set(observe [=[{"action":"observe_flicker","segment":2,"side":1,"max_frames":240}]=])
list(APPEND steps "${observe}" "${save}")
list(JOIN steps "," controls)
string(JSON plan SET "${base}" steps "[${controls}]")
write_resume_plan(flight "${plan}" "${checkpoint}")
list(LENGTH steps count)
run_pumo_player_phase(flight 3 3 ${count} 0)
string(JSON checkpoint GET "${flight_receipt}" checkpoints 3)
string(JSON plan SET "${base}" steps "[${observe}]")
write_resume_plan(cold "${plan}" "${checkpoint}")
run_pumo_player_phase(cold 3 3 1 0)
