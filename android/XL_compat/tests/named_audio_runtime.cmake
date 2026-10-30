file(SHA256 "${HOG}" source_hash)
if(REFERENCE AND NOT EFFECT_REFERENCE)
    message(FATAL_ERROR "Audio source comparison also requires the ambient activation reference")
endif()
if(NOT source_hash STREQUAL "4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303")
    message(FATAL_ERROR "Named audio controls require unchanged audited Neptune geometry")
endif()
file(MAKE_DIRECTORY "${OUTPUT}/missions" "${OUTPUT}/preview-user")
execute_process(COMMAND "${FIXTURE}" "${HOG}" "${OUTPUT}/missions" RESULT_VARIABLE result
                OUTPUT_VARIABLE log ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Cannot create named audio controls: ${log}${error}")
endif()
file(WRITE "${OUTPUT}/runtime.log" "")
# Each format starts with a fresh window/pause lifecycle in the headless host
foreach(ordinal IN ITEMS 1 2)
    if(ordinal EQUAL 1)
        set(version 23)
    else()
        set(version 27)
    endif()
    execute_process(
        COMMAND "${ENGINE}" -named-audio-test production -audio-level "${ordinal}" -hogdir "${DATA}"
                -extra-dir "${OUTPUT}" -userdir "${OUTPUT}/preview-user" -nomusic
        RESULT_VARIABLE result
        OUTPUT_VARIABLE log
        ERROR_VARIABLE error
        TIMEOUT 45)
    file(APPEND "${OUTPUT}/runtime.log" "${log}${error}")
    if(NOT result EQUAL 0 OR NOT log MATCHES "XL named audio runtime controls passed"
       OR NOT log MATCHES "${version}/40 sounds: wall/master/object/autoplay")
        message(FATAL_ERROR "Named audio version ${version} failed (${result}): ${log}${error}")
    endif()
    if(REFERENCE)
        execute_process(
            COMMAND "${REFERENCE}" --creation-runtime "${OUTPUT}/audio-creation.txt"
                    --surface-runtime "${OUTPUT}/surface-audio.txt"
            RESULT_VARIABLE result
            OUTPUT_VARIABLE log
            ERROR_VARIABLE error
            TIMEOUT 15)
        file(APPEND "${OUTPUT}/runtime.log" "${log}${error}")
        if(NOT result EQUAL 0 OR NOT log MATCHES "native fresh sound creations match actual source"
           OR NOT log MATCHES "loaded-mine surface cases.*match actual source")
            message(
                FATAL_ERROR "Native audio creation differs from source (${result}): ${log}${error}")
        endif()
    endif()
    execute_process(
        COMMAND "${ENGINE}" -named-audio-test ambient -audio-level "${ordinal}" -hogdir "${DATA}"
                -extra-dir "${OUTPUT}" -userdir "${OUTPUT}/preview-user" -nomusic
        RESULT_VARIABLE result
        OUTPUT_VARIABLE log
        ERROR_VARIABLE error
        TIMEOUT 15)
    file(APPEND "${OUTPUT}/runtime.log" "${log}${error}")
    if(NOT result EQUAL 0 OR NOT log MATCHES "Loaded ambient objects:.*passed")
        message(FATAL_ERROR "Ambient object version ${version} failed (${result}): ${log}${error}")
    endif()
    if(REFERENCE)
        execute_process(
            COMMAND "${REFERENCE}" --ambient-runtime "${OUTPUT}/ambient-audio.txt"
            RESULT_VARIABLE result
            OUTPUT_VARIABLE log
            ERROR_VARIABLE error
            TIMEOUT 15)
        file(APPEND "${OUTPUT}/runtime.log" "${log}${error}")
        if(NOT result EQUAL 0 OR NOT log MATCHES "loaded ambient owners match actual source")
            message(
                FATAL_ERROR "Ambient initialization differs from source (${result}): ${log}${error}"
            )
        endif()
    endif()
    if(EFFECT_REFERENCE)
        execute_process(
            COMMAND "${EFFECT_REFERENCE}" --ambient-runtime "${OUTPUT}/ambient-activation.txt"
            RESULT_VARIABLE result
            OUTPUT_VARIABLE log
            ERROR_VARIABLE error
            TIMEOUT 15)
        file(APPEND "${OUTPUT}/runtime.log" "${log}${error}")
        if(NOT result EQUAL 0 OR NOT log MATCHES
                                 "loaded wall/attached ambient actions match actual source")
            message(
                FATAL_ERROR "Ambient activation differs from source (${result}): ${log}${error}")
        endif()
    endif()
endforeach()
execute_process(
    COMMAND "${ENGINE}" -audio-scope-test production -hogdir "${DATA}" -extra-dir "${OUTPUT}"
            -userdir "${OUTPUT}/preview-user" -nomusic
    RESULT_VARIABLE result
    OUTPUT_VARIABLE log
    ERROR_VARIABLE error
    TIMEOUT 45)
file(APPEND "${OUTPUT}/runtime.log" "${log}${error}")
if(NOT result EQUAL 0 OR NOT log MATCHES "XL sound scope runtime passed")
    message(FATAL_ERROR "Named audio scopes failed (${result}): ${log}${error}")
endif()
if(REFERENCE)
    execute_process(
        COMMAND "${REFERENCE}" --geometry-runtime "${OUTPUT}/audio-geometry.txt"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE log
        ERROR_VARIABLE error
        TIMEOUT 15)
    file(APPEND "${OUTPUT}/runtime.log" "${log}${error}")
    if(NOT result EQUAL 0 OR NOT log MATCHES
                             "loaded-mine spatial audio outputs match actual source")
        message(FATAL_ERROR "Loaded audio geometry differs from source (${result}): ${log}${error}")
    endif()
endif()
file(SHA256 "${HOG}" final_hash)
if(NOT final_hash STREQUAL source_hash)
    message(FATAL_ERROR "Original geometry changed during named audio controls")
endif()
