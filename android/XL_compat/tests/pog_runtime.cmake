file(SHA256 "${HOG}" hog_hash)
file(SHA256 "${DESCRIPTOR}" descriptor_hash)
if(NOT hog_hash STREQUAL "adbfea019c1ac29ac922e15876c22ca6ce30eb02d598a98e6ef7dfdce2b2dd3e"
   OR NOT descriptor_hash STREQUAL
      "14150755f14e210b284aa03c45d75e1b2f96be7b567bbc324bf4986b96928a71")
    message(FATAL_ERROR "POG runtime requires the pinned original Arena resources")
endif()
set(originals "${HOG}")
set(pumo_args)
if(PUMO)
    file(SHA256 "${PUMO}" pumo_hash)
    if(NOT pumo_hash STREQUAL "6e3e723e549897188a8fdc19db8b04630de9d428d2b31f8e3f48af108915b043")
        message(FATAL_ERROR "Pumo POG controls require the unchanged original HOG")
    endif()
    list(APPEND originals "${PUMO}")
    list(APPEND pumo_args -pumo-hog "${PUMO}")
endif()
file(MAKE_DIRECTORY "${OUTPUT}/missions" "${OUTPUT}/preview-user")
configure_file("${HOG}" "${OUTPUT}/missions/rtarena.hog" COPYONLY)
configure_file("${DESCRIPTOR}" "${OUTPUT}/missions/rtarena.mn2" COPYONLY)
execute_process(
    COMMAND "${ENGINE}" -pog-test rgba -xl-water-physics -hogdir "${DATA}" -extra-dir "${OUTPUT}"
            -userdir "${OUTPUT}/preview-user" ${pumo_args} -nosound -nomusic
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
    TIMEOUT 90)
file(WRITE "${OUTPUT}/pog-runtime.log" "${stdout}${stderr}")
if(NOT result EQUAL 0
   OR NOT stdout MATCHES "Original Arena RGBA: 19 installed replacements"
   OR NOT
      stdout
      MATCHES
      "POG bank transitions: original alien1/water/groupa indexed headers and pixels restored exactly"
   OR NOT stdout MATCHES "48 actual FVI rays and 48 destructible texture hits"
   OR NOT stdout MATCHES "POG admission: truncated pixels")
    message(FATAL_ERROR "POG runtime incomplete (${result}): ${stdout}${stderr}")
endif()
if(PUMO AND NOT stdout MATCHES "all five files and 26 replacements")
    message(FATAL_ERROR "Pumo original POG runtime controls were omitted")
endif()
if(NOT REFERENCE)
    message(FATAL_ERROR "POG original resources require the pinned source comparison")
endif()
execute_process(
    COMMAND "${REFERENCE}" ${originals}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE source_out
    ERROR_VARIABLE source_err
    TIMEOUT 60)
file(WRITE "${OUTPUT}/pog-source.log" "${source_out}${source_err}")
if(NOT result EQUAL 0)
    message(FATAL_ERROR "POG source comparison failed (${result}): ${source_out}${source_err}")
endif()
if(GPU)
    execute_process(
        COMMAND "${GPU}" ${originals}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE gpu_out
        ERROR_VARIABLE gpu_err
        TIMEOUT 60)
    file(WRITE "${OUTPUT}/pog-gpu.log" "${gpu_out}${gpu_err}")
    if(NOT result EQUAL 0 OR NOT gpu_out MATCHES "1 POG files, 19 complete uploads")
        message(
            FATAL_ERROR "Original POG GPU comparison incomplete (${result}): ${gpu_out}${gpu_err}")
    endif()
    if(PUMO AND NOT gpu_out MATCHES "5 POG files, 26 complete uploads")
        message(FATAL_ERROR "Pumo original GPU POG controls were omitted")
    endif()
endif()
file(SHA256 "${HOG}" final_hog)
if(NOT final_hog STREQUAL hog_hash)
    message(FATAL_ERROR "Original POG resources changed")
endif()
if(PUMO)
    file(SHA256 "${PUMO}" final_pumo)
    if(NOT final_pumo STREQUAL pumo_hash)
        message(FATAL_ERROR "Original Pumo HOG changed")
    endif()
endif()
message(STATUS "${stdout}${source_out}${gpu_out}")
