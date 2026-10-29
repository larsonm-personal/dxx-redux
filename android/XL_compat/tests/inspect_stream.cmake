execute_process(COMMAND "${FIXTURE}" --write "${OUTPUT}" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Cannot create streaming inspection controls")
endif()
file(SHA256 "${OUTPUT}/stream.hog" source_hash)
execute_process(COMMAND "${INSPECT}" "${OUTPUT}/stream.hog" "${OUTPUT}/report.json"
                RESULT_VARIABLE result OUTPUT_VARIABLE log ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Streaming large HOG failed: ${log}${error}")
endif()
file(READ "${OUTPUT}/report.json" report)
string(
    JSON
    version
    GET
    "${report}"
    levels
    0
    inventory
    level_version)
string(JSON count LENGTH "${report}" levels)
if(NOT version EQUAL 27 OR NOT count EQUAL 1)
    message(FATAL_ERROR "Large nonlevel payload prevented complete level inspection")
endif()
execute_process(COMMAND "${FIXTURE}" --age "${OUTPUT}/report.json" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Cannot age stable-report control")
endif()
file(TIMESTAMP "${OUTPUT}/report.json" before UTC)
execute_process(COMMAND "${INSPECT}" "${OUTPUT}/stream.hog" "${OUTPUT}/report.json"
                RESULT_VARIABLE result)
file(TIMESTAMP "${OUTPUT}/report.json" after UTC)
file(READ "${OUTPUT}/report.json" repeated)
if(NOT result EQUAL 0 OR NOT before STREQUAL after OR NOT report STREQUAL repeated)
    message(FATAL_ERROR "Identical inspection rewrote its report")
endif()
foreach(case IN ITEMS truncated.hog duplicate.hog oversized.rl2)
    execute_process(COMMAND "${INSPECT}" "${OUTPUT}/${case}" "${OUTPUT}/report.json"
                    RESULT_VARIABLE result ERROR_VARIABLE error)
    file(READ "${OUTPUT}/report.json" unchanged)
    if(NOT result EQUAL 2 OR NOT unchanged STREQUAL report)
        message(FATAL_ERROR "Invalid ${case} replaced an existing complete report: ${error}")
    endif()
endforeach()
execute_process(COMMAND "${INSPECT}" "${OUTPUT}/large-level.hog" "${OUTPUT}/partial.json"
                RESULT_VARIABLE result)
file(READ "${OUTPUT}/partial.json" partial)
string(
    JSON
    status
    GET
    "${partial}"
    levels
    0
    inventory
    status)
string(
    JSON
    version
    GET
    "${partial}"
    levels
    1
    inventory
    level_version)
if(NOT result EQUAL 1 OR NOT status STREQUAL "rejected" OR NOT version EQUAL 27)
    message(FATAL_ERROR "Per-level size limit was lost or prevented inspection of the next member")
endif()
execute_process(COMMAND "${INSPECT}" "${OUTPUT}/stream.hog" "${OUTPUT}/stream.hog"
                RESULT_VARIABLE result ERROR_VARIABLE error)
file(SHA256 "${OUTPUT}/stream.hog" after_hash)
if(NOT result EQUAL 2 OR NOT after_hash STREQUAL source_hash)
    message(FATAL_ERROR "Inspector overwrote its input")
endif()
message(
    STATUS
        "Streaming HOG, per-level bounds, malformed tail, duplicate, stable-report and source controls passed"
)
