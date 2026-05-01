set(D2X_MAIN_SOURCES
    ai.c
    ai2.c
    aipath.c
    automap.c
    bm.c
    cntrlcen.c
    collide.c
    config.c
    console.c
    controls.c
    credits.c
    digiobj.c
    dumpmine.c
    effects.c
    input_demo_start.c
    endlevel.c
    escort.c
    fireball.c
    fuelcen.c
    fvi.c
    game.c
    gamecntl.c
    gamefont.c
    gamemine.c
    gamepal.c
    gamerend.c
    gamesave.c
    gameseg.c
    gameseq.c
    gauges.c
    hostage.c
    hud.c
    inferno.c
    kconfig.c
    kmatrix.c
    laser.c
    lighting.c
    menu.c
    mglobal.c
    mission.c
    morph.c
    movie.c
    multi.c
    multibot.c
    newdemo.c
    newmenu.c
    object.c
    paging.c
    physics.c
    piggy.c
    player.c
    playsave.c
    polyobj.c
    powerup.c
    render.c
    robot.c
    scores.c
    segment.c
    slew.c
    songs.c
    state.c
    switch.c
    terrain.c
    texmerge.c
    text.c
    titles.c
    vclip.c
    wall.c
    weapon.c
    )

if(ANDROID)
    add_library(dxx-redux-d2 SHARED ${D2X_MAIN_SOURCES})
else()
    add_executable(dxx-redux-d2 ${D2X_MAIN_SOURCES})
endif()

include_directories(../include ../arch/include)

include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/input-demo-codec-deps.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/input-demo-build-metadata.cmake)

target_compile_definitions(dxx-redux-d2 PUBLIC
    DXX_VERSION_MAJORi=${PROJECT_VERSION_MAJOR}
    DXX_VERSION_MINORi=${PROJECT_VERSION_MINOR}
    DXX_VERSION_MICROi=$<IF:$<BOOL:${PROJECT_VERSION_MICRO}>,${PROJECT_VERSION_MICRO},0>
    )

dxx_input_demo_apply_build_metadata(dxx-redux-d2)

include(CheckTypeSize)
check_type_size("ssize_t" SIZEOF_SSIZE_T LANGUAGE C)
if(NOT "${SIZEOF_SSIZE_T}" STREQUAL "")
    target_compile_definitions(dxx-redux-d2 PUBLIC HAVE_SSIZE_T=1)
endif()

if(OPENGL)
    target_link_libraries(dxx-redux-d2 PRIVATE ${DXX_TARGET_PREFIX}arch_ogl ${DXX_TARGET_PREFIX}xmodel)
endif()

if(SDLMIXER)
    target_link_libraries(dxx-redux-d2 PUBLIC ${SDL_MIXER_LIBRARIES})
endif()

if(NOT ANDROID)
    if(NOT TARGET nlohmann_json::nlohmann_json)
        include(FetchContent)
        FetchContent_Declare(
            nlohmann_json
            GIT_REPOSITORY https://github.com/nlohmann/json.git
            GIT_TAG        v3.11.3
            GIT_SHALLOW    TRUE
        )
        set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
        set(JSON_Install OFF CACHE BOOL "" FORCE)
        set(JSON_MultipleHeaders OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(nlohmann_json)
    endif()

    find_package(ZLIB REQUIRED)

    target_compile_features(dxx-redux-d2 PRIVATE cxx_std_11)
    target_sources(dxx-redux-d2 PRIVATE
        ../../android/app/src/main/cpp/shared/input_demo_rng_mode.c
        ../../android/app/src/main/cpp/shared/input_demo_controls.cpp
        ../../android/app/src/main/cpp/shared/input_demo_fixture.cpp
        ../../android/app/src/main/cpp/shared/input_demo_codec.cpp
        ../../android/app/src/main/cpp/shared/input_demo_result.cpp
        ../../android/app/src/main/cpp/shared/input_demo_recorder.cpp
        ../../android/app/src/main/cpp/shared/input_demo_replay.cpp)
    target_include_directories(dxx-redux-d2 PRIVATE ../../android/app/src/main/cpp/shared)
    target_link_libraries(dxx-redux-d2 PRIVATE nlohmann_json::nlohmann_json)
    target_link_libraries(dxx-redux-d2 PRIVATE ZLIB::ZLIB)
    dxx_input_demo_apply_codec_deps(dxx-redux-d2)
endif()

if(EDITOR)
    target_sources(dxx-redux-d2 PRIVATE bmread.c)
    target_link_libraries(dxx-redux-d2 PRIVATE ${DXX_TARGET_PREFIX}editor ${DXX_TARGET_PREFIX}ui)
endif()

if(UDP)
    target_include_directories(dxx-redux-d2 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_sources(dxx-redux-d2 PRIVATE
        net_udp.c
        ../../android/app/src/main/cpp/shared/net/net_udp_android.c)
    if(ANDROID)
        target_sources(dxx-redux-d2 PRIVATE auto_net.c coop_save.c coop_warp.c)
    endif()
endif()

if(WIN32)
    list(APPEND EXTRA_LIBRARIES glu32 winmm ws2_32 dbghelp)
    target_link_libraries(dxx-redux-d2 PRIVATE ${DXX_TARGET_PREFIX}arch_win32)
    target_sources(dxx-redux-d2 PRIVATE
        ${CMAKE_SOURCE_DIR}/arch/win32/d2x-rebirth.ico
        ${CMAKE_SOURCE_DIR}/arch/win32/d2x-rebirth.rc
        )
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /NODEFAULTLIB:msvcrt.lib")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        # The manifest has to be included through the .rc for compatibility with MSYS. However
        # MSVC will automatically generate a manifest using this method - which will cause a
        # collision - so we need to tell it not to.
        target_link_options(dxx-redux-d2 PRIVATE "/MANIFEST:NO")
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            # Enable Hot Reload for debug builds
            target_link_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:/INCREMENTAL>)
            target_compile_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:/ZI>)
        endif()
    endif()

    install(FILES
        ${PROJECT_BINARY_DIR}/main/SDL.dll
        ${PROJECT_BINARY_DIR}/main/physfs.dll
        DESTINATION .)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        install(FILES ${PROJECT_BINARY_DIR}/main/zlibd1.dll DESTINATION .)
    else()
        install(FILES ${PROJECT_BINARY_DIR}/main/zlib1.dll DESTINATION .)
    endif()
    if(SDLMIXER)
        install(FILES ${PROJECT_BINARY_DIR}/main/SDL_mixer.dll DESTINATION .)
    endif()
    if(OPENGL)
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            install(FILES ${PROJECT_BINARY_DIR}/main/glew32d.dll DESTINATION .)
        else()
            install(FILES ${PROJECT_BINARY_DIR}/main/glew32.dll DESTINATION .)
        endif()
    endif()
    if(PNG)
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            install(FILES ${PROJECT_BINARY_DIR}/main/libpng16d.dll DESTINATION .)
        else()
            install(FILES ${PROJECT_BINARY_DIR}/main/libpng16.dll DESTINATION .)
        endif()
    endif()
endif()

if(APPLE)
    target_link_libraries(dxx-redux-d2 PRIVATE ${DXX_TARGET_PREFIX}arch_cocoa)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(dxx-redux-d2 PRIVATE ${DXX_TARGET_PREFIX}arch_x11)
    list(APPEND EXTRA_LIBRARIES "X11")
endif()

if(NOT WIN32)
    list(APPEND EXTRA_LIBRARIES "m")
endif()

find_package(SDL 1.2 REQUIRED)
find_package(PhysFS REQUIRED)
if(OPENGL)
    find_package(OpenGL REQUIRED)
    find_package(GLEW REQUIRED)
    list(APPEND EXTRA_LIBRARIES GLEW::GLEW)
endif()
if(PNG)
    find_package(PNG REQUIRED)
endif()
target_include_directories(dxx-redux-d2 PUBLIC ${SDL_INCLUDE_DIR} ${PHYSFS_INCLUDE_DIR})
target_link_libraries(dxx-redux-d2 PRIVATE ${DXX_TARGET_PREFIX}2d ${DXX_TARGET_PREFIX}3d ${DXX_TARGET_PREFIX}arch_sdl ${DXX_TARGET_PREFIX}iff ${DXX_TARGET_PREFIX}libmve ${DXX_TARGET_PREFIX}maths ${DXX_TARGET_PREFIX}mem ${DXX_TARGET_PREFIX}misc ${DXX_TARGET_PREFIX}texmap)
target_link_libraries(dxx-redux-d2 PUBLIC ${OPENGL_LIBRARY} ${SDL_LIBRARY} ${SDL_MIXER_LIBRARY} ${PHYSFS_LIBRARY} ${PNG_LIBRARY} ${EXTRA_LIBRARIES})
target_sources(dxx-redux-d2 PRIVATE vers_id.c)

if(NOT ANDROID)
    set(D2X_HEADLESS_SOURCES ${D2X_MAIN_SOURCES})
    list(REMOVE_ITEM D2X_HEADLESS_SOURCES inferno.c)

    add_executable(dxx-redux-d2-headless
        ${D2X_HEADLESS_SOURCES}
        vers_id.c
        ../../android/app/src/main/cpp/headless/d2_headless_runtime.c
        ../../android/app/src/main/cpp/headless/input_demo_headless_main.cpp
        ../../android/app/src/main/cpp/shared/input_demo_rng_mode.c
        ../../android/app/src/main/cpp/shared/input_demo_controls.cpp
        ../../android/app/src/main/cpp/shared/input_demo_fixture.cpp
        ../../android/app/src/main/cpp/shared/input_demo_codec.cpp
        ../../android/app/src/main/cpp/shared/input_demo_result.cpp
        ../../android/app/src/main/cpp/shared/input_demo_recorder.cpp
        ../../android/app/src/main/cpp/shared/input_demo_replay.cpp)

    target_compile_features(dxx-redux-d2-headless PRIVATE cxx_std_11)
    target_compile_definitions(dxx-redux-d2-headless PRIVATE
        DXX_VERSION_MAJORi=${PROJECT_VERSION_MAJOR}
        DXX_VERSION_MINORi=${PROJECT_VERSION_MINOR}
        DXX_VERSION_MICROi=$<IF:$<BOOL:${PROJECT_VERSION_MICRO}>,${PROJECT_VERSION_MICRO},0>
        DXX_HEADLESS_CONSOLE=1)
    if(NOT "${SIZEOF_SSIZE_T}" STREQUAL "")
        target_compile_definitions(dxx-redux-d2-headless PRIVATE HAVE_SSIZE_T=1)
    endif()

    target_include_directories(dxx-redux-d2-headless PUBLIC ${SDL_INCLUDE_DIR} ${PHYSFS_INCLUDE_DIR})
    target_include_directories(dxx-redux-d2-headless PRIVATE ../../android/app/src/main/cpp/shared)
    target_link_libraries(dxx-redux-d2-headless PRIVATE ${DXX_TARGET_PREFIX}2d ${DXX_TARGET_PREFIX}3d ${DXX_TARGET_PREFIX}arch_sdl ${DXX_TARGET_PREFIX}iff ${DXX_TARGET_PREFIX}libmve ${DXX_TARGET_PREFIX}maths ${DXX_TARGET_PREFIX}mem ${DXX_TARGET_PREFIX}misc ${DXX_TARGET_PREFIX}texmap)
    target_link_libraries(dxx-redux-d2-headless PRIVATE nlohmann_json::nlohmann_json ZLIB::ZLIB)

    if(OPENGL)
        target_link_libraries(dxx-redux-d2-headless PRIVATE ${DXX_TARGET_PREFIX}arch_ogl ${DXX_TARGET_PREFIX}xmodel)
    endif()
    if(SDLMIXER)
        target_link_libraries(dxx-redux-d2-headless PUBLIC ${SDL_MIXER_LIBRARIES})
    endif()
    if(UDP)
        target_include_directories(dxx-redux-d2-headless PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
        target_sources(dxx-redux-d2-headless PRIVATE
            net_udp.c
            ../../android/app/src/main/cpp/shared/net/net_udp_android.c)
    endif()
    if(WIN32)
        target_link_libraries(dxx-redux-d2-headless PRIVATE ${DXX_TARGET_PREFIX}arch_win32)
    endif()
    if(APPLE)
        target_link_libraries(dxx-redux-d2-headless PRIVATE ${DXX_TARGET_PREFIX}arch_cocoa)
    endif()
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_libraries(dxx-redux-d2-headless PRIVATE ${DXX_TARGET_PREFIX}arch_x11)
    endif()

    target_link_libraries(dxx-redux-d2-headless PUBLIC ${OPENGL_LIBRARY} ${SDL_LIBRARY} ${SDL_MIXER_LIBRARY} ${PHYSFS_LIBRARY} ${PNG_LIBRARY} ${EXTRA_LIBRARIES})
    dxx_input_demo_apply_codec_deps(dxx-redux-d2-headless)
    dxx_input_demo_apply_build_metadata(dxx-redux-d2-headless)
endif()

install(TARGETS dxx-redux-d2 RUNTIME DESTINATION .)
