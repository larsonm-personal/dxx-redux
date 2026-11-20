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

# Start both pilots in the same ship. The only control difference is whether to brake for the point
# beyond each open portal.
foreach(mode braking cruising)
    set(steps)
    foreach(room 1307 1304 1301 1298 1295)
        set(flight
            "{\"action\":\"fly_through_side\",\"segment\":${room},\"side\":0,\"max_frames\":800,\"cruise_speed\":100}"
        )
        if(mode STREQUAL "cruising")
            string(JSON flight SET "${flight}" maintain_cruise true)
        endif()
        list(APPEND steps "${flight}")
    endforeach()
    list(
        APPEND
        steps
        [=[{"action":"assert","expect":{"current_level_num":1,"position.segment":1193,"player.lives":3,"player_dead":false,"cheats_enabled":false}}]=]
    )
    list(JOIN steps "," controls)
    string(JSON plan SET "${base}" steps "[${controls}]")
    file(WRITE "${OUTPUT}/${mode}-plan.json" "${plan}\n")
    run_pumo_player_phase(${mode} 1 1 6 0)
    file(STRINGS "${OUTPUT}/${mode}-trace.jsonl" frames REGEX "\"flight_frame\":")
    list(LENGTH frames ${mode}_frames)
endforeach()
if(cruising_frames LESS 1 OR NOT cruising_frames LESS braking_frames)
    message(
        FATAL_ERROR
            "Portal traversal consumed ${cruising_frames} cruise frames and ${braking_frames} braking frames"
    )
endif()
message(
    STATUS "Five open portals: ${cruising_frames} cruise frames, ${braking_frames} braking frames")
