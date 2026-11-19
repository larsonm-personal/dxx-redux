foreach(required ENGINE DATA RESOURCES ENTRY OUTPUT STAGE)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Black Prophecy flight requires ${required}")
    endif()
endforeach()
if(NOT STAGE MATCHES "^(approach|return)$")
    message(FATAL_ERROR "Unknown Black Prophecy flight stage ${STAGE}")
endif()
find_program(PWSH pwsh REQUIRED)
execute_process(
    COMMAND
        "${PWSH}" -NoProfile -File
        "${CMAKE_CURRENT_LIST_DIR}/../../helpers/retain-recent-artifacts.ps1" -Artifacts
        "${OUTPUT}" COMMAND_ERROR_IS_FATAL ANY)
file(MAKE_DIRECTORY "${OUTPUT}/preview-user")
include("${CMAKE_CURRENT_LIST_DIR}/pumo_player_phase.cmake")
set(XL_PLAYER_MISSION PROPHXL)
set(PUMO_PHASE_TIMEOUT 300)
set(base
    [=[{"mission":{"filename":"PROPHXL","descriptor_sha256":"8a47df99aaa72d34366d8ae357f1303e2c82c631163b062636b19d20c878b62b","hog_sha256":"aead9b792e3dc6ff13cd53b68e727d8cd814e1b6480ea83f5e5c9843caf23880"},"initial_level":2,"simulation_fps":60,"checkpoint_expect":{}}]=]
)
set(save [=[{"action":"set_debug","field":"android_game_request","value":"quick_save"}]=])
if(STAGE STREQUAL "return")
    file(READ "${ENTRY}/entry-result.json" earned)
    string(JSON checkpoint GET "${earned}" checkpoints 2)
    string(JSON filename GET "${checkpoint}" file)
    get_filename_component(basename "${filename}" NAME)
    if(NOT filename STREQUAL basename OR filename STREQUAL "")
        message(FATAL_ERROR "Invalid Phoenix entry checkpoint name")
    endif()
    configure_file("${ENTRY}/preview-user/${filename}" "${OUTPUT}/preview-user/${filename}"
                   COPYONLY)
endif()

file(READ "${CMAKE_CURRENT_LIST_DIR}/phoenix_secret_${STAGE}.json" route)
string(JSON phases LENGTH "${route}")
math(EXPR final "${phases} - 1")
foreach(index RANGE 0 ${final} 1)
    set(steps)
    string(JSON blocks GET "${route}" ${index})
    string(JSON block_count LENGTH "${blocks}")
    math(EXPR last "${block_count} - 1")
    foreach(i RANGE 0 ${last} 1)
        string(JSON block GET "${blocks}" ${i})
        string(JSON portals ERROR_VARIABLE missing LENGTH "${block}" portals)
        if(missing)
            list(APPEND steps "${block}")
        else()
            string(JSON control REMOVE "${block}" portals)
            string(JSON control SET "${control}" action "\"fly_through_side\"")
            math(EXPR last_portal "${portals} - 1")
            foreach(j RANGE 0 ${last_portal} 1)
                string(
                    JSON
                    room
                    GET
                    "${block}"
                    portals
                    ${j}
                    0)
                string(
                    JSON
                    side
                    GET
                    "${block}"
                    portals
                    ${j}
                    1)
                string(JSON control SET "${control}" segment ${room})
                string(JSON control SET "${control}" side ${side})
                list(APPEND steps "${control}")
            endforeach()
        endif()
    endforeach()
    if(EXPORT_ANDROID AND index EQUAL final)
        string(JSON save SET "${save}" export_android true)
    endif()
    list(APPEND steps "${save}")
    list(LENGTH steps count)
    list(JOIN steps "," controls)
    string(JSON plan SET "${base}" steps "[${controls}]")
    set(name "flight-${index}")
    if(DEFINED checkpoint)
        write_resume_plan("${name}" "${plan}" "${checkpoint}")
    else()
        file(WRITE "${OUTPUT}/${name}-plan.json" "${plan}\n")
    endif()
    set(transitions 0)
    if(STAGE STREQUAL "return")
        set(transitions 2)
    endif()
    run_pumo_player_phase("${name}" 2 2 "${count}" "${transitions}")
    set(receipt "${${name}_receipt}")
    string(JSON checkpoint GET "${receipt}" checkpoints 2)
endforeach()
if(EXPORT_ANDROID)
    string(
        JSON
        exported
        GET
        "${receipt}"
        checkpoint_history
        0
        android_export
        file)
    string(
        JSON
        expected_hash
        GET
        "${receipt}"
        checkpoint_history
        0
        android_export
        sha256)
    file(SHA256 "${OUTPUT}/preview-user/${exported}" actual_hash)
    if(NOT actual_hash STREQUAL expected_hash)
        message(FATAL_ERROR "Phoenix Android export did not match its native receipt")
    endif()
endif()
if(STAGE STREQUAL "approach")
    configure_file("${OUTPUT}/${name}-result.json" "${OUTPUT}/entry-result.json" COPYONLY)
else()
    string(
        JSON
        secret
        GET
        "${receipt}"
        transitions
        0
        to)
    if(NOT secret EQUAL -1)
        message(FATAL_ERROR "Phoenix did not enter its first signed secret")
    endif()
    # Reopen the returned positive-level world and run one second of simulation
    math(EXPR assertion "${count} - 2")
    string(JSON restored GET "${plan}" steps ${assertion})
    string(JSON plan SET "${base}" steps
           "[${restored},{\"action\":\"wait_ms\",\"ms\":1000},${restored}]")
    write_resume_plan(cold "${plan}" "${checkpoint}")
    run_pumo_player_phase(cold 2 2 3 0)
endif()
