cmake_minimum_required(VERSION 3.21)
project(dxx_xl_compat LANGUAGES CXX)

if(ANDROID)
    add_subdirectory(android)
endif()

add_library(
    xl_inventory STATIC
    inventory.cpp
    probe.cpp
    compatibility.cpp
    lighting.cpp
    mission_label.cpp
    admission.cpp
    wave.cpp
    companion_lighting.cpp
    scoped_sound.cpp)
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
    add_executable(xl_static_pixels_test tests/static_pixels_test.cpp)
    target_compile_features(xl_static_pixels_test PRIVATE cxx_std_17)
    add_test(NAME xl_static_pixels_contract COMMAND xl_static_pixels_test)
    add_executable(xl_scoped_texture_test tests/scoped_texture_test.cpp)
    target_link_libraries(xl_scoped_texture_test PRIVATE xl_inventory)
    add_test(NAME xl_scoped_texture_contract COMMAND xl_scoped_texture_test)
    add_executable(xl_companion_lighting_test tests/companion_lighting_test.cpp)
    target_link_libraries(xl_companion_lighting_test PRIVATE xl_inventory)
    add_test(NAME xl_companion_lighting_contract COMMAND xl_companion_lighting_test)
    set(XL_DRAGON_HOG "" CACHE FILEPATH "Private original Dragon Pit HOG")
    if(XL_DRAGON_HOG)
        add_test(
            NAME xl_dragon_lighting_decode
            COMMAND
                ${CMAKE_COMMAND} -DHOG=${XL_DRAGON_HOG}
                -DTEST=$<TARGET_FILE:xl_companion_lighting_test> -P
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/dragon_lighting.cmake)
    endif()
    add_executable(xl_wave_test tests/wave_test.cpp)
    target_link_libraries(xl_wave_test PRIVATE xl_inventory)
    add_test(NAME xl_wave_contract COMMAND xl_wave_test)
    set(XL_DRAGON_SOUNDS "" CACHE PATH "Private original Dragon Pit sound directory")
    if(XL_DRAGON_SOUNDS)
        add_test(
            NAME xl_dragon_wave_decode
            COMMAND
                ${CMAKE_COMMAND} -DSOUNDS=${XL_DRAGON_SOUNDS} -DTEST=$<TARGET_FILE:xl_wave_test> -P
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/dragon_wave.cmake)
    endif()
    add_executable(xl_mission_label_test tests/mission_label_test.cpp)
    target_link_libraries(xl_mission_label_test PRIVATE xl_inventory)
    add_test(NAME xl_mission_labels COMMAND xl_mission_label_test)
    set(XL_REFERENCE_RENDERLIB "" CACHE FILEPATH
                                        "Pinned local XL renderlib.cpp for arithmetic oracle")
    if(XL_REFERENCE_RENDERLIB)
        include(tests/reference_lighting.cmake)
    endif()
    set(XL_REFERENCE_SEGMENT_FLAGS ""
        CACHE FILEPATH "Pinned XL main/findsegment.cpp for ambient flag oracle")
    if(XL_REFERENCE_SEGMENT_FLAGS)
        include(tests/reference_segment_flags.cmake)
    endif()
    set(XL_REFERENCE_LIGHT "" CACHE FILEPATH
                                    "Pinned XL lighting/light.cpp for dynamic color oracle")
    if(XL_REFERENCE_LIGHT)
        include(tests/reference_dynamic.cmake)
    endif()
    add_executable(xl_lighting_test tests/lighting_test.cpp)
    set(XL_REFERENCE_GEOMETRY_ROOT "" CACHE PATH "Pinned XL source root for face hit-point oracle")
    if(XL_REFERENCE_GEOMETRY_ROOT)
        include(tests/reference_face_point.cmake)
    endif()
    set(XL_REFERENCE_LIGHT_PRECALC "" CACHE FILEPATH
                                            "Pinned XL lightprecalc.cpp for nearest-light oracle")
    if(XL_REFERENCE_LIGHT_PRECALC)
        include(tests/reference_nearest_lights.cmake)
    endif()
    set(XL_REFERENCE_OGL_COLOR ""
        CACHE FILEPATH "Pinned XL ogl/ogl_color.cpp for geometry attenuation oracle")
    if(XL_REFERENCE_OGL_COLOR)
        include(tests/reference_geometry_attenuation.cmake)
    endif()
    target_link_libraries(xl_lighting_test PRIVATE xl_inventory)
    add_test(NAME xl_lighting_calculation COMMAND xl_lighting_test)
    add_executable(xl_inventory_test tests/inventory_test.cpp)
    target_link_libraries(xl_inventory_test PRIVATE xl_inventory)
    add_test(NAME xl_inventory_contract COMMAND xl_inventory_test)
    set(XL_OBSERVATORY_HOG "" CACHE FILEPATH
                                    "Original Observatory XL HOG for private inspection coverage")
    if(XL_OBSERVATORY_HOG)
        add_test(
            NAME xl_observatory_inventory
            COMMAND
                ${CMAKE_COMMAND} -DHOG=${XL_OBSERVATORY_HOG}
                -DTEST=$<TARGET_FILE:xl_inventory_test> -P
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/observatory_inventory.cmake)
        set_tests_properties(xl_observatory_inventory PROPERTIES TIMEOUT 30)
    endif()
    set(XL_PILOT_HOG "" CACHE FILEPATH
                              "Original D2-XL Counterstrike HOG for private integration coverage")
    set(XL_PILOT_LEVEL "" CACHE FILEPATH "Original d2leva-1.rl2 for stock loader probe")
    set(XL_PILOT_DESCRIPTOR "" CACHE FILEPATH "Original outer d2-xl.mn2 for mission/save testing")
    set(XL_STOCK_METADATA "" CACHE FILEPATH "Unmodified D2 headless metadata executable")
    set(XL_STOCK_ROUTE "" CACHE FILEPATH "Unmodified D2 headless route diagnostic executable")
    set(XL_RETAIL_DATA "" CACHE PATH "Private retail D2 data directory")
    set(XL_PREVIEW_LIFECYCLE "" CACHE FILEPATH "Isolated XL preview lifecycle executable")
    set(XL_OBSERVATORY_DESCRIPTOR "" CACHE FILEPATH "Original Observatory mission descriptor")
    if(XL_DRAGON_HOG AND XL_PREVIEW_LIFECYCLE AND XL_RETAIL_DATA)
        add_test(
            NAME xl_dragon_companion_ownership
            COMMAND
                ${CMAKE_COMMAND} -DHOG=${XL_DRAGON_HOG} -DENGINE=${XL_PREVIEW_LIFECYCLE}
                -DDATA=${XL_RETAIL_DATA}
                -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/dragon-companion-ownership -P
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/dragon_companion_ownership.cmake)
        set_tests_properties(xl_dragon_companion_ownership PROPERTIES TIMEOUT 60)
    endif()
    if(XL_OBSERVATORY_HOG AND XL_OBSERVATORY_DESCRIPTOR AND XL_PREVIEW_LIFECYCLE AND XL_RETAIL_DATA)
        add_test(
            NAME xl_observatory_descriptor
            COMMAND
                ${CMAKE_COMMAND} -DHOG=${XL_OBSERVATORY_HOG}
                -DDESCRIPTOR=${XL_OBSERVATORY_DESCRIPTOR} -DENGINE=${XL_PREVIEW_LIFECYCLE}
                -DDATA=${XL_RETAIL_DATA}
                -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/observatory-descriptor -P
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/observatory_descriptor.cmake)
        set_tests_properties(xl_observatory_descriptor PROPERTIES TIMEOUT 90)
    endif()
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
            add_test(
                NAME xl_earned_campaign_transition
                COMMAND
                    ${CMAKE_COMMAND} -DNAVIGATION=ON -DREACTOR=ON -DESCAPE=ON -DCAMPAIGN=ON
                    -DLEVEL=${XL_PILOT_LEVEL} -DPROBE=$<TARGET_FILE:xl_probe_fixture>
                    -DENGINE=${XL_STOCK_METADATA} -DDATA=${XL_RETAIL_DATA}
                    -DPREVIEW=${XL_PREVIEW_LIFECYCLE} -DPILOT_HOG=${XL_PILOT_HOG}
                    -DPILOT_DESCRIPTOR=${XL_PILOT_DESCRIPTOR}
                    -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/campaign-tests -P
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/preview_lifecycle.cmake)
            set_tests_properties(xl_earned_campaign_transition PROPERTIES TIMEOUT 90)
            add_test(
                NAME xl_level_two_keys
                COMMAND
                    ${CMAKE_COMMAND} -DNAVIGATION=ON -DREACTOR=ON -DESCAPE=ON -DCAMPAIGN=ON
                    -DLEVEL_TWO_ROUTE=ON -DLEVEL=${XL_PILOT_LEVEL}
                    -DPROBE=$<TARGET_FILE:xl_probe_fixture> -DENGINE=${XL_STOCK_METADATA}
                    -DDATA=${XL_RETAIL_DATA} -DPREVIEW=${XL_PREVIEW_LIFECYCLE}
                    -DPILOT_HOG=${XL_PILOT_HOG} -DPILOT_DESCRIPTOR=${XL_PILOT_DESCRIPTOR}
                    -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/level-two-route-tests -P
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/preview_lifecycle.cmake)
            set_tests_properties(xl_level_two_keys PROPERTIES TIMEOUT 90)
            # Linked portals not crossed by the existing branch and secret routes
            foreach(crossing IN ITEMS "1,176,182" "2,193,194" "3,136,137" "5,5,185")
                string(REPLACE "," ";" endpoints "${crossing}")
                list(GET endpoints 0 trigger)
                list(GET endpoints 1 cross_from)
                list(GET endpoints 2 cross_to)
                add_test(
                    NAME xl_normal_portal_${cross_from}_${cross_to}
                    COMMAND
                        ${CMAKE_COMMAND} -DNAVIGATION=ON -DBRANCH=${trigger}
                        -DCROSS_FROM=${cross_from} -DCROSS_TO=${cross_to} -DLEVEL=${XL_PILOT_LEVEL}
                        -DPROBE=$<TARGET_FILE:xl_probe_fixture> -DENGINE=${XL_STOCK_METADATA}
                        -DDATA=${XL_RETAIL_DATA} -DPREVIEW=${XL_PREVIEW_LIFECYCLE}
                        -DPILOT_HOG=${XL_PILOT_HOG} -DPILOT_DESCRIPTOR=${XL_PILOT_DESCRIPTOR}
                        -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/portal-${cross_from}-${cross_to}-tests
                        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/preview_lifecycle.cmake)
                set_tests_properties(xl_normal_portal_${cross_from}_${cross_to} PROPERTIES TIMEOUT
                                                                                           90)
            endforeach()
            # Capture segments for all nine secret areas in the original pilot
            foreach(
                visit IN
                ITEMS 4
                      205
                      206
                      17
                      81
                      225
                      14
                      227
                      199)
                set(visit_reactor OFF)
                if(visit EQUAL 199)
                    # Reach the reactor-adjacent secret after the normal reactor fight
                    set(visit_reactor ON)
                endif()
                add_test(
                    NAME xl_normal_secret_${visit}
                    COMMAND
                        ${CMAKE_COMMAND} -DNAVIGATION=ON -DVISIT=${visit}
                        -DREACTOR=${visit_reactor} -DLEVEL=${XL_PILOT_LEVEL}
                        -DPROBE=$<TARGET_FILE:xl_probe_fixture> -DENGINE=${XL_STOCK_METADATA}
                        -DDATA=${XL_RETAIL_DATA} -DPREVIEW=${XL_PREVIEW_LIFECYCLE}
                        -DPILOT_HOG=${XL_PILOT_HOG} -DPILOT_DESCRIPTOR=${XL_PILOT_DESCRIPTOR}
                        -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/secret-${visit}-tests -P
                        ${CMAKE_CURRENT_SOURCE_DIR}/tests/preview_lifecycle.cmake)
                set_tests_properties(xl_normal_secret_${visit} PROPERTIES TIMEOUT 90)
            endforeach()
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
