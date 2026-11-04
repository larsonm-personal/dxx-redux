# Shared earned-save native phase receipt checks; original actions stay in JSON
function(run_pumo_player_phase name initial destination step_count transition_count)
    execute_process(
        COMMAND
            "${CMAKE_COMMAND}" -E env --unset=SDL_VIDEODRIVER SDL_AUDIODRIVER=dummy "${ENGINE}"
            -pumo-player-test "${OUTPUT}/${name}-plan.json" -pumo-resource-root "${RESOURCES}"
            -hogdir "${DATA}" -extra-dir "${OUTPUT}" -userdir "${OUTPUT}/preview-user" -nosound
            -nomusic
        RESULT_VARIABLE exit_code
        OUTPUT_VARIABLE stdout
        ERROR_VARIABLE stderr
        TIMEOUT 150)
    file(WRITE "${OUTPUT}/${name}-controls.log" "${stdout}${stderr}")
    if(NOT exit_code EQUAL 0)
        message(FATAL_ERROR "Pumo ${name} failed (${exit_code}): ${stdout}${stderr}")
    endif()
    file(READ "${OUTPUT}/player-result.json" receipt)
    string(JSON status GET "${receipt}" result)
    string(JSON steps GET "${receipt}" steps)
    string(JSON final GET "${receipt}" final_level)
    string(JSON setup GET "${receipt}" initial_level_setup)
    string(JSON resumed GET "${receipt}" resumed_earned_checkpoint)
    string(JSON count LENGTH "${receipt}" transitions)
    if(NOT status STREQUAL "PASS"
       OR NOT steps EQUAL step_count
       OR NOT setup EQUAL initial
       OR NOT final EQUAL destination
       OR NOT resumed
       OR NOT count EQUAL transition_count)
        message(FATAL_ERROR "Pumo ${name} progression evidence incomplete")
    endif()
    if(transition_count)
        string(
            JSON
            source
            GET
            "${receipt}"
            transitions
            0
            from)
        string(
            JSON
            target
            GET
            "${receipt}"
            transitions
            0
            to)
        if(NOT source EQUAL initial OR NOT target EQUAL destination)
            message(FATAL_ERROR "Pumo ${name} transition mismatch")
        endif()
    endif()
    foreach(member result state)
        configure_file("${OUTPUT}/player-${member}.json" "${OUTPUT}/${name}-${member}.json"
                       COPYONLY)
    endforeach()
    configure_file("${OUTPUT}/player-trace.jsonl" "${OUTPUT}/${name}-trace.jsonl" COPYONLY)
    set(${name}_receipt "${receipt}" PARENT_SCOPE)
endfunction()
