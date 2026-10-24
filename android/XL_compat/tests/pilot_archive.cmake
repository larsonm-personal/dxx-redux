# Private corpus integration: never substitute a synthetic pass when the HOG is absent
file(SHA256 "${HOG}" source_hash)
if(NOT source_hash STREQUAL "7491c45d217831509a840d934641bf1791e029ae6c1bcbe3e4efba2847d53d32")
    message(FATAL_ERROR "Pilot HOG differs from the audited original")
endif()
execute_process(COMMAND "${INSPECT}" "${HOG}" "${REPORT}" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Pilot archive inspection failed: ${result}")
endif()
file(READ "${REPORT}" report)
string(JSON count LENGTH "${report}" levels)
if(NOT count EQUAL 30)
    message(FATAL_ERROR "Expected all 30 levels")
endif()
set(found_pilot FALSE)
foreach(i RANGE 0 29 1)
    string(
        JSON
        status
        GET
        "${report}"
        levels
        ${i}
        inventory
        status)
    string(
        JSON
        playable
        GET
        "${report}"
        levels
        ${i}
        inventory
        playable)
    if(NOT status STREQUAL "inspection_only" OR playable)
        message(FATAL_ERROR "Level ${i}: rejected or falsely declared playable")
    endif()
    string(
        JSON
        name
        GET
        "${report}"
        levels
        ${i}
        name)
    if(name STREQUAL "d2leva-1.rl2")
        set(found_pilot TRUE)
        foreach(
            pair
            "vertices;828"
            "segments;228"
            "objects;110"
            "walls;86"
            "triggers;13"
            "vertex_colors;828"
            "side_colors;1368"
            "texture_colors;910"
            "object_trigger_heads;700"
            "light_indices;70"
            "delta_lights;3960")
            list(GET pair 0 key)
            list(GET pair 1 expected)
            string(JSON actual LENGTH "${report}" levels ${i} inventory ${key})
            if(NOT actual EQUAL expected)
                message(FATAL_ERROR "Pilot ${key}: expected ${expected}, got ${actual}")
            endif()
        endforeach()
    endif()
endforeach()
if(NOT found_pilot)
    message(FATAL_ERROR "Pilot level missing")
endif()
file(SHA256 "${REPORT}" first_hash)
file(TIMESTAMP "${REPORT}" first_time "%s")
execute_process(COMMAND "${INSPECT}" "${HOG}" "${REPORT}" RESULT_VARIABLE result)
file(SHA256 "${REPORT}" second_hash)
file(TIMESTAMP "${REPORT}" second_time "%s")
if(NOT result EQUAL 0 OR NOT first_hash STREQUAL second_hash OR NOT first_time STREQUAL second_time)
    message(FATAL_ERROR "Identical regeneration changed the report")
endif()
file(SHA256 "${HOG}" after_hash)
if(NOT source_hash STREQUAL after_hash)
    message(FATAL_ERROR "Inspection modified the input")
endif()
