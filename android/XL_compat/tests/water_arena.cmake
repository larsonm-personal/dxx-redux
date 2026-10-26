# Private bytes stay outside the repository; hashes pin original resources too
file(SHA256 "${HOG}" hog_hash)
file(SHA256 "${DESCRIPTOR}" descriptor_hash)
if(NOT hog_hash STREQUAL "adbfea019c1ac29ac922e15876c22ca6ce30eb02d598a98e6ef7dfdce2b2dd3e"
   OR NOT descriptor_hash STREQUAL
      "14150755f14e210b284aa03c45d75e1b2f96be7b567bbc324bf4986b96928a71")
    message(FATAL_ERROR "Water arena requires the audited original HOG and descriptor")
endif()
file(MAKE_DIRECTORY "${OUTPUT}/missions" "${OUTPUT}/preview-user")
# Match the Android qualification viewport when recording and replaying controls
file(WRITE "${OUTPUT}/preview-user/descent.cfg"
     "ResolutionX=1920\nResolutionY=1080\nAspectX=9\nAspectY=16\n")
configure_file("${HOG}" "${OUTPUT}/missions/rtarena.hog" COPYONLY)
configure_file("${DESCRIPTOR}" "${OUTPUT}/missions/rtarena.mn2" COPYONLY)
execute_process(
    COMMAND "${ENGINE}" -arena-test water -xl-water-physics -hogdir "${DATA}" -extra-dir "${OUTPUT}"
            -userdir "${OUTPUT}/preview-user" -mission rtarena -nosound -nomusic
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
    TIMEOUT 90)
file(WRITE "${OUTPUT}/water.log" "${stdout}${stderr}")
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Water arena failed (${result}): ${stdout}${stderr}")
endif()
if(NOT stdout MATCHES "Arena spawn input recording: segment=")
    message(FATAL_ERROR "Water spawn input recording was not exported")
endif()
if(NOT stdout MATCHES
   "Mission requirement snapshot: source edits cannot change admission until reselection")
    message(FATAL_ERROR "Owned descriptor requirement controls did not finish")
endif()
execute_process(
    COMMAND
        "${ENGINE}" -arena-test water -arena-replay "${OUTPUT}/preview-user/xl-water-spawn.dximdemo"
        -xl-water-physics -hogdir "${DATA}" -extra-dir "${OUTPUT}" -userdir "${OUTPUT}/preview-user"
        -nosound -nomusic
    RESULT_VARIABLE replay_result
    OUTPUT_VARIABLE replay_stdout
    ERROR_VARIABLE replay_stderr
    TIMEOUT 60)
file(WRITE "${OUTPUT}/spawn-replay.log" "${replay_stdout}${replay_stderr}")
if(NOT replay_result EQUAL 0
   OR NOT replay_stdout MATCHES "Input demo replay result matched embedded trailer"
   OR replay_stdout MATCHES "mismatch"
   OR NOT replay_stdout MATCHES "Arena spawn replay completed through native ordered input")
    message(
        FATAL_ERROR "Water spawn replay failed (${replay_result}): ${replay_stdout}${replay_stderr}"
    )
endif()
if(NOT
   stdout
   MATCHES
   "XL base sound banks: identical sources accepted, changed banks rejected before resource mutation, recovery passed"
)
    message(FATAL_ERROR "Base sound resource rejection/recovery coverage did not finish")
endif()
file(SHA256 "${HOG}" final_hog_hash)
if(NOT
   stdout
   MATCHES
   "XL graphics sources: identical copies accepted, changed palette/PIG rejected before mutation, native recovery passed"
)
    message(FATAL_ERROR "Graphics resource rejection/recovery coverage did not finish")
endif()
file(SHA256 "${DESCRIPTOR}" final_descriptor_hash)
if(NOT
   stdout
   MATCHES
   "Unqualified water/robot capability rejected, active water preserved, original admission recovered"
)
    message(FATAL_ERROR "Rejected water candidate discarded the active profile")
endif()
if(NOT final_hog_hash STREQUAL hog_hash OR NOT final_descriptor_hash STREQUAL descriptor_hash)
    message(FATAL_ERROR "Original arena sources changed during the test")
endif()
message(STATUS "${stdout}")
