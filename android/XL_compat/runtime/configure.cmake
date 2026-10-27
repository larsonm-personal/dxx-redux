# Shared stock/XL runtime for every OpenGL D2 build; the macro marks renderer capability
set(xl_runtime_dir "${CMAKE_CURRENT_LIST_DIR}")
if(NOT OPENGL)
    message(FATAL_ERROR "DXX_XL_RUNTIME currently requires the OpenGL runtime")
endif()
get_property(xl_targets DIRECTORY "${PROJECT_SOURCE_DIR}/main" PROPERTY BUILDSYSTEM_TARGETS)
foreach(xl_target IN LISTS xl_targets)
    get_target_property(xl_type ${xl_target} TYPE)
    if(NOT xl_target MATCHES "^dxx-redux-d2"
       AND NOT xl_target MATCHES "^test_(guidebot_(original|live)_navigation|upstream_compat)$")
        continue()
    endif()
    if(NOT xl_type STREQUAL "EXECUTABLE" AND NOT xl_type STREQUAL "SHARED_LIBRARY")
        continue()
    endif()
    target_compile_definitions(${xl_target} PRIVATE DXX_XL_RUNTIME)
    if(DXX_XL_DEVELOPER_TESTS)
        target_compile_definitions(${xl_target} PRIVATE DXX_XL_DEVELOPER_PROBE)
    endif()
    target_include_directories(${xl_target} PRIVATE "${xl_runtime_dir}")
    target_sources(
        ${xl_target}
        PRIVATE "${xl_runtime_dir}/preview.cpp"
                "${xl_runtime_dir}/teleport.cpp"
                "${xl_runtime_dir}/trigger.cpp"
                "${xl_runtime_dir}/wall_actions.cpp"
                "${xl_runtime_dir}/player_resources.cpp"
                "${xl_runtime_dir}/trigger_save.cpp"
                "${xl_runtime_dir}/object_trigger.cpp"
                "${xl_runtime_dir}/robot_generator.cpp"
                "${xl_runtime_dir}/boss_spawn.cpp"
                "${xl_runtime_dir}/equipment.cpp"
                "${xl_runtime_dir}/../inventory.cpp"
                "${xl_runtime_dir}/../admission.cpp"
                "${xl_runtime_dir}/../probe.cpp"
                "${xl_runtime_dir}/../lighting.cpp"
                "${xl_runtime_dir}/../scoped_sound.cpp"
                "${xl_runtime_dir}/../wave.cpp"
                "${xl_runtime_dir}/../companion_lighting.cpp"
                "${xl_runtime_dir}/companion_resources.cpp"
                "${xl_runtime_dir}/geometry.cpp"
                "${xl_runtime_dir}/light_sources.cpp"
                "${xl_runtime_dir}/visibility.cpp"
                "${xl_runtime_dir}/menu_indicator.c"
                "${xl_runtime_dir}/resource_validation.cpp"
                "${xl_runtime_dir}/../../app/src/main/cpp/shared/d1_custom_validation.c")
    if(NOT (DXX_XL_DEVELOPER_TESTS AND xl_target STREQUAL "dxx-redux-d2-headless-metadata"))
        target_sources(${xl_target} PRIVATE "${xl_runtime_dir}/start.cpp")
    endif()
    target_compile_features(${xl_target} PRIVATE cxx_std_17)
    if(ANDROID)
        target_sources(${xl_target} PRIVATE "${xl_runtime_dir}/persistence_probe.cpp"
                                            "${xl_runtime_dir}/corona_probe.cpp")
    endif()
endforeach()
target_compile_definitions(${DXX_TARGET_PREFIX}arch_ogl PRIVATE DXX_XL_RUNTIME)
if(DXX_XL_DEVELOPER_TESTS)
    target_compile_definitions(${DXX_TARGET_PREFIX}arch_ogl PRIVATE DXX_XL_DEVELOPER_PROBE)
endif()
target_include_directories(${DXX_TARGET_PREFIX}arch_ogl PRIVATE "${xl_runtime_dir}")
target_sources(${DXX_TARGET_PREFIX}arch_ogl PRIVATE "${xl_runtime_dir}/corona_renderer.c"
                                                    "${xl_runtime_dir}/water_renderer.c")
if(DXX_XL_DEVELOPER_TESTS AND TARGET dxx-redux-d2-headless-metadata)
    # Export earned private checkpoints with Android's real source-definition trailer
    set(xl_checkpoint_sources "${xl_runtime_dir}/checkpoint_export.cpp"
                              "${xl_runtime_dir}/../../app/src/main/cpp/shared/mod_save_compat.cpp")
    set_source_files_properties(
        ${xl_checkpoint_sources} TARGET_DIRECTORY dxx-redux-d2-headless-metadata
        PROPERTIES COMPILE_DEFINITIONS DXX_REWIND_FILE_WRAPPER)
    target_sources(dxx-redux-d2-headless-metadata PRIVATE ${xl_checkpoint_sources})
    target_sources(
        dxx-redux-d2-headless-metadata
        PRIVATE "${xl_runtime_dir}/../../app/src/main/cpp/shared/android_mission_assets.cpp")
    target_compile_definitions(
        dxx-redux-d2-headless-metadata PRIVATE DXX_XL_CHECKPOINT_EXPORT DXX_XL_DESCRIPTOR_TEST
                                               DXX_XL_DEVELOPER_PROBE)
    set_target_properties(dxx-redux-d2-headless-metadata PROPERTIES OUTPUT_NAME
                                                                    xl-preview-lifecycle)
endif()
