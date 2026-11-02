file(SHA256 "${HOG}" original_hash)
if(NOT original_hash STREQUAL "4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303")
    message(FATAL_ERROR "Projectile controls require original Neptune")
endif()
file(MAKE_DIRECTORY "${OUTPUT}/missions" "${OUTPUT}/preview-user")
if(VERTIGO_HAM)
    file(SHA256 "${VERTIGO_HAM}" vertigo_hash)
    if(NOT vertigo_hash STREQUAL "faa67189ccf5c7d6cdc8e37fa009fb21a1d57ee2c6c212ce091642e4366e3c0f")
        message(FATAL_ERROR "Vertigo controls require the original robot extension")
    endif()
    file(MAKE_DIRECTORY "${OUTPUT}/data")
    configure_file("${VERTIGO_HAM}" "${OUTPUT}/data/d2x.ham" COPYONLY)
endif()
if(CAMERA_HOG)
    file(SHA256 "${CAMERA_HOG}" camera_hash)
    if(NOT camera_hash STREQUAL "df69c62aad949f2d92bc1bbea4b597623cd29d0a0f594cdcc3d66568d6ce482a")
        message(FATAL_ERROR "Camera controls require the original XL addon HOG")
    endif()
    file(MAKE_DIRECTORY "${OUTPUT}/data")
    configure_file("${CAMERA_HOG}" "${OUTPUT}/data/d2x-xl.hog" COPYONLY)
endif()
configure_file("${HOG}" "${OUTPUT}/missions/neptunxl.hog" COPYONLY)
configure_file("${DESCRIPTOR}" "${OUTPUT}/missions/neptunxl.mn2" COPYONLY)
execute_process(
    COMMAND "${ENGINE}" -projectile-test dispatch -hogdir "${DATA}" -extra-dir "${OUTPUT}" -userdir
            "${OUTPUT}/preview-user" -nosound -nomusic
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
    TIMEOUT 120)
file(WRITE "${OUTPUT}/controls.log" "${stdout}${stderr}")
if(VERTIGO_HAM AND NOT stdout MATCHES
                   "XL Vertigo extension rejection and mission reload passed; original=1")
    message(FATAL_ERROR "Original Vertigo extension control was not exercised: ${stdout}${stderr}")
endif()
if(NOT result EQUAL 0 OR NOT stdout MATCHES "XL projectile native controls passed")
    message(FATAL_ERROR "Projectile controls failed (${result}): ${stdout}${stderr}")
endif()
if(CAMERA_HOG)
    if(NOT stdout MATCHES "Native camera flash: 4 ordered/self impacts")
        message(FATAL_ERROR "Camera flash gameplay controls were not exercised")
    endif()
    if(NOT stdout MATCHES "Native passive camera physics: 48 robot/fireball frames")
        message(FATAL_ERROR "Camera robot/fireball physics controls were not exercised")
    endif()
    if(NOT stdout MATCHES "Native camera projectile contacts: 56")
        message(FATAL_ERROR "Camera weapon controls were not exercised")
    endif()
    if(NOT stdout MATCHES "Native camera projectile physics: 256 frames, 240 deaths, 16 retries")
        message(FATAL_ERROR "Camera primary and missile physics controls were not exercised")
    endif()
    if(NOT stdout MATCHES "Native camera radial robot contacts: 4800 passed")
        message(FATAL_ERROR "Camera radial robot controls were not exercised")
    endif()
    if(NOT stdout MATCHES "xl_motion=1" OR NOT stdout MATCHES
                                           "Native camera explosion motion samples: 2160")
        message(
            FATAL_ERROR
                "Active camera player contact or explosion motion controls were not exercised")
    endif()
    if(EFFECT_REFERENCE)
        execute_process(
            COMMAND "${EFFECT_REFERENCE}" --camera-splash-rotation "${OUTPUT}/camera-rotation.txt"
            RESULT_VARIABLE rotation_result
            OUTPUT_VARIABLE rotation_stdout
            ERROR_VARIABLE rotation_stderr
            TIMEOUT 60)
        file(WRITE "${OUTPUT}/source-rotation-reference.log" "${rotation_stdout}${rotation_stderr}")
        if(NOT rotation_result EQUAL 0)
            message(
                FATAL_ERROR
                    "Camera rotation source comparison failed: ${rotation_stdout}${rotation_stderr}"
            )
        endif()
    endif()
    if(EFFECT_REFERENCE)
        execute_process(
            COMMAND "${EFFECT_REFERENCE}" --camera-explosion-motion "${OUTPUT}/camera-motion.txt"
            RESULT_VARIABLE motion_result
            OUTPUT_VARIABLE motion_stdout
            ERROR_VARIABLE motion_stderr
            TIMEOUT 60)
        file(WRITE "${OUTPUT}/source-motion-reference.log" "${motion_stdout}${motion_stderr}")
        if(NOT motion_result EQUAL 0)
            message(
                FATAL_ERROR
                    "Camera explosion motion source comparison failed: ${motion_stdout}${motion_stderr}"
            )
        endif()
    endif()
    if(NOT stdout MATCHES "Native camera controller frames: 10800 passed")
        message(FATAL_ERROR "Native camera controller was not exercised")
    endif()
    if(NOT stdout MATCHES "Native camera trigger identity profiles: 7 passed")
        message(FATAL_ERROR "Native camera trigger identities were not exercised")
    endif()
    if(NOT stdout MATCHES "Native camera object record restores: 9 passed")
        message(FATAL_ERROR "Native camera object restore was not exercised")
    endif()
    if(NOT stdout MATCHES "Native original model hitbox caches: 2 models, 25 queries")
        message(FATAL_ERROR "Native original model hitbox caches were not exercised")
    endif()
    if(HXM_REFERENCE)
        foreach(model IN ITEMS camera player)
            execute_process(
                COMMAND "${HXM_REFERENCE}" --model "${OUTPUT}/${model}-model.bin"
                RESULT_VARIABLE model_result
                OUTPUT_VARIABLE model_stdout
                ERROR_VARIABLE model_stderr
                TIMEOUT 60)
            file(WRITE "${OUTPUT}/${model}-hitbox-reference.log" "${model_stdout}${model_stderr}")
            if(NOT model_result EQUAL 0)
                message(
                    FATAL_ERROR
                        "Native ${model} model hitbox reference failed: ${model_stdout}${model_stderr}"
                )
            endif()
        endforeach()
    endif()
    if(EFFECT_REFERENCE)
        execute_process(
            COMMAND "${EFFECT_REFERENCE}" --camera-controller "${OUTPUT}/camera-controller.txt"
            RESULT_VARIABLE controller_result
            OUTPUT_VARIABLE controller_stdout
            ERROR_VARIABLE controller_stderr
            TIMEOUT 60)
        file(WRITE "${OUTPUT}/source-controller-reference.log"
             "${controller_stdout}${controller_stderr}")
        if(NOT controller_result EQUAL 0)
            message(
                FATAL_ERROR
                    "Camera controller source comparison failed: ${controller_stdout}${controller_stderr}"
            )
        endif()
    endif()
    file(SHA256 "${CAMERA_HOG}" final_camera_hash)
    if(NOT final_camera_hash STREQUAL camera_hash)
        message(FATAL_ERROR "Original camera addon changed")
    endif()
endif()
if(REFERENCE)
    execute_process(
        COMMAND "${REFERENCE}" "${OUTPUT}/projectile-impacts.txt"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE stdout
        ERROR_VARIABLE stderr
        TIMEOUT 30)
    file(WRITE "${OUTPUT}/reference.log" "${stdout}${stderr}")
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Projectile reference controls failed (${result}): ${stdout}${stderr}")
    endif()
endif()
file(SHA256 "${HOG}" final_hash)
if(NOT original_hash STREQUAL final_hash)
    message(FATAL_ERROR "Original Neptune changed during projectile controls")
endif()
