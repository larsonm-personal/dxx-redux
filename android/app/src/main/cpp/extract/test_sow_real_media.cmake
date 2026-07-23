if(NOT DEFINED RETAIL_HOG_SHA)
    set(RETAIL_HOG_SHA "f1abf516512739c97b43e2e93611a2398fc9f8bc7a014095ebc2b6b2fd21b703")
endif()

set(required_fixtures "${RETAIL_SOW}" "${SPLIT_SOW_DIR}/d2_1.sow" "${SPLIT_SOW_DIR}/d2_2.sow"
                      "${SPLIT_SOW_DIR}/d2_3.sow")
foreach(fixture IN LISTS required_fixtures)
    if(NOT EXISTS "${fixture}")
        message("SKIP: SOW real-media fixtures unavailable: ${fixture}")
        return()
    endif()
endforeach()

function(run_sow archive output_dir expected_count append)
    set(command "${TEST_SOW_DIRECT}" "${archive}" "${output_dir}")
    if(append)
        list(APPEND command --append)
    endif()
    execute_process(COMMAND ${command} RESULT_VARIABLE result ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "SOW extraction failed for ${archive}: ${error}")
    endif()
    if(NOT error MATCHES "Extracted ${expected_count} files")
        message(
            FATAL_ERROR
                "SOW extraction count mismatch for ${archive}: expected ${expected_count}\n${error}"
        )
    endif()
endfunction()

function(assert_sha256 path expected)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Expected SOW output is missing: ${path}")
    endif()
    file(SHA256 "${path}" actual)
    if(NOT actual STREQUAL expected)
        message(
            FATAL_ERROR "SOW output hash mismatch for ${path}: expected ${expected}, got ${actual}")
    endif()
endfunction()

function(assert_file_count directory expected)
    file(GLOB outputs LIST_DIRECTORIES FALSE "${directory}/*")
    list(LENGTH outputs actual)
    if(NOT actual EQUAL expected)
        message(
            FATAL_ERROR
                "SOW output count mismatch for ${directory}: expected ${expected}, got ${actual}")
    endif()
endfunction()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}/retail" "${WORK_DIR}/split")

run_sow("${RETAIL_SOW}" "${WORK_DIR}/retail" 34 FALSE)
assert_file_count("${WORK_DIR}/retail" 34)
assert_sha256("${WORK_DIR}/retail/DESCENT2.HOG" "${RETAIL_HOG_SHA}")
assert_sha256("${WORK_DIR}/retail/DESCENT2.HAM"
              "5233242206c677d65db7f075dd61f2b0a1b7bbe8cd65f56d769efaee1cc38b4d")
assert_sha256("${WORK_DIR}/retail/GROUPA.PIG"
              "facdde6cf8a2cab99ea39ba06931872a1fe5636fe211e61fb58c57d706bf627b")

run_sow("${SPLIT_SOW_DIR}/d2_1.sow" "${WORK_DIR}/split" 8 TRUE)
run_sow("${SPLIT_SOW_DIR}/d2_2.sow" "${WORK_DIR}/split" 2 TRUE)
run_sow("${SPLIT_SOW_DIR}/d2_3.sow" "${WORK_DIR}/split" 17 TRUE)
assert_file_count("${WORK_DIR}/split" 25)
assert_sha256("${WORK_DIR}/split/D2DEMO.DEM"
              "8c6e2d43ba88166d17759d90e3817edd0c3ef0a33861ef35a51a8cd4db89c892")
assert_sha256("${WORK_DIR}/split/D2DEMO.HAM"
              "747ccf2494916892061e13601cd8695c35e46f2a99062fff3e3f298da94b9be6")
assert_sha256("${WORK_DIR}/split/D2DEMO.HOG"
              "ccdf88722d90ea4a7ebb40f75fddb71b4c6a68b2a0bee10e82b4fcf887973478")
assert_sha256("${WORK_DIR}/split/D2DEMO.PIG"
              "368f9ea56fe8eb8b6e4636ab5eba60bfffdf692fe10100d604fedf654d7d8989")

file(REMOVE_RECURSE "${WORK_DIR}")
message("SOW real-media oracle tests passed")
