# Assisted route diagnostics only, never a normal-playthrough acceptance test
include("${CMAKE_CURRENT_LIST_DIR}/stock_probe.cmake")
file(MAKE_DIRECTORY "${fixture}/route-user")
function(run_route name)
    execute_process(
        COMMAND
            "${ROUTE}" -route-confirm-json-out "${fixture}/${name}.json" -level 1 -mission xlprobe
            -hogdir "${DATA}" -extra-dir "${fixture}" -route-confirm-user-dir
            "${fixture}/route-user" -route-confirm-timeout-seconds 120 ${ARGN}
        RESULT_VARIABLE result
        TIMEOUT 40
        OUTPUT_FILE "${fixture}/${name}.log"
        ERROR_FILE "${fixture}/${name}-error.log")
    if(NOT result STREQUAL "0")
        message(FATAL_ERROR "Route diagnostic ${name} failed: ${result}; see ${fixture}")
    endif()
    file(READ "${fixture}/${name}.json" report)
    string(JSON status GET "${report}" status)
    if(NOT status STREQUAL "confirmed")
        message(FATAL_ERROR "Route diagnostic ${name} did not confirm its limited objective")
    endif()
    set(${name}_report "${report}" PARENT_SCOPE)
endfunction()

run_route(full)
run_route(repeated)
file(SHA256 "${fixture}/full.json" first_hash)
file(SHA256 "${fixture}/repeated.json" repeated_hash)
if(NOT first_hash STREQUAL repeated_hash)
    message(FATAL_ERROR "Fresh route diagnostic is not reproducible")
endif()
string(JSON count LENGTH "${full_report}" objectives)
if(NOT count EQUAL 3)
    message(FATAL_ERROR "Expected key, reactor and exit route objectives")
endif()
set(expected_kinds key reactor exit)
foreach(i RANGE 0 2 1)
    list(GET expected_kinds ${i} expected_kind)
    string(
        JSON
        actual_kind
        GET
        "${full_report}"
        objectives
        ${i}
        kind)
    if(NOT actual_kind STREQUAL expected_kind)
        message(FATAL_ERROR "Unexpected route objective ${i}: ${actual_kind}")
    endif()
endforeach()
run_route(key -route-confirm-key red -route-confirm-checkpoint-out xl-key.sav)
if(NOT EXISTS "${fixture}/route-user/xl-key.sav")
    message(FATAL_ERROR "No checkpoint was written")
endif()
run_route(restored -route-confirm-checkpoint xl-key.sav)
string(JSON start_kind GET "${restored_report}" start_state kind)
string(JSON key_flags GET "${restored_report}" start_state key_flags)
string(JSON objective_count LENGTH "${restored_report}" objectives)
string(
    JSON
    first_kind
    GET
    "${restored_report}"
    objectives
    0
    kind)
string(
    JSON
    last_kind
    GET
    "${restored_report}"
    objectives
    1
    kind)
if(NOT start_kind STREQUAL "saved_world"
   OR NOT key_flags EQUAL 4
   OR NOT objective_count EQUAL 2
   OR NOT first_kind STREQUAL "reactor"
   OR NOT last_kind STREQUAL "exit")
    message(
        FATAL_ERROR "Restored route did not retain the red key and finish reactor/exit objectives")
endif()
file(SHA256 "${LEVEL}" final_source_hash)
if(NOT final_source_hash STREQUAL original_hash)
    message(FATAL_ERROR "Source level changed during route diagnostic")
endif()
file(
    WRITE "${fixture}/coverage.json"
    "{\n  \"status\": \"assisted_route_diagnostic_only\",\n  \"playable\": false,\n  \"fresh_route_reproducible\": true,\n  \"red_key_checkpoint_restore\": true,\n  \"normal_playthrough\": \"unverified\",\n  \"all_triggers_and_branches\": \"unverified\",\n  \"reference_parity\": \"unverified\",\n  \"presentation\": \"unsupported\",\n  \"assistance\": [\"separate_test_actor\", \"player_invulnerability\", \"boosted_projectile_damage\", \"ordinary_robots_removed\", \"reactor_countdown_paused\", \"position_committed_for_checkpoint\"]\n}\n"
)
message(
    STATUS
        "Assisted route and red-key checkpoint diagnostics passed; mission support remains UNVERIFIED"
)
