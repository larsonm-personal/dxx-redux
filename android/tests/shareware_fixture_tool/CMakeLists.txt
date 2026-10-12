cmake_minimum_required(VERSION 3.16)
project(dxx-shareware-fixture-tool C)

# Build the existing split-volume CLI without the unrelated CD/GOG test dependencies
set(EXTRACT_DIR "${CMAKE_CURRENT_LIST_DIR}/../../app/src/main/cpp/extract")
add_executable(test_sow_direct "${EXTRACT_DIR}/test_sow_direct.c" "${EXTRACT_DIR}/sow_extract.c"
                               "${EXTRACT_DIR}/extract_attempt_budget.c")
target_include_directories(test_sow_direct PRIVATE "${EXTRACT_DIR}")
if(MSVC)
    target_compile_definitions(test_sow_direct PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
