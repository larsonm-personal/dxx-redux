foreach(required ENGINE DATA RESOURCES OUTPUT)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Pumo native player requires ${required}")
    endif()
endforeach()
find_program(PWSH pwsh REQUIRED)
get_filename_component(pumo_android_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
execute_process(
    COMMAND "${PWSH}" -NoProfile -File "${pumo_android_root}/helpers/retain-recent-artifacts.ps1"
            -Artifacts "${OUTPUT}" -Keep 1000 RESULT_VARIABLE retained)
if(NOT retained EQUAL 0)
    message(FATAL_ERROR "Pumo player output retention/reserve check failed")
endif()
file(MAKE_DIRECTORY "${OUTPUT}/missions" "${OUTPUT}/mods" "${OUTPUT}/data" "${OUTPUT}/preview-user")
foreach(member pmines.hog pmines.mn2)
    configure_file("${RESOURCES}/missions/${member}" "${OUTPUT}/missions/${member}" COPYONLY)
endforeach()
configure_file("${RESOURCES}/data/d2x-xl.hog" "${OUTPUT}/data/d2x-xl.hog" COPYONLY)
file(COPY "${RESOURCES}/mods/pmines" DESTINATION "${OUTPUT}/mods")
# Share the Android action/expectation source, with explicit presentation adaptation
execute_process(
    COMMAND
        "${PWSH}" -NoProfile -CommandWithArgs
        [=[
$ErrorActionPreference = 'Stop'
$steps = @(Get-Content -LiteralPath $args[0] -Raw | ConvertFrom-Json -AsHashtable | Select-Object -Skip 1)
if ($steps[-3].action -ne 'wait_for' -or $steps[-3].expect['menu.subtitle'].contains -ne 'Level 3 Complete') { throw 'Majha completion source changed' }
$steps = @($steps | Select-Object -SkipLast 3)
$steps += @(@{action='wait_for'; expect=@{current_level_num=4; 'position.segment'=126; player_dead=$false; 'player.has_blue_key'=$false; 'player.has_red_key'=$false; 'player.invulnerable'=$false; cheats_enabled=$false}; timeout_ms=20000}, @{action='introspect'})
$steps = @($steps | Select-Object -SkipLast 1)
$steps += @(@{action='set_debug'; field='android_game_request'; value='quick_save'}, @{action='face_view'; segment=0; side=0; distance=8}, @{action='set_debug'; field='android_game_request'; value='quick_load'}, @{action='wait_for'; expect=@{current_level_num=4; 'position.segment'=126; 'xl_pilot.runtime_active'=$true; player_dead=$false; 'player.has_blue_key'=$false; 'player.has_red_key'=$false}; timeout_ms=20000}, @{action='introspect'})
@{initial_level=3; qualification='initial_level_setup_then_ordinary_player_actions_unrendered'; steps=$steps} | ConvertTo-Json -Depth 24 | Set-Content -Encoding utf8 $args[1]
]=] "${pumo_android_root}/game_scripts/test_xl_pumo_majha_escape.jsonc" "${OUTPUT}/native-plan.json"
    RESULT_VARIABLE prepared)
if(NOT prepared EQUAL 0)
    message(FATAL_ERROR "Pumo player plan preparation failed")
endif()
execute_process(
    COMMAND
        "${CMAKE_COMMAND}" -E env --unset=SDL_VIDEODRIVER SDL_AUDIODRIVER=dummy "${ENGINE}"
        -pumo-player-test "${OUTPUT}/native-plan.json" -hogdir "${DATA}" -extra-dir "${OUTPUT}"
        -userdir "${OUTPUT}/preview-user" -nosound -nomusic
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
    TIMEOUT 150)
file(WRITE "${OUTPUT}/native-controls.log" "${stdout}${stderr}")
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Pumo native player failed (${result}): ${stdout}${stderr}")
endif()
file(READ "${OUTPUT}/player-result.json" receipt)
string(JSON status GET "${receipt}" result)
string(JSON initial GET "${receipt}" initial_level_setup)
string(JSON final GET "${receipt}" final_level)
string(JSON steps GET "${receipt}" steps)
if(NOT status STREQUAL "PASS" OR NOT initial EQUAL 3 OR NOT final EQUAL 4 OR NOT steps EQUAL 76)
    message(FATAL_ERROR "Pumo native player returned incomplete progression evidence")
endif()
string(JSON transitions LENGTH "${receipt}" transitions)
if(NOT transitions EQUAL 1)
    message(FATAL_ERROR "Pumo player did not record exactly one ordinary level transition")
endif()
foreach(index RANGE 0 26 1)
    # Exit6 transitions before its caller can publish old-level diagnostics
    if(index EQUAL 6)
        continue()
    endif()
    string(
        JSON
        operated
        GET
        "${receipt}"
        transitions
        0
        last_activation
        xl_pilot
        wall_triggers
        ${index}
        operated)
    if(NOT operated)
        message(FATAL_ERROR "Pumo Majha trigger${index} was not ordinarily operated before exit")
    endif()
endforeach()
foreach(member result state trace)
    if(member STREQUAL "trace")
        configure_file("${OUTPUT}/player-trace.jsonl" "${OUTPUT}/warm-trace.jsonl" COPYONLY)
    else()
        configure_file("${OUTPUT}/player-${member}.json" "${OUTPUT}/warm-${member}.json" COPYONLY)
    endif()
endforeach()
configure_file("${OUTPUT}/preview-user/pumo-player-level3.sav"
               "${OUTPUT}/preview-user/pumo-player-earned.sav" COPYONLY)
file(SHA256 "${OUTPUT}/preview-user/pumo-player-earned.sav" earned_hash)
execute_process(
    COMMAND
        "${PWSH}" -NoProfile -CommandWithArgs
        [=[
$ErrorActionPreference = 'Stop'
$warm = Get-Content -LiteralPath $args[0] -Raw | ConvertFrom-Json -AsHashtable
$receipt = Get-Content -LiteralPath $args[1] -Raw | ConvertFrom-Json -AsHashtable
$lastLoad = -1
for ($i=0; $i -lt $warm.steps.Count; $i++) { if ($warm.steps[$i].expect.current_level_num -eq 4) { break }; if ($warm.steps[$i].action -eq 'set_debug' -and $warm.steps[$i].value -eq 'quick_load') { $lastLoad=$i } }
if ($lastLoad -lt 0) { throw 'Majha source lacks an earned checkpoint' }
$player = $receipt.transitions[0].last_activation.player
@{initial_level=3; resume_checkpoint='pumo-player-earned.sav'; resume_sha256=$args[2]; checkpoint_expect=@{current_level_num=3; 'position.segment'=598; 'player.shields'=$player.shields; 'player.energy'=$player.energy; 'player.has_blue_key'=$true; 'player.has_red_key'=$true; 'xl_pilot.remaining_objectives'=0; 'reactor_countdown.active'=$true}; steps=@($warm.steps | Select-Object -Skip ($lastLoad+1))} | ConvertTo-Json -Depth 24 | Set-Content -Encoding utf8 $args[3]
]=] "${OUTPUT}/native-plan.json" "${OUTPUT}/warm-result.json" "${earned_hash}"
        "${OUTPUT}/native-cold-plan.json"
    RESULT_VARIABLE cold_prepared)
if(NOT cold_prepared EQUAL 0)
    message(FATAL_ERROR "Pumo earned cold checkpoint plan failed")
endif()
execute_process(
    COMMAND
        "${CMAKE_COMMAND}" -E env --unset=SDL_VIDEODRIVER SDL_AUDIODRIVER=dummy "${ENGINE}"
        -pumo-player-test "${OUTPUT}/native-cold-plan.json" -hogdir "${DATA}" -extra-dir "${OUTPUT}"
        -userdir "${OUTPUT}/preview-user" -nosound -nomusic
    RESULT_VARIABLE cold_result
    OUTPUT_VARIABLE cold_stdout
    ERROR_VARIABLE cold_stderr
    TIMEOUT 150)
file(WRITE "${OUTPUT}/cold-controls.log" "${cold_stdout}${cold_stderr}")
if(NOT cold_result EQUAL 0)
    message(
        FATAL_ERROR "Pumo native cold player failed (${cold_result}): ${cold_stdout}${cold_stderr}")
endif()
file(READ "${OUTPUT}/player-result.json" cold_receipt)
string(JSON cold_status GET "${cold_receipt}" result)
string(JSON cold_steps GET "${cold_receipt}" steps)
string(JSON cold_final GET "${cold_receipt}" final_level)
string(JSON warm_process GET "${receipt}" process_id)
string(JSON cold_process GET "${cold_receipt}" process_id)
if(NOT cold_status STREQUAL "PASS" OR NOT cold_steps EQUAL 10 OR NOT cold_final EQUAL 4
   OR warm_process EQUAL cold_process)
    message(FATAL_ERROR "Pumo cold process/checkpoint evidence incomplete")
endif()
foreach(member result state trace)
    if(member STREQUAL "trace")
        configure_file("${OUTPUT}/player-trace.jsonl" "${OUTPUT}/cold-trace.jsonl" COPYONLY)
    else()
        configure_file("${OUTPUT}/player-${member}.json" "${OUTPUT}/cold-${member}.json" COPYONLY)
    endif()
endforeach()
