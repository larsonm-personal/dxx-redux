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
    add_executable(xl_rejection_fixture tests/rejection_fixture.cpp)
    target_link_libraries(xl_rejection_fixture PRIVATE xl_inventory)
    add_executable(xl_animation_phase_test tests/animation_phase_test.cpp)
    target_compile_features(xl_animation_phase_test PRIVATE cxx_std_17)
    add_test(NAME xl_animation_phase_contract COMMAND xl_animation_phase_test)
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
        include(tests/reference_teleport.cmake)
        include(tests/reference_speed_boost.cmake)
        include(tests/reference_spawn_points.cmake)
        include(tests/reference_water_actors.cmake)
        include(tests/reference_lava.cmake)
        include(tests/reference_trigger.cmake)
        include(tests/reference_scheduler.cmake)
        include(tests/reference_object.cmake)
        include(tests/reference_generators.cmake)
        include(tests/reference_boss.cmake)
        include(tests/reference_boss_routes.cmake)
        include(tests/reference_trigger_flags.cmake)
        include(tests/reference_player_resources.cmake)
        include(tests/reference_equipment.cmake)
        include(tests/reference_effects.cmake)
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
    add_test(NAME xl_effect_payloads COMMAND xl_inventory_test --effects)
    set(XL_NEPTUNE_HOG "" CACHE FILEPATH "Original Neptune XL HOG for private 23/40 qualification")
    set(XL_NEPTUNE_DESCRIPTOR "" CACHE FILEPATH "Original Neptune XL mission descriptor")
    set(XL_REFERENCE_COLOR_ROOT "" CACHE PATH "Pinned XL source for CLR/embedded precedence oracle")
    if(XL_REFERENCE_COLOR_ROOT)
        include(tests/reference_color_precedence.cmake)
    endif()
    if(XL_NEPTUNE_HOG)
        add_test(
            NAME xl_neptune_inventory
            COMMAND
                ${CMAKE_COMMAND} -DHOG=${XL_NEPTUNE_HOG} -DTEST=$<TARGET_FILE:xl_inventory_test> -P
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/neptune_inventory.cmake)
        set_tests_properties(xl_neptune_inventory PROPERTIES TIMEOUT 30)
    endif()
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
    if(XL_NEPTUNE_HOG AND XL_PREVIEW_LIFECYCLE AND XL_RETAIL_DATA)
        add_test(
            NAME xl_neptune_native_layout
            COMMAND
                ${CMAKE_COMMAND} -DHOG=${XL_NEPTUNE_HOG} -DENGINE=${XL_PREVIEW_LIFECYCLE}
                -DDATA=${XL_RETAIL_DATA} -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/neptune-layout -P
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/neptune_layout.cmake)
        set_tests_properties(xl_neptune_native_layout PROPERTIES TIMEOUT 70)
        if(XL_NEPTUNE_DESCRIPTOR)
            set(xl_teleport_oracle_args)
            if(TARGET xl_teleport_reference)
                list(APPEND xl_teleport_oracle_args
                     "-DREFERENCE=$<TARGET_FILE:xl_teleport_reference>")
            endif()
            add_test(
                NAME xl_wall_teleports
                COMMAND
                    ${CMAKE_COMMAND} -DHOG=${XL_NEPTUNE_HOG} -DDESCRIPTOR=${XL_NEPTUNE_DESCRIPTOR}
                    -DENGINE=${XL_PREVIEW_LIFECYCLE} -DDATA=${XL_RETAIL_DATA}
                    -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/wall-teleports ${xl_teleport_oracle_args}
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/teleport.cmake)
            set_tests_properties(xl_wall_teleports PROPERTIES TIMEOUT 85)
            set(xl_trigger_oracle_args)
            if(TARGET xl_trigger_reference)
                list(APPEND xl_trigger_oracle_args
                     "-DREFERENCE=$<TARGET_FILE:xl_trigger_reference>")
            endif()
            add_test(
                NAME xl_wall_trigger_chains
                COMMAND
                    ${CMAKE_COMMAND} -DHOG=${XL_NEPTUNE_HOG} -DENGINE=${XL_PREVIEW_LIFECYCLE}
                    -DDATA=${XL_RETAIL_DATA} -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/trigger-chains
                    ${xl_trigger_oracle_args} -P
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/trigger_chain.cmake)
            set_tests_properties(xl_wall_trigger_chains PROPERTIES TIMEOUT 85)
            set(xl_scheduler_oracle_args)
            if(TARGET xl_scheduler_reference)
                list(APPEND xl_scheduler_oracle_args
                     "-DREFERENCE=$<TARGET_FILE:xl_scheduler_reference>")
            endif()
            add_test(
                NAME xl_wall_trigger_scheduler
                COMMAND
                    ${CMAKE_COMMAND} -DHOG=${XL_NEPTUNE_HOG} -DENGINE=${XL_PREVIEW_LIFECYCLE}
                    -DDATA=${XL_RETAIL_DATA} -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/trigger-scheduler
                    ${xl_scheduler_oracle_args} -P
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/trigger_scheduler.cmake)
            set_tests_properties(xl_wall_trigger_scheduler PROPERTIES TIMEOUT 85)
            set(xl_object_oracle_args)
            if(TARGET xl_object_decode_reference)
                list(APPEND xl_object_oracle_args
                     "-DREFERENCE=$<TARGET_FILE:xl_object_decode_reference>")
            endif()
            add_test(
                NAME xl_object_events
                COMMAND
                    ${CMAKE_COMMAND} -DHOG=${XL_NEPTUNE_HOG} -DENGINE=${XL_PREVIEW_LIFECYCLE}
                    -DDATA=${XL_RETAIL_DATA} -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/object-events
                    ${xl_object_oracle_args} -P
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/object_events.cmake)
            set_tests_properties(xl_object_events PROPERTIES TIMEOUT 90)
            add_test(
                NAME xl_object_chain_delays
                COMMAND
                    ${CMAKE_COMMAND} -DHOG=${XL_NEPTUNE_HOG} -DENGINE=${XL_PREVIEW_LIFECYCLE}
                    -DDATA=${XL_RETAIL_DATA}
                    -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/object-chain-delays
                    ${xl_scheduler_oracle_args} -P
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/object_chain_delay.cmake)
            set_tests_properties(xl_object_chain_delays PROPERTIES TIMEOUT 90)
            set(xl_boost_oracle_args)
            if(TARGET xl_speed_boost_reference)
                list(APPEND xl_boost_oracle_args
                     "-DREFERENCE=$<TARGET_FILE:xl_speed_boost_reference>")
            endif()
            add_test(
                NAME xl_speed_boosts
                COMMAND
                    ${CMAKE_COMMAND} -DHOG=${XL_NEPTUNE_HOG} -DENGINE=${XL_PREVIEW_LIFECYCLE}
                    -DDATA=${XL_RETAIL_DATA} -DDESCRIPTOR=${XL_NEPTUNE_DESCRIPTOR}
                    -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/speed-boosts ${xl_boost_oracle_args} -P
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/speed_boost.cmake)
            set_tests_properties(xl_speed_boosts PROPERTIES TIMEOUT 90)
            set(xl_spawn_oracle_args)
            if(TARGET xl_spawn_points_reference)
                list(APPEND xl_spawn_oracle_args
                     "-DREFERENCE=$<TARGET_FILE:xl_spawn_points_reference>")
            endif()
            add_test(
                NAME xl_spawn_points
                COMMAND
                    ${CMAKE_COMMAND} -DHOG=${XL_NEPTUNE_HOG} -DENGINE=${XL_PREVIEW_LIFECYCLE}
                    -DDATA=${XL_RETAIL_DATA} -DDESCRIPTOR=${XL_NEPTUNE_DESCRIPTOR}
                    -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/spawn-points ${xl_spawn_oracle_args} -P
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/spawn_points.cmake)
            set_tests_properties(xl_spawn_points PROPERTIES TIMEOUT 90)
            set(xl_lava_args)
            if(TARGET xl_lava_reference)
                list(APPEND xl_lava_args "-DREFERENCE=$<TARGET_FILE:xl_lava_reference>"
                     "-DMOTION_REFERENCE=$<TARGET_FILE:xl_water_actors_reference>")
            endif()
            add_test(
                NAME xl_lava_volumes
                COMMAND
                    ${CMAKE_COMMAND} -DHOG=${XL_NEPTUNE_HOG} -DENGINE=${XL_PREVIEW_LIFECYCLE}
                    -DDATA=${XL_RETAIL_DATA} -DDESCRIPTOR=${XL_NEPTUNE_DESCRIPTOR}
                    -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/lava ${xl_lava_args} -P
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/lava.cmake)
            set_tests_properties(xl_lava_volumes PROPERTIES TIMEOUT 120)
            set(xl_water_actor_args)
            if(TARGET xl_water_actors_reference)
                list(APPEND xl_water_actor_args
                     "-DREFERENCE=$<TARGET_FILE:xl_water_actors_reference>")
            endif()
            add_test(
                NAME xl_water_actors
                COMMAND
                    ${CMAKE_COMMAND} -DHOG=${XL_NEPTUNE_HOG} -DENGINE=${XL_PREVIEW_LIFECYCLE}
                    -DDATA=${XL_RETAIL_DATA} -DDESCRIPTOR=${XL_NEPTUNE_DESCRIPTOR}
                    -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/water-actors ${xl_water_actor_args} -P
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/water_actors.cmake)
            set_tests_properties(xl_water_actors PROPERTIES TIMEOUT 120)
            set(xl_generator_oracle_args)
            if(TARGET xl_generator_reference)
                list(APPEND xl_generator_oracle_args
                     "-DREFERENCE=$<TARGET_FILE:xl_generator_reference>")
            endif()
            add_test(
                NAME xl_generator_assignment_runtime
                COMMAND
                    ${CMAKE_COMMAND} -DHOG=${XL_NEPTUNE_HOG} -DENGINE=${XL_PREVIEW_LIFECYCLE}
                    -DDATA=${XL_RETAIL_DATA} -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/generator-runtime
                    ${xl_generator_oracle_args} -P
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/generator_runtime.cmake)
            set_tests_properties(xl_generator_assignment_runtime PROPERTIES TIMEOUT 90)
            set(xl_boss_oracle_args)
            if(TARGET xl_boss_reference)
                list(APPEND xl_boss_oracle_args "-DREFERENCE=$<TARGET_FILE:xl_boss_reference>")
            endif()
            add_test(
                NAME xl_boss_spawning
                COMMAND
                    ${CMAKE_COMMAND} -DHOG=${XL_NEPTUNE_HOG} -DENGINE=${XL_PREVIEW_LIFECYCLE}
                    -DDATA=${XL_RETAIL_DATA} -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/boss-spawning
                    ${xl_boss_oracle_args} -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/boss_spawn.cmake)
            set_tests_properties(xl_boss_spawning PROPERTIES TIMEOUT 90)
            set(xl_boss_route_args)
            if(TARGET xl_boss_routes_reference)
                list(APPEND xl_boss_route_args
                     "-DREFERENCE=$<TARGET_FILE:xl_boss_routes_reference>")
            endif()
            add_test(
                NAME xl_boss_transport
                COMMAND
                    ${CMAKE_COMMAND} -DHOG=${XL_NEPTUNE_HOG} -DENGINE=${XL_PREVIEW_LIFECYCLE}
                    -DDATA=${XL_RETAIL_DATA} -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/boss-transport
                    ${xl_boss_route_args} -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/boss_transport.cmake)
            set_tests_properties(xl_boss_transport PROPERTIES TIMEOUT 90)
            add_test(
                NAME xl_trigger_flags
                COMMAND
                    ${CMAKE_COMMAND} -DHOG=${XL_NEPTUNE_HOG} -DENGINE=${XL_PREVIEW_LIFECYCLE}
                    -DDATA=${XL_RETAIL_DATA} -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/trigger-flags -P
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/trigger_flags.cmake)
            set_tests_properties(xl_trigger_flags PROPERTIES TIMEOUT 90)
            add_test(
                NAME xl_boss_groups
                COMMAND
                    ${CMAKE_COMMAND} -DHOG=${XL_NEPTUNE_HOG} -DENGINE=${XL_PREVIEW_LIFECYCLE}
                    -DDATA=${XL_RETAIL_DATA} -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/boss-groups -P
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/boss_groups.cmake)
            set_tests_properties(xl_boss_groups PROPERTIES TIMEOUT 90)
            add_test(
                NAME xl_player_resources
                COMMAND
                    ${CMAKE_COMMAND} -DHOG=${XL_NEPTUNE_HOG} -DENGINE=${XL_PREVIEW_LIFECYCLE}
                    -DDATA=${XL_RETAIL_DATA} -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/player-resources
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/player_resources.cmake)
            set_tests_properties(xl_player_resources PROPERTIES TIMEOUT 90)
            set(xl_equipment_oracle_args)
            if(TARGET xl_equipment_reference)
                list(APPEND xl_equipment_oracle_args
                     "-DREFERENCE=$<TARGET_FILE:xl_equipment_reference>")
            endif()
            add_test(
                NAME xl_equipment_generators
                COMMAND
                    ${CMAKE_COMMAND} -DHOG=${XL_NEPTUNE_HOG} -DENGINE=${XL_PREVIEW_LIFECYCLE}
                    -DDATA=${XL_RETAIL_DATA} -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/equipment
                    ${xl_equipment_oracle_args} -P
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/equipment.cmake)
            set_tests_properties(xl_equipment_generators PROPERTIES TIMEOUT 90)
            add_test(
                NAME xl_effect_inspection_gate
                COMMAND
                    ${CMAKE_COMMAND} -DHOG=${XL_NEPTUNE_HOG} -DENGINE=${XL_PREVIEW_LIFECYCLE}
                    -DDATA=${XL_RETAIL_DATA} -DFIXTURE=$<TARGET_FILE:xl_inventory_test>
                    -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/effect-inspection -P
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/effect_inspection.cmake)
            set_tests_properties(xl_effect_inspection_gate PROPERTIES TIMEOUT 90)
            add_test(
                NAME xl_modern_properties
                COMMAND
                    ${CMAKE_COMMAND} -DHOG=${XL_NEPTUNE_HOG} -DDESCRIPTOR=${XL_NEPTUNE_DESCRIPTOR}
                    -DENGINE=${XL_PREVIEW_LIFECYCLE} -DDATA=${XL_RETAIL_DATA}
                    -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/modern-properties -P
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/modern_properties.cmake)
            set_tests_properties(xl_modern_properties PROPERTIES TIMEOUT 85)
            add_test(
                NAME xl_modern_water
                COMMAND
                    ${CMAKE_COMMAND} -DHOG=${XL_NEPTUNE_HOG} -DDESCRIPTOR=${XL_NEPTUNE_DESCRIPTOR}
                    -DENGINE=${XL_PREVIEW_LIFECYCLE} -DDATA=${XL_RETAIL_DATA} -DMODE=water
                    -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/modern-water -P
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/modern_properties.cmake)
            set_tests_properties(xl_modern_water PROPERTIES TIMEOUT 85)
            add_test(
                NAME xl_neptune_native_features
                COMMAND
                    ${CMAKE_COMMAND} -DHOG=${XL_NEPTUNE_HOG} -DDESCRIPTOR=${XL_NEPTUNE_DESCRIPTOR}
                    -DENGINE=${XL_PREVIEW_LIFECYCLE} -DDATA=${XL_RETAIL_DATA}
                    -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/neptune-features -P
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/neptune_features.cmake)
            set_tests_properties(xl_neptune_native_features PROPERTIES TIMEOUT 85)
        endif()
    endif()
    set(XL_OBSERVATORY_DESCRIPTOR "" CACHE FILEPATH "Original Observatory mission descriptor")
    set(XL_KONFLICT_HOG "" CACHE FILEPATH "Original Konflict HOG for bounded 17/36 coverage")
    set(XL_KONFLICT_DESCRIPTOR "" CACHE FILEPATH "Original Konflict mission descriptor")
    if(XL_KONFLICT_HOG AND XL_KONFLICT_DESCRIPTOR AND XL_PREVIEW_LIFECYCLE AND XL_RETAIL_DATA)
        add_test(
            NAME xl_konflict_features
            COMMAND
                ${CMAKE_COMMAND} -DHOG=${XL_KONFLICT_HOG} -DDESCRIPTOR=${XL_KONFLICT_DESCRIPTOR}
                -DENGINE=${XL_PREVIEW_LIFECYCLE} -DDATA=${XL_RETAIL_DATA}
                -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/konflict-features -P
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/konflict_features.cmake)
        set_tests_properties(xl_konflict_features PROPERTIES TIMEOUT 90)
    endif()
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
    set(XL_REFERENCE_WEAPON_CONTROL "" CACHE FILEPATH
                                             "Pinned XL weapon control for matched no-damage gates")
    set(xl_reference_gate_args)
    if(XL_REFERENCE_WEAPON_CONTROL)
        include(tests/reference_no_damage_gates.cmake)
        list(APPEND xl_reference_gate_args
             -DREFERENCE_GATES=$<TARGET_FILE:xl_no_damage_reference_gates>)
    endif()
    if(XL_OBSERVATORY_HOG AND XL_OBSERVATORY_DESCRIPTOR AND XL_PREVIEW_LIFECYCLE AND XL_RETAIL_DATA)
        add_test(
            NAME xl_observatory_descriptor
            COMMAND
                ${CMAKE_COMMAND} -DHOG=${XL_OBSERVATORY_HOG}
                -DDESCRIPTOR=${XL_OBSERVATORY_DESCRIPTOR} -DENGINE=${XL_PREVIEW_LIFECYCLE}
                -DDATA=${XL_RETAIL_DATA}
                -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/observatory-descriptor
                ${xl_reference_gate_args} -P
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
