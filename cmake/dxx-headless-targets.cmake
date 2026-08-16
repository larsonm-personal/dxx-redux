include_guard(GLOBAL)

function(dxx_add_headless_target target main_source)
    set(options INCLUDE_GAME_DIR)
    set(multi_value_args GAME_SOURCES PRIVATE_DEFINITIONS PRIVATE_LIBRARIES)
    cmake_parse_arguments(PARSE_ARGV 2 arg "${options}" "" "${multi_value_args}")

    add_executable(
        ${target}
        ${arg_GAME_SOURCES}
        vers_id.c
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../android/app/src/main/cpp/headless/d2_headless_runtime.c
        ${main_source}
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../android/app/src/main/cpp/shared/input_demo_rng_mode.c
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../android/app/src/main/cpp/shared/input_demo_controls.cpp
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../android/app/src/main/cpp/shared/input_demo_fixture.cpp
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../android/app/src/main/cpp/shared/input_demo_codec.cpp
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../android/app/src/main/cpp/shared/input_demo_result.cpp
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../android/app/src/main/cpp/shared/input_demo_state_trace.cpp
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../android/app/src/main/cpp/shared/input_demo_recorder.cpp
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../android/app/src/main/cpp/shared/input_demo_replay.cpp
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../android/app/src/main/cpp/shared/input_demo_direct_command_policy.c
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../android/app/src/main/cpp/shared/hmp_android_shared.c
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../android/app/src/main/cpp/shared/midi_metadata.c
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../android/app/src/main/cpp/shared/midi_metadata_physfs.c
    )

    target_compile_features(${target} PRIVATE cxx_std_11)
    target_compile_definitions(
        ${target}
        PRIVATE ${arg_PRIVATE_DEFINITIONS}
                DXX_VERSION_MAJORi=${PROJECT_VERSION_MAJOR}
                DXX_VERSION_MINORi=${PROJECT_VERSION_MINOR}
                DXX_VERSION_MICROi=$<IF:$<BOOL:${PROJECT_VERSION_MICRO}>,${PROJECT_VERSION_MICRO},0>
                DXX_HEADLESS_CONSOLE=1)
    if(NOT "${SIZEOF_SSIZE_T}" STREQUAL "")
        target_compile_definitions(${target} PRIVATE HAVE_SSIZE_T=1)
    endif()

    target_include_directories(${target} PUBLIC ${SDL_INCLUDE_DIR} ${PHYSFS_INCLUDE_DIR})
    target_include_directories(
        ${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../android/app/src/main/cpp/shared)
    if(arg_INCLUDE_GAME_DIR)
        target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    endif()
    target_link_libraries(
        ${target}
        PRIVATE ${DXX_TARGET_PREFIX}2d
                ${DXX_TARGET_PREFIX}3d
                ${DXX_TARGET_PREFIX}arch_sdl
                ${DXX_TARGET_PREFIX}iff
                ${arg_PRIVATE_LIBRARIES}
                ${DXX_TARGET_PREFIX}maths
                ${DXX_TARGET_PREFIX}mem
                ${DXX_TARGET_PREFIX}texmap
                nlohmann_json::nlohmann_json
                ZLIB::ZLIB)
    if(OPENGL)
        target_link_libraries(${target} PRIVATE ${DXX_TARGET_PREFIX}arch_ogl
                                                ${DXX_TARGET_PREFIX}xmodel)
    endif()
    target_link_libraries(${target} PRIVATE ${DXX_TARGET_PREFIX}misc)
    if(SDLMIXER)
        target_link_libraries(${target} PUBLIC ${SDL_MIXER_LIBRARIES})
    endif()
    if(UDP)
        target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
        target_sources(
            ${target}
            PRIVATE
                net_udp.c
                ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../android/app/src/main/cpp/shared/net/net_udp_android.c
                ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../android/app/src/main/cpp/shared/net/net_udp_reconnect_auth.c
                ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../android/app/src/main/cpp/shared/net/net_udp_p2p_proxy_shared.c
        )
    endif()
    if(WIN32)
        target_link_libraries(${target} PRIVATE ${DXX_TARGET_PREFIX}arch_win32)
    endif()
    if(APPLE)
        target_link_libraries(${target} PRIVATE ${DXX_TARGET_PREFIX}arch_cocoa)
    endif()
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_libraries(${target} PRIVATE ${DXX_TARGET_PREFIX}arch_x11)
    endif()

    target_link_libraries(${target} PUBLIC ${OPENGL_LIBRARY} ${SDL_LIBRARY} ${SDL_MIXER_LIBRARY}
                                           ${PHYSFS_LIBRARY} ${PNG_LIBRARY} ${EXTRA_LIBRARIES})
    dxx_input_demo_apply_codec_deps(${target})
    dxx_input_demo_apply_build_metadata(${target})
endfunction()
