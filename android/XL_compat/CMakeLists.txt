cmake_minimum_required(VERSION 3.21)
project(dxx_xl_compat LANGUAGES CXX)

if(ANDROID)
    add_subdirectory(android)
endif()

add_library(xl_inventory STATIC inventory.cpp probe.cpp compatibility.cpp lighting.cpp)
target_compile_features(xl_inventory PUBLIC cxx_std_17)
target_include_directories(xl_inventory PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
add_executable(xl_inspect inspect.cpp)
target_link_libraries(xl_inspect PRIVATE xl_inventory)
add_executable(xl_probe_fixture probe_main.cpp)
target_link_libraries(xl_probe_fixture PRIVATE xl_inventory)
if(MSVC)
    target_compile_options(xl_inventory PRIVATE /W4 /WX)
    target_compile_options(xl_inspect PRIVATE /W4 /WX)
    target_compile_options(xl_probe_fixture PRIVATE /W4 /WX)
else()
    target_compile_options(xl_inventory PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(xl_inspect PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(xl_probe_fixture PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

include(CTest)
if(BUILD_TESTING)
    set(XL_REFERENCE_RENDERLIB "" CACHE FILEPATH
                                        "Pinned local XL renderlib.cpp for arithmetic oracle")
    if(XL_REFERENCE_RENDERLIB)
        include(tests/reference_lighting.cmake)
    endif()
    set(XL_REFERENCE_LIGHT "" CACHE FILEPATH
                                    "Pinned XL lighting/light.cpp for dynamic color oracle")
    if(XL_REFERENCE_LIGHT)
        include(tests/reference_dynamic.cmake)
    endif()
    add_executable(xl_lighting_test tests/lighting_test.cpp)
    target_link_libraries(xl_lighting_test PRIVATE xl_inventory)
    add_test(NAME xl_lighting_calculation COMMAND xl_lighting_test)
    add_executable(xl_inventory_test tests/inventory_test.cpp)
    target_link_libraries(xl_inventory_test PRIVATE xl_inventory)
    add_test(NAME xl_inventory_contract COMMAND xl_inventory_test)
    set(XL_PILOT_HOG "" CACHE FILEPATH
                              "Original D2-XL Counterstrike HOG for private integration coverage")
    set(XL_PILOT_LEVEL "" CACHE FILEPATH "Original d2leva-1.rl2 for stock loader probe")
    set(XL_PILOT_DESCRIPTOR "" CACHE FILEPATH "Original outer d2-xl.mn2 for mission/save testing")
    set(XL_STOCK_METADATA "" CACHE FILEPATH "Unmodified D2 headless metadata executable")
    set(XL_STOCK_ROUTE "" CACHE FILEPATH "Unmodified D2 headless route diagnostic executable")
    set(XL_RETAIL_DATA "" CACHE PATH "Private retail D2 data directory")
    set(XL_PREVIEW_LIFECYCLE "" CACHE FILEPATH "Isolated XL preview lifecycle executable")
    set(XL_ARENA_HOG "" CACHE FILEPATH "Original Robotech Arena HOG for private water coverage")
    set(XL_ARENA_DESCRIPTOR "" CACHE FILEPATH "Original Robotech Arena mission descriptor")
    if(XL_ARENA_HOG AND XL_ARENA_DESCRIPTOR AND XL_PREVIEW_LIFECYCLE AND XL_RETAIL_DATA)
        add_test(
            NAME xl_water_arena
            COMMAND
                ${CMAKE_COMMAND} -DHOG=${XL_ARENA_HOG} -DDESCRIPTOR=${XL_ARENA_DESCRIPTOR}
                -DENGINE=${XL_PREVIEW_LIFECYCLE} -DDATA=${XL_RETAIL_DATA}
                -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/water-arena -P
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/water_arena.cmake)
        set_tests_properties(xl_water_arena PROPERTIES TIMEOUT 120)
    endif()
    if(XL_PREVIEW_LIFECYCLE AND XL_PILOT_LEVEL AND XL_STOCK_METADATA AND XL_RETAIL_DATA)
        add_test(
            NAME xl_preview_lifecycle
            COMMAND
                ${CMAKE_COMMAND} -DLEVEL=${XL_PILOT_LEVEL} -DPROBE=$<TARGET_FILE:xl_probe_fixture>
                -DENGINE=${XL_STOCK_METADATA} -DDATA=${XL_RETAIL_DATA}
                -DPREVIEW=${XL_PREVIEW_LIFECYCLE} -DPILOT_HOG=${XL_PILOT_HOG}
                -DPILOT_DESCRIPTOR=${XL_PILOT_DESCRIPTOR}
                -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/preview-tests -P
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/preview_lifecycle.cmake)
        if(XL_PILOT_DESCRIPTOR AND XL_PILOT_HOG)
            add_test(
                NAME xl_normal_red_key
                COMMAND
                    ${CMAKE_COMMAND} -DNAVIGATION=ON -DLEVEL=${XL_PILOT_LEVEL}
                    -DPROBE=$<TARGET_FILE:xl_probe_fixture> -DENGINE=${XL_STOCK_METADATA}
                    -DDATA=${XL_RETAIL_DATA} -DPREVIEW=${XL_PREVIEW_LIFECYCLE}
                    -DPILOT_HOG=${XL_PILOT_HOG} -DPILOT_DESCRIPTOR=${XL_PILOT_DESCRIPTOR}
                    -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/navigation-tests -P
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/preview_lifecycle.cmake)
            set_tests_properties(xl_normal_red_key PROPERTIES TIMEOUT 90)
            add_test(
                NAME xl_normal_reactor
                COMMAND
                    ${CMAKE_COMMAND} -DNAVIGATION=ON -DREACTOR=ON -DLEVEL=${XL_PILOT_LEVEL}
                    -DPROBE=$<TARGET_FILE:xl_probe_fixture> -DENGINE=${XL_STOCK_METADATA}
                    -DDATA=${XL_RETAIL_DATA} -DPREVIEW=${XL_PREVIEW_LIFECYCLE}
                    -DPILOT_HOG=${XL_PILOT_HOG} -DPILOT_DESCRIPTOR=${XL_PILOT_DESCRIPTOR}
                    -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/reactor-tests -P
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/preview_lifecycle.cmake)
            set_tests_properties(xl_normal_reactor PROPERTIES TIMEOUT 90)
            add_test(
                NAME xl_normal_escape
                COMMAND
                    ${CMAKE_COMMAND} -DNAVIGATION=ON -DREACTOR=ON -DESCAPE=ON
                    -DLEVEL=${XL_PILOT_LEVEL} -DPROBE=$<TARGET_FILE:xl_probe_fixture>
                    -DENGINE=${XL_STOCK_METADATA} -DDATA=${XL_RETAIL_DATA}
                    -DPREVIEW=${XL_PREVIEW_LIFECYCLE} -DPILOT_HOG=${XL_PILOT_HOG}
                    -DPILOT_DESCRIPTOR=${XL_PILOT_DESCRIPTOR}
                    -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/escape-tests -P
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/preview_lifecycle.cmake)
            set_tests_properties(xl_normal_escape PROPERTIES TIMEOUT 90)
            foreach(
                branch IN
                ITEMS 1
                      2
                      3
                      4
                      5
                      6
                      7
                      8
                      9
                      10
                      11
                      12)
                add_test(
                    NAME xl_normal_branch_${branch}
                    COMMAND
                        ${CMAKE_COMMAND} -DNAVIGATION=ON -DBRANCH=${branch}
                        -DLEVEL=${XL_PILOT_LEVEL} -DPROBE=$<TARGET_FILE:xl_probe_fixture>
                        -DENGINE=${XL_STOCK_METADATA} -DDATA=${XL_RETAIL_DATA}
                        -DPREVIEW=${XL_PREVIEW_LIFECYCLE} -DPILOT_HOG=${XL_PILOT_HOG}
                        -DPILOT_DESCRIPTOR=${XL_PILOT_DESCRIPTOR}
                        -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/branch-${branch}-tests -P
                        ${CMAKE_CURRENT_SOURCE_DIR}/tests/preview_lifecycle.cmake)
                set_tests_properties(xl_normal_branch_${branch} PROPERTIES TIMEOUT 90)
            endforeach()
        endif()
    endif()
    if(XL_PILOT_LEVEL AND XL_STOCK_METADATA AND XL_RETAIL_DATA)
        add_test(
            NAME xl_stock_loader_probe
            COMMAND
                ${CMAKE_COMMAND} -DPROBE=$<TARGET_FILE:xl_probe_fixture> -DLEVEL=${XL_PILOT_LEVEL}
                -DENGINE=${XL_STOCK_METADATA} -DDATA=${XL_RETAIL_DATA}
                -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/stock-probes -P
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/stock_probe.cmake)
        set_tests_properties(xl_stock_loader_probe PROPERTIES TIMEOUT 60)
        if(XL_STOCK_ROUTE)
            add_test(
                NAME xl_route_checkpoint_probe
                COMMAND
                    ${CMAKE_COMMAND} -DPROBE=$<TARGET_FILE:xl_probe_fixture>
                    -DLEVEL=${XL_PILOT_LEVEL} -DENGINE=${XL_STOCK_METADATA}
                    -DROUTE=${XL_STOCK_ROUTE} -DDATA=${XL_RETAIL_DATA}
                    -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/route-probes -P
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/route_probe.cmake)
            set_tests_properties(xl_route_checkpoint_probe PROPERTIES TIMEOUT 180)
        endif()
    endif()
    if(XL_PILOT_HOG)
        add_test(
            NAME xl_pilot_archive
            COMMAND
                ${CMAKE_COMMAND} -DINSPECT=$<TARGET_FILE:xl_inspect> -DHOG=${XL_PILOT_HOG}
                -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/pilot-inventory.json -P
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/pilot_archive.cmake)
    endif()
endif()
