file(SHA256 "${HOG}" original_hash)
if(NOT original_hash STREQUAL "4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303")
    message(FATAL_ERROR "Effect rejection control requires unchanged original Neptune")
endif()
file(MAKE_DIRECTORY "${OUTPUT}/missions" "${OUTPUT}/preview-user" "${OUTPUT}/fixtures")
configure_file("${HOG}" "${OUTPUT}/missions/neptunxl.hog" COPYONLY)
execute_process(COMMAND "${FIXTURE}" --effect-fixtures "${OUTPUT}/fixtures" RESULT_VARIABLE result
                OUTPUT_VARIABLE log ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Effect inspection fixtures failed: ${log}${error}")
endif()
set(particle_world_args)
if(REFERENCE)
    list(APPEND particle_world_args -particle-world-export "${OUTPUT}/particle-world.txt")
endif()
execute_process(
    COMMAND "${ENGINE}" -effect-inspection-test "${OUTPUT}/fixtures" -hogdir "${DATA}" -extra-dir
            "${OUTPUT}" -userdir "${OUTPUT}/preview-user" -nosound -nomusic ${particle_world_args}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE log
    ERROR_VARIABLE error
    TIMEOUT 80)
file(WRITE "${OUTPUT}/controls.log" "${log}${error}")
string(FIND "${log}" "XL effect inspection gate passed" witness)
if(NOT result EQUAL 0 OR witness LESS 0)
    message(FATAL_ERROR "Effect inspection gate failed (${result}): ${log}${error}")
endif()
foreach(version IN ITEMS 23 25 27)
    string(
        FIND "${log}"
             "XL effect inspection version ${version}: stock/XL rejection and supported recovery passed"
             witness)
    if(witness LESS 0)
        message(FATAL_ERROR "Missing effect version ${version} rejection/recovery witness")
    endif()
endforeach()
string(FIND "${log}" "XL particle native ownership:" particle_witness)
if(particle_witness LESS 0)
    message(FATAL_ERROR "Missing native particle ownership witness")
endif()
string(FIND "${log}" "XL transparency native frame:" transparency_witness)
if(transparency_witness LESS 0)
    message(FATAL_ERROR "Missing native shared transparency frame witness")
endif()
string(FIND "${log}" "XL particle image resources:" image_witness)
if(image_witness LESS 0)
    message(FATAL_ERROR "Missing native particle image resource witness")
endif()
if(particle_world_args)
    execute_process(
        COMMAND "${REFERENCE}" --particle-world "${OUTPUT}/particle-world.txt"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE log
        ERROR_VARIABLE error
        TIMEOUT 30)
    file(WRITE "${OUTPUT}/particle-world-reference.log" "${log}${error}")
    string(FIND "${log}" "Pinned loaded particle world:" witness)
    if(NOT result EQUAL 0 OR witness LESS 0)
        message(
            FATAL_ERROR "Loaded particle world source comparison failed (${result}): ${log}${error}"
        )
    endif()
endif()
file(SHA256 "${HOG}" final_hash)
if(NOT final_hash STREQUAL original_hash)
    message(FATAL_ERROR "Original Neptune changed during effect rejection control")
endif()
