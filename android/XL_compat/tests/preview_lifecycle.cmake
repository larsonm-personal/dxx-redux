include("${CMAKE_CURRENT_LIST_DIR}/stock_probe.cmake")
configure_file("${fixture}/xlprobe.rl2" "${fixture}/stockprobe.rl2" COPYONLY)
execute_process(
    COMMAND
        "${PREVIEW}" -xl-static-light-preview -xl-source "${LEVEL}" -fixture-path
        "${fixture}/xlprobe.rl2" -hogdir "${DATA}" -extra-dir "${fixture}" -mission xlprobe -userdir
        "${fixture}/preview-user"
    RESULT_VARIABLE result
    TIMEOUT 60
    OUTPUT_FILE "${fixture}/preview.log"
    ERROR_FILE "${fixture}/preview-error.log")
if(NOT result EQUAL 0)
    message(FATAL_ERROR "XL preview lifecycle failed: ${result}; see ${fixture}")
endif()
file(READ "${fixture}/preview.log" report)
if(NOT report MATCHES "recover passed")
    message(FATAL_ERROR "XL preview lifecycle did not reach final assertion")
endif()
file(SHA256 "${LEVEL}" after_source_hash)
file(SHA256 "${fixture}/xlprobe.rl2" restored_hash)
file(SHA256 "${fixture}/stockprobe.rl2" fixture_hash)
if(NOT after_source_hash STREQUAL original_hash OR NOT restored_hash STREQUAL fixture_hash)
    message(FATAL_ERROR "Lifecycle test changed the source or failed to restore its fixture")
endif()
message(STATUS "XL preview lifecycle passed; rendering and gameplay remain unverified")
