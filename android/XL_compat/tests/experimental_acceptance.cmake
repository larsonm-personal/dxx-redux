# Emit only a fixed classification for the known ordinary red-route assertion
# The caller still fails the test; crashes, timeouts and other assertions get no marker
function(xl_report_experimental_route_failure result fixture)
    if(NOT "${result}" STREQUAL "1"
       OR NOT EXISTS "${fixture}/preview.log"
       OR NOT EXISTS "${fixture}/preview-error.log")
        return()
    endif()
    file(STRINGS "${fixture}/preview-error.log" failures REGEX "^XL lifecycle FAILED:")
    list(LENGTH failures failure_count)
    if(NOT failure_count EQUAL 1
       OR NOT "${failures}" STREQUAL "XL lifecycle FAILED: level-two ordinary key route incomplete")
        return()
    endif()
    file(STRINGS "${fixture}/preview.log" blue
         REGEX "^Level two ordinary blue key: frames=[1-9][0-9]* alive=1 checkpoint=restored$")
    file(STRINGS "${fixture}/preview.log" red
         REGEX "^Level two route result: key=5 frames=[1-9][0-9]* dead=[01] shields=-?[0-9]+ complete=0$")
    list(LENGTH blue blue_count)
    list(LENGTH red red_count)
    if(blue_count EQUAL 1 AND red_count EQUAL 1)
        message("XL_EXPERIMENTAL_ACCEPTANCE: xl_level_two_keys route_incomplete")
    endif()
endfunction()
