cmake_minimum_required(VERSION 3.21)
# Three representative production-path scenarios, followed by a fresh-process restore
foreach(required ENGINE DATA RESOURCES OUTPUT)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Pumo scenarios require ${required}")
    endif()
endforeach()
include("${CMAKE_CURRENT_LIST_DIR}/pumo_player_phase.cmake")
set(owner "${OUTPUT}")
set(earned_save "")
set(seen_nonces)
foreach(phase smoke smoke_cold combat progression guided cold)
    set(OUTPUT "${owner}/${phase}")
    file(MAKE_DIRECTORY "${OUTPUT}/preview-user")
    if(phase MATCHES "cold$")
        # Compare persisted gameplay state, excluding the relative simulation clock
        set(expect "{}")
        foreach(
            path IN
            ITEMS player.primary_weapon
                  player.secondary_weapon
                  player.primary_weapon_flags
                  player.secondary_weapon_flags
                  player.secondary_ammo
                  player.has_blue_key
                  player.has_red_key
                  player.has_gold_key
                  player.robots_killed_level
                  player.shields
                  player.energy
                  position.segment
                  position.x
                  position.y
                  position.z
                  xl_pilot.wall_triggers.6.operated
                  xl_pilot.doorways.57.type
                  xl_pilot.doorways.58.type
                  xl_pilot.remaining_objectives)
            string(REPLACE "." ";" fields "${path}")
            string(JSON value GET "${prior_state}" ${fields})
            string(JSON type TYPE "${prior_state}" ${fields})
            if(type STREQUAL "BOOLEAN")
                if(value)
                    set(value true)
                else()
                    set(value false)
                endif()
            endif()
            string(JSON expect SET "${expect}" "${path}" "${value}")
        endforeach()
        string(JSON plan SET "{\"initial_level\":7,\"steps\":[{\"action\":\"introspect\"}]}"
               checkpoint_expect "${expect}")
    else()
        file(READ "${CMAKE_CURRENT_LIST_DIR}/pumo_scenario_${phase}.json" plan)
    endif()
    if(phase STREQUAL "combat")
        set(earned_save "")
    endif()
    if(earned_save)
        configure_file("${earned_save}" "${OUTPUT}/preview-user/input.sav" COPYONLY)
        file(SHA256 "${OUTPUT}/preview-user/input.sav" input_hash)
        string(JSON plan SET "${plan}" resume_checkpoint "\"input.sav\"")
        string(JSON plan SET "${plan}" resume_sha256 "\"${input_hash}\"")
    endif()
    file(WRITE "${OUTPUT}/${phase}-plan.json" "${plan}\n")
    string(JSON steps LENGTH "${plan}" steps)
    run_pumo_player_phase(${phase} 7 7 ${steps} 0)
    string(JSON nonce GET "${${phase}_receipt}" run_id)
    if(nonce IN_LIST seen_nonces)
        message(FATAL_ERROR "Pumo ${phase} reused a run identity")
    endif()
    list(APPEND seen_nonces "${nonce}")
    file(READ "${OUTPUT}/player-state.json" prior_state)
    if(phase STREQUAL "guided" OR phase STREQUAL "progression")
        file(STRINGS "${OUTPUT}/player-trace.jsonl" contacts
             REGEX "\"event\":\"weapon_wall_contacts\"")
        string(JSON count LENGTH "${contacts}" result contacts)
        if(count LESS 1)
            message(FATAL_ERROR "Pumo ${phase} missing actual weapon-wall contact")
        endif()
        string(
            JSON
            weapon
            GET
            "${contacts}"
            result
            contacts
            0
            weapon_id)
        if(phase STREQUAL "guided" AND NOT weapon EQUAL 37)
            message(FATAL_ERROR "Pumo guided contact is not the fired guided missile")
        endif()
    endif()
    if(phase STREQUAL "smoke")
        string(JSON assists GET "${${phase}_receipt}" position_assist_events)
        file(STRINGS "${OUTPUT}/player-trace.jsonl" first REGEX "\"step\":0[},]")
        string(JSON initial_position GET "${first}" state position)
        string(JSON final_position GET "${prior_state}" position)
        if(assists OR initial_position STREQUAL final_position)
            message(FATAL_ERROR "Pumo ordinary smoke did not move through unassisted input")
        endif()
    endif()
    if(NOT phase MATCHES "cold$")
        string(JSON count LENGTH "${${phase}_receipt}" checkpoint_history)
        if(NOT count EQUAL 1)
            message(FATAL_ERROR "Pumo ${phase} must produce one fresh checkpoint")
        endif()
        string(
            JSON
            saved
            GET
            "${${phase}_receipt}"
            checkpoint_history
            0
            file)
        set(earned_save "${OUTPUT}/preview-user/${saved}")
    endif()
endforeach()
message(STATUS "Pumo combat, progression, guided contact and cold restore passed")
