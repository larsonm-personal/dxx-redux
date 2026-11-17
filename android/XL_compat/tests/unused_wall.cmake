file(SHA256 "${EXTRA}/missions/wshock20.hog" original_hash)
if(NOT original_hash STREQUAL "2054dca034ea9aac0d13c37a5867b18fa84d9b307b36702b56bcc9ed3a61b45c")
    message(FATAL_ERROR "Unused-wall controls require original Warp Shock 2.0")
endif()
file(SHA256 "${DATA}/d2x.hog" vertigo_hash)
file(SHA256 "${DATA}/d2x-xl.hog" addon_hash)
if(NOT vertigo_hash STREQUAL "97a4433833ec1f2fbdd2becc144a1e9726ca78b1210a71a49ddbec6182665fe6"
   OR NOT addon_hash STREQUAL "e7351d66d534c3fdb400f03b91b771ad2d0c0d2b0100cf48718d504008ada7e6")
    message(FATAL_ERROR "Unused-wall controls require pinned authentic Vertigo and XL resources")
endif()
function(input_identity result)
    file(GLOB_RECURSE inputs LIST_DIRECTORIES false "${EXTRA}/missions/*" "${EXTRA}/mods/*")
    list(SORT inputs)
    set(identity "")
    foreach(input IN LISTS inputs)
        if(IS_SYMLINK "${input}")
            message(FATAL_ERROR "Unused-wall inputs must be regular files")
        endif()
        file(SHA256 "${input}" hash)
        file(RELATIVE_PATH relative "${EXTRA}" "${input}")
        string(APPEND identity "${relative}=${hash}\n")
    endforeach()
    string(SHA256 hash "${identity}")
    set(${result} "${hash}" PARENT_SCOPE)
endfunction()
input_identity(inputs_before)
# Keep earlier failed or successful receipts when this optional test is repeated
string(RANDOM LENGTH 16 ALPHABET 0123456789abcdef run_id)
set(run_output "${OUTPUT}/${run_id}")
file(MAKE_DIRECTORY "${run_output}/preview-user")
file(COPY "${EXTRA}/missions" "${EXTRA}/mods" DESTINATION "${run_output}")
file(SHA256 "${ENGINE}" engine_hash)
execute_process(
    COMMAND "${ENGINE}" -unused-wall-test 1 -hogdir "${DATA}" -extra-dir "${run_output}" -userdir
            "${run_output}/preview-user" -nosound -nomusic
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
    TIMEOUT 120)
file(WRITE "${run_output}/engine.log" "${stdout}${stderr}")
input_identity(inputs_after)
file(
    WRITE "${run_output}/identity.json"
    "{\n  \"engine_sha256\": \"${engine_hash}\",\n  \"hog_sha256\": \"${original_hash}\",\n  \"input_tree_before\": \"${inputs_before}\",\n  \"input_tree_after\": \"${inputs_after}\",\n  \"exit_code\": \"${result}\"\n}\n"
)
message(STATUS "Original unused-wall receipts: ${run_output}")
if(NOT result EQUAL 0 OR NOT inputs_before STREQUAL inputs_after)
    message(FATAL_ERROR "Original unused-wall lifecycle failed (${result}): ${stderr}")
endif()
file(READ "${run_output}/unused-wall.json" receipt)
foreach(control future_equal cold_cloak_restore illusion_transitions cloak_transitions
                raw_clips_preserved)
    string(JSON passed GET "${receipt}" "${control}")
    if(NOT passed)
        message(FATAL_ERROR "Original unused-wall control did not complete: ${control}")
    endif()
endforeach()
