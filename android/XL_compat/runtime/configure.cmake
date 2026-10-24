# Opt-in CMAKE_PROJECT_INCLUDE, in a dedicated XL preview build tree only
if(PROJECT_NAME STREQUAL "dxx-redux-d2")
    function(xl_configure_render_preview)
        set(runtime_dir "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
        set(stock_main "${PROJECT_SOURCE_DIR}/main")
        set(generated "${PROJECT_BINARY_DIR}/xl_preview")
        file(MAKE_DIRECTORY "${generated}")
        file(READ "${stock_main}/render.c" render_source)
        set(anchor
            "\tif ( PlayerCfg.AlphaEffects && ( TmapInfo[tmap1].eclip_num == ECLIP_NUM_FUELCEN")
        string(FIND "${render_source}" "${anchor}" position)
        if(position LESS 0)
            message(FATAL_ERROR "Stock render_face hook changed; review XL overlay")
        endif()
        string(REPLACE "${anchor}" "" without_anchor "${render_source}")
        string(LENGTH "${render_source}" original_size)
        string(LENGTH "${without_anchor}" stripped_size)
        string(LENGTH "${anchor}" anchor_size)
        math(EXPR expected_size "${stripped_size} + ${anchor_size}")
        if(NOT original_size EQUAL expected_size)
            message(FATAL_ERROR "XL render hook must occur exactly once")
        endif()
        set(hook
            "\tfor (i = 0; i < nv; ++i) xl_preview_vertex(vp[i], uvlp[i].l, &dyn_light[i].r, &dyn_light[i].g, &dyn_light[i].b);\n"
        )
        string(REPLACE "${anchor}" "${hook}${anchor}" render_source "${render_source}")
        string(PREPEND render_source "#include \"${runtime_dir}/preview.h\"\n")
        file(GENERATE OUTPUT "${generated}/render.c" CONTENT "${render_source}")
        file(READ "${stock_main}/inferno.c" startup_source)
        foreach(anchor IN ITEMS "int replay_result = input_demo_maybe_start_replay_from_cmdline();")
            string(REPLACE "${anchor}" "" stripped "${startup_source}")
            string(LENGTH "${startup_source}" original_size)
            string(LENGTH "${stripped}" stripped_size)
            string(LENGTH "${anchor}" anchor_size)
            math(EXPR expected_size "${stripped_size} + ${anchor_size}")
            if(NOT original_size EQUAL expected_size)
                message(FATAL_ERROR "XL startup hook must occur exactly once: ${anchor}")
            endif()
        endforeach()
        string(REPLACE "int replay_result = input_demo_maybe_start_replay_from_cmdline();"
                       "int replay_result = xl_preview_start();" startup_source "${startup_source}")
        string(PREPEND startup_source "#include \"${runtime_dir}/preview.h\"\n")
        file(GENERATE OUTPUT "${generated}/inferno.c" CONTENT "${startup_source}")
        file(READ "${PROJECT_SOURCE_DIR}/arch/ogl/gr.c" graphics_source)
        foreach(anchor IN ITEMS "\tSDL_GL_SwapBuffers();" "\tglReadBuffer(GL_FRONT);")
            string(REPLACE "${anchor}" "" stripped "${graphics_source}")
            string(LENGTH "${graphics_source}" original_size)
            string(LENGTH "${stripped}" stripped_size)
            string(LENGTH "${anchor}" anchor_size)
            math(EXPR expected_size "${stripped_size} + ${anchor_size}")
            if(NOT original_size EQUAL expected_size)
                message(FATAL_ERROR "XL capture hook must occur exactly once: ${anchor}")
            endif()
        endforeach()
        string(REPLACE "\tSDL_GL_SwapBuffers();"
                       "\txl_preview_before_swap();\n\tSDL_GL_SwapBuffers();" graphics_source
                       "${graphics_source}")
        string(REPLACE "\tglReadBuffer(GL_FRONT);" "\tglReadBuffer(GL_BACK);" graphics_source
                       "${graphics_source}")
        string(PREPEND graphics_source "#include \"${runtime_dir}/preview.h\"\n")
        file(GENERATE OUTPUT "${generated}/gr.c" CONTENT "${graphics_source}")
        get_target_property(graphics_sources ${DXX_TARGET_PREFIX}arch_ogl SOURCES)
        list(FILTER graphics_sources EXCLUDE REGEX "^gr\\.c$")
        set_property(TARGET ${DXX_TARGET_PREFIX}arch_ogl PROPERTY SOURCES "${graphics_sources}")
        target_sources(${DXX_TARGET_PREFIX}arch_ogl PRIVATE "${generated}/gr.c")
        target_include_directories(${DXX_TARGET_PREFIX}arch_ogl
                                   PRIVATE "${PROJECT_SOURCE_DIR}/arch/ogl")
        file(
            GENERATE
            OUTPUT "${generated}/gamesave.c"
            CONTENT
                "#define load_level xl_stock_load_level\n#include \"${stock_main}/gamesave.c\"\n")
        get_target_property(stock_sources dxx-redux-d2 SOURCES)
        list(FILTER stock_sources EXCLUDE REGEX "^(render|gamesave|inferno)\\.c$")
        set_property(TARGET dxx-redux-d2 PROPERTY SOURCES "${stock_sources}")
        target_sources(
            dxx-redux-d2
            PRIVATE "${generated}/render.c"
                    "${generated}/gamesave.c"
                    "${generated}/inferno.c"
                    "${runtime_dir}/start.cpp"
                    "${runtime_dir}/preview.cpp"
                    "${runtime_dir}/../inventory.cpp"
                    "${runtime_dir}/../probe.cpp"
                    "${runtime_dir}/../lighting.cpp")
        target_compile_features(dxx-redux-d2 PRIVATE cxx_std_17)
        set_target_properties(dxx-redux-d2 PROPERTIES OUTPUT_NAME xl-render-preview)
        set(test_target dxx-redux-d2-headless-metadata)
        get_target_property(test_sources ${test_target} SOURCES)
        list(FILTER test_sources EXCLUDE REGEX
             "(^|/)gamesave\\.c$|headless_metadata_dump_main\\.cpp$")
        set_property(TARGET ${test_target} PROPERTY SOURCES "${test_sources}")
        target_sources(
            ${test_target}
            PRIVATE "${generated}/gamesave.c" "${runtime_dir}/lifecycle_test.cpp"
                    "${runtime_dir}/preview.cpp" "${runtime_dir}/../inventory.cpp"
                    "${runtime_dir}/../probe.cpp" "${runtime_dir}/../lighting.cpp")
        target_compile_features(${test_target} PRIVATE cxx_std_17)
        set_target_properties(${test_target} PROPERTIES OUTPUT_NAME xl-preview-lifecycle)
    endfunction()
    cmake_language(DEFER CALL xl_configure_render_preview)
endif()
