foreach(required ENGINE DATA RESOURCES OUTPUT)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Sphere red key flight requires ${required}")
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
set(XL_PLAYER_MISSION sphere)
set(PUMO_PHASE_TIMEOUT 300)
file(READ "${CMAKE_CURRENT_LIST_DIR}/sphere_red_key.json" plan)
file(WRITE "${OUTPUT}/flight-plan.json" "${plan}")
string(JSON count LENGTH "${plan}" steps)
run_pumo_player_phase(flight 1 1 "${count}" 0)
string(JSON checkpoint GET "${flight_receipt}" checkpoints 1)
math(EXPR before_save "${count} - 2")
string(JSON assertion GET "${plan}" steps ${before_save})
string(JSON plan SET "${plan}" steps
       "[${assertion},{\"action\":\"wait_ms\",\"ms\":1000},${assertion}]")
write_resume_plan(cold "${plan}" "${checkpoint}")
run_pumo_player_phase(cold 1 1 3 0)
