include_guard(GLOBAL)
include(${CMAKE_CURRENT_LIST_DIR}/dxx-verified-dependencies.cmake)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${DXX_DEPENDENCY_MANIFEST}")

# Local scope avoids changing other dependencies
function(dxx_add_fluidsynth)
    foreach(dependency FLUIDSYNTH GCEM)
        dxx_dependency_value(${dependency}_URL ${dependency}_URL)
        dxx_dependency_value(${dependency}_SHA256 ${dependency}_SHA256)
    endforeach()
    # Synthesis only: no audio drivers, networking, GPL readline, or optional DSP dependencies
    foreach(
        feature
        portaudio
        floats
        alsa
        aufile
        dbus
        ipv6
        jack
        ladspa
        libsndfile
        midishare
        opensles
        oboe
        network
        oss
        dsound
        wasapi
        waveout
        winmidi
        sdl3
        pulseaudio
        pipewire
        readline
        openmp
        native-dls
        signalsmith
        systemd
        coreaudio
        coremidi
        framework)
        set(enable-${feature} OFF CACHE BOOL "Disabled for shared music renderer" FORCE)
    endforeach()
    set(BUILD_SHARED_LIBS ON)
    set(osal cpp11 CACHE STRING "No GLib dependency" FORCE)
    FetchContent_Declare(fluid URL "${FLUIDSYNTH_URL}" URL_HASH "SHA256=${FLUIDSYNTH_SHA256}"
                         DOWNLOAD_EXTRACT_TIMESTAMP FALSE)
    # Match upstream's pinned Apache-2.0 compile-time math dependency, fetched with TLS verification
    FetchContent_Declare(gcem URL "${GCEM_URL}" URL_HASH "SHA256=${GCEM_SHA256}"
                         SOURCE_SUBDIR dxx-populate-only DOWNLOAD_EXTRACT_TIMESTAMP FALSE)
    set(CMAKE_TLS_VERIFY ON)
    FetchContent_MakeAvailable(gcem)
    set(GCEM_INCLUDE_DIR "${gcem_SOURCE_DIR}/include" CACHE PATH "Pinned GCEM" FORCE)
    FetchContent_MakeAvailable(fluid)
    # Build only the linked library, not upstream CLI, examples or test executables
    set_property(DIRECTORY "${fluid_SOURCE_DIR}" PROPERTY EXCLUDE_FROM_ALL TRUE)
    if(ANDROID)
        # Unoptimized DSP starves the audio queue as polyphony grows Keep double precision/effects
        target_compile_options(libfluidsynth-OBJ PRIVATE $<$<CONFIG:Debug>:-O2>)
    endif()

    # MSVC's C enum diagnostic persists even with explicit casts in upstream's flag masks Limit this
    # compatibility exception to that diagnostic in the third-party target
    if(MSVC)
        target_compile_options(libfluidsynth-OBJ PRIVATE /wd5287)
    endif()
    # Make upstream's intentional int-to-float boundary conversion explicit for Clang
    set(cast_file "${fluid_SOURCE_DIR}/src/drivers/fluid_audio_convert.h")
    file(READ "${cast_file}" contents)
    set(before "float fmax = std::numeric_limits<T>::max();")
    set(after "float fmax = static_cast<float>(std::numeric_limits<T>::max());")
    string(FIND "${contents}" "${before}" position)
    if(NOT position EQUAL -1)
        string(REPLACE "${before}" "${after}" contents "${contents}")
        file(WRITE "${cast_file}" "${contents}")
    else()
        string(FIND "${contents}" "${after}" position)
        if(position EQUAL -1)
            message(FATAL_ERROR "Unexpected FluidSynth source while applying numeric cast patch")
        endif()
    endif()

endfunction()
dxx_add_fluidsynth()
