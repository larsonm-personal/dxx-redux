if(NOT MODE)
    set(MODE dispatch)
endif()
if(NOT MODE STREQUAL "dispatch" AND NOT MODE STREQUAL "signed27")
    message(FATAL_ERROR "Unknown HXM test mode")
endif()
file(SHA256 "${HOG}" original_hash)
if(NOT original_hash STREQUAL "f0611f1428549373526dfca8ca5a1e1883ace910f4209a938698086d189919db")
    message(FATAL_ERROR "HXM controls require original Apocalyptic Factor XL")
endif()
file(MAKE_DIRECTORY "${OUTPUT}/missions" "${OUTPUT}/preview-user")
if(CAMERA_HOG)
    file(SHA256 "${CAMERA_HOG}" camera_hash)
    if(NOT camera_hash STREQUAL "df69c62aad949f2d92bc1bbea4b597623cd29d0a0f594cdcc3d66568d6ce482a")
        message(FATAL_ERROR "Camera controls require the original XL addon HOG")
    endif()
    file(MAKE_DIRECTORY "${OUTPUT}/data")
    configure_file("${CAMERA_HOG}" "${OUTPUT}/data/d2x-xl.hog" COPYONLY)
endif()
configure_file("${HOG}" "${OUTPUT}/missions/hxmtest.hog" COPYONLY)
file(WRITE "${OUTPUT}/missions/hxmtest.mn2"
     "d2x-name = Alarlof HXM control\nnum_levels = 1\nalarlof.rl2\n")
execute_process(
    COMMAND "${ENGINE}" -hxm-test "${MODE}" -hogdir "${DATA}" -extra-dir "${OUTPUT}" -userdir
            "${OUTPUT}/preview-user" -nosound -nomusic
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
    TIMEOUT 240)
file(WRITE "${OUTPUT}/controls.log" "${stdout}${stderr}")
if(MODE STREQUAL "signed27")
    set(required_output "Signed profile27 native combat and disk controls passed")
else()
    set(required_output "Original HXM normal startup")
endif()
if(NOT result EQUAL 0 OR NOT stdout MATCHES "${required_output}")
    message(FATAL_ERROR "HXM native controls failed: ${result}: ${stdout}${stderr}")
endif()
if(CAMERA_HOG)
    if(NOT stdout MATCHES "Original camera HXM native append"
       OR NOT stdout MATCHES "Native camera object initialization"
       OR NOT stdout MATCHES "Native camera/player contact: 8 ordered dispatch")
        message(FATAL_ERROR "Camera resource controls were not exercised")
    endif()
    file(SHA256 "${CAMERA_HOG}" final_camera_hash)
    if(NOT final_camera_hash STREQUAL camera_hash)
        message(FATAL_ERROR "Original camera addon archive changed")
    endif()
endif()
file(SHA256 "${HOG}" final_hash)
if(NOT final_hash STREQUAL original_hash)
    message(FATAL_ERROR "Original HXM archive changed")
endif()
if(REFERENCE)
    execute_process(COMMAND "${REFERENCE}" "${HOG}" RESULT_VARIABLE result OUTPUT_VARIABLE stdout
                    ERROR_VARIABLE stderr)
    file(WRITE "${OUTPUT}/reference.log" "${stdout}${stderr}")
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "HXM source comparison failed: ${stdout}${stderr}")
    endif()
endif()

if(REFERENCE AND MODE STREQUAL "dispatch")
    execute_process(COMMAND "${REFERENCE}" --animation "${OUTPUT}/robot-animation-samples.txt"
                    RESULT_VARIABLE result OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr)
    file(WRITE "${OUTPUT}/animation-reference.log" "${stdout}${stderr}")
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Robot animation source comparison failed: ${stdout}${stderr}")
    endif()
endif()

if(REFERENCE AND MODE STREQUAL "dispatch")
    execute_process(COMMAND "${REFERENCE}" --starts "${OUTPUT}/player-start-samples.txt"
                    RESULT_VARIABLE result OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr)
    file(WRITE "${OUTPUT}/player-start-reference.log" "${stdout}${stderr}")
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Player-start source comparison failed: ${stdout}${stderr}")
    endif()
endif()

if(WARP_HOG AND WARP_DATA AND WARP_ENGINE AND MODE STREQUAL "dispatch")
    file(SHA256 "${WARP_HOG}" warp_hash)
    if(NOT warp_hash STREQUAL "2054dca034ea9aac0d13c37a5867b18fa84d9b307b36702b56bcc9ed3a61b45c")
        message(FATAL_ERROR "Warp 5 controls require the unchanged original Warp Shock 2.0 HOG")
    endif()
    get_filename_component(warp_dir "${WARP_HOG}" DIRECTORY)
    set(warp_descriptor "${warp_dir}/wshock20.mn2")
    file(SHA256 "${warp_descriptor}" descriptor_hash)
    if(NOT descriptor_hash STREQUAL
       "e4ee4da1dfc16b48b91be3409216e73cf43dd74e6e20d1be40b6bb310fe5a42a")
        message(FATAL_ERROR "Warp 5 controls require the unchanged original descriptor")
    endif()
    configure_file("${WARP_HOG}" "${OUTPUT}/missions/wshock20.hog" COPYONLY)
    configure_file("${warp_descriptor}" "${OUTPUT}/missions/wshock20.mn2" COPYONLY)
    file(MAKE_DIRECTORY "${OUTPUT}/warp-user")
    foreach(warp_mode IN ITEMS warp5-warm warp5-cold)
        if(warp_mode STREQUAL "warp5-warm")
            set(proof_mode warm)
        else()
            set(proof_mode cold)
        endif()
        execute_process(
            COMMAND
                "${WARP_ENGINE}" -mission wshock20 -level 5 -hogdir "${WARP_DATA}" -extra-dir
                "${OUTPUT}" -route-confirm-user-dir "${OUTPUT}/warp-user" -route-confirm-json-out
                "${OUTPUT}/${warp_mode}.json" -xl-hxm-actor-proof "${proof_mode}" -nosound -nomusic
            RESULT_VARIABLE warp_result
            OUTPUT_VARIABLE warp_stdout
            ERROR_VARIABLE warp_stderr
            TIMEOUT 90)
        file(WRITE "${OUTPUT}/${warp_mode}.log" "${warp_stdout}${warp_stderr}")
        if(NOT EXISTS "${OUTPUT}/${warp_mode}.json")
            message(
                FATAL_ERROR
                    "Warp 5 ${warp_mode} emitted no proof: ${warp_result}: ${warp_stdout}${warp_stderr}"
            )
        endif()
        file(READ "${OUTPUT}/${warp_mode}.json" warp_proof)
        string(JSON warp_phase GET "${warp_proof}" phase)
        string(JSON warp_shields GET "${warp_proof}" shields_after)
        if(NOT warp_result EQUAL 0 OR NOT warp_phase STREQUAL "passed")
            message(
                FATAL_ERROR
                    "Warp 5 ${warp_mode} failed: ${warp_result}: ${warp_stdout}${warp_stderr}")
        endif()
        if(warp_mode STREQUAL "warp5-warm")
            set(warm_shields "${warp_shields}")
        elseif(NOT warp_shields STREQUAL warm_shields)
            message(FATAL_ERROR "Warp 5 fresh-process restore changed combat shields")
        endif()
    endforeach()
    file(SHA256 "${WARP_HOG}" final_warp_hash)
    if(NOT final_warp_hash STREQUAL warp_hash)
        message(FATAL_ERROR "Original Warp Shock 2.0 HOG changed")
    endif()
endif()
