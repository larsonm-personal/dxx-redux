find_program(PWSH pwsh REQUIRED)
execute_process(
    COMMAND
        "${PWSH}" -NoProfile -File
        "${CMAKE_CURRENT_LIST_DIR}/../../helpers/retain-recent-artifacts.ps1" -Artifacts
        "${OUTPUT}" COMMAND_ERROR_IS_FATAL ANY)
file(MAKE_DIRECTORY "${OUTPUT}/preview-user")
include("${CMAKE_CURRENT_LIST_DIR}/pumo_player_phase.cmake")
file(READ "${CMAKE_CURRENT_LIST_DIR}/sphere_reactor_aim.json" plan)
string(RANDOM LENGTH 24 ALPHABET 0123456789abcdef expected_run_id)
string(JSON plan SET "${plan}" receipt_run_id "\"${expected_run_id}\"")
set(executed_plan "${OUTPUT}/aim-plan.json")
file(WRITE "${executed_plan}" "${plan}\n")
file(SHA256 "${executed_plan}" expected_plan_sha256)
file(SHA256 "${ENGINE}" expected_executable_sha256)
set(expected_resume_sha256 "")
execute_process(
    COMMAND
        "${CMAKE_COMMAND}" -E env --unset=SDL_VIDEODRIVER SDL_AUDIODRIVER=dummy "${ENGINE}"
        -xl-player-test "${executed_plan}" -xl-player-resources "${RESOURCES}" -hogdir "${DATA}"
        -extra-dir "${OUTPUT}" -userdir "${OUTPUT}/preview-user" -nosound -nomusic
    WORKING_DIRECTORY "${OUTPUT}"
    OUTPUT_FILE "${OUTPUT}/aim-controls.log"
    ERROR_FILE "${OUTPUT}/aim-controls.log"
    TIMEOUT 150 COMMAND_ERROR_IS_FATAL ANY)
validate_pumo_player_receipt(aim 2 2 4 0)
string(JSON mission GET "${aim_receipt}" mission)
string(JSON positions GET "${aim_receipt}" position_assist_events)
string(JSON fixed_views GET "${aim_receipt}" fixed_view_aim_events)
if(NOT mission STREQUAL "sphere" OR NOT positions EQUAL 1 OR NOT fixed_views EQUAL 0)
    message(FATAL_ERROR "Sphere reactor control requires exactly its single positioning assist")
endif()
