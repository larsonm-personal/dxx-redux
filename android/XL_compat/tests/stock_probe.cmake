# Loader coverage only: counts are not behavioral or presentation equivalence
file(SHA256 "${LEVEL}" original_hash)
if(NOT original_hash STREQUAL "5e835e76e984c658531175640294a4b05f6127904884ba7acfd65c93165be5b2")
    message(FATAL_ERROR "Pilot level differs from audited original")
endif()
file(MAKE_DIRECTORY "${OUTPUT}")
string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef run_id)
set(fixture "${OUTPUT}/${run_id}")
set(probe_kind --incomplete-presentation-probe)
if(SOURCE_RUNTIME)
    set(probe_kind --source-runtime-probe)
endif()
execute_process(COMMAND "${PROBE}" "${probe_kind}" "${LEVEL}" "${fixture}" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Probe creation failed")
endif()
execute_process(COMMAND "${PROBE}" "${probe_kind}" "${LEVEL}" "${fixture}"
                RESULT_VARIABLE overwrite)
if(overwrite EQUAL 0)
    message(FATAL_ERROR "Probe overwrote existing output")
endif()
file(READ "${fixture}/xlprobe.source.json" inventory)
string(JSON playable GET "${inventory}" playable)
if(playable)
    message(FATAL_ERROR "Probe falsely grants playability")
endif()
execute_process(
    COMMAND "${ENGINE}" -secretarea-json-out "${fixture}/metadata.json" -hogdir "${DATA}" -extra-dir
            "${fixture}" -mission xlprobe -level 1 -userdir "${fixture}/user" RESULT_VARIABLE result
    OUTPUT_FILE "${fixture}/loader.log" ERROR_FILE "${fixture}/loader-error.log")
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Stock loader rejected fixture; see ${fixture}")
endif()
file(READ "${fixture}/metadata.json" metadata)
foreach(
    pair
    segment_count:228
    wall_count:86
    trigger_count:13
    object_count:110
    robot_count:38
    hostage_count:4
    powerup_count:55
    reactor_count:1)
    string(REPLACE ":" ";" parts "${pair}")
    list(GET parts 0 key)
    list(GET parts 1 expected)
    string(
        JSON
        actual
        GET
        "${metadata}"
        levels
        0
        "${key}")
    if(NOT actual EQUAL expected)
        message(FATAL_ERROR "Stock ${key}: expected ${expected}, got ${actual}")
    endif()
endforeach()
file(SHA256 "${LEVEL}" after_hash)
if(NOT after_hash STREQUAL original_hash)
    message(FATAL_ERROR "Source changed")
endif()
message(STATUS "Stock decoding counts agree; gameplay and presentation remain UNVERIFIED")
