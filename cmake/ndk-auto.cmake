# cmake/ndk-auto.cmake
#
# Auto-discover the Android NDK.  Returns the result in ANDROID_NDK (cache).
#
# Search order:
#   1. ANDROID_NDK_ROOT / ANDROID_NDK environment variables (standard)
#   2. NDK_ROOT environment variable
#   3. Newest android-ndk-* folder under dependency_base.txt dir (or C:/local fallback)
#   4. Common SDK locations (<SDK>/ndk/<version> and <SDK>/ndk-bundle)
#   5. Common standalone install locations
#
# Usage from Gradle / CMake:
#   include(${CMAKE_CURRENT_LIST_DIR}/../cmake/ndk-auto.cmake)
#   -- after return, ANDROID_NDK is set if found.

if(DEFINED _NDK_AUTO_INCLUDED)
    return()
endif()
set(_NDK_AUTO_INCLUDED TRUE)

# ---------------------------------------------------------------------------
function(_ndk_try_root candidate result_var)
    if(${result_var})
        return()
    endif()
    file(TO_CMAKE_PATH "${candidate}" _ndk_candidate)
    # The NDK ships android.toolchain.cmake here:
    if(EXISTS "${_ndk_candidate}/build/cmake/android.toolchain.cmake")
        set(${result_var} "${_ndk_candidate}" PARENT_SCOPE)
    endif()
endfunction()

set(_NDK_ROOT "")

# 1-2. Environment variables
foreach(_envvar ANDROID_NDK_ROOT ANDROID_NDK NDK_ROOT)
    if(DEFINED ENV{${_envvar}})
        _ndk_try_root("$ENV{${_envvar}}" _NDK_ROOT)
    endif()
endforeach()

# 3. Scan dependency base dir for newest android-ndk-* folder (Windows)
if(NOT _NDK_ROOT AND WIN32)
    # Read dependency_base.txt from repo root
    set(_dep_base_file "${CMAKE_CURRENT_LIST_DIR}/../dependency_base.txt")
    if(EXISTS "${_dep_base_file}")
        file(READ "${_dep_base_file}" _dep_base)
        string(STRIP "${_dep_base}" _dep_base)
        file(TO_CMAKE_PATH "${_dep_base}" _dep_base)
        file(GLOB _ndk_candidates LIST_DIRECTORIES true "${_dep_base}/android-ndk-*")
    else()
        file(GLOB _ndk_candidates LIST_DIRECTORIES true "C:/local/android-ndk-*")
    endif()
    if(_ndk_candidates)
        list(SORT _ndk_candidates ORDER DESCENDING)
        foreach(_c IN LISTS _ndk_candidates)
            _ndk_try_root("${_c}" _NDK_ROOT)
        endforeach()
    endif()
endif()

# 4. SDK-relative paths (ANDROID_HOME / ANDROID_SDK_ROOT)
foreach(_sdk_env ANDROID_HOME ANDROID_SDK_ROOT)
    if(NOT _NDK_ROOT AND DEFINED ENV{${_sdk_env}})
        file(TO_CMAKE_PATH "$ENV{${_sdk_env}}" _sdk)
        # Newest side-by-side NDK
        file(GLOB _ndk_side LIST_DIRECTORIES true "${_sdk}/ndk/*")
        if(_ndk_side)
            list(SORT _ndk_side ORDER DESCENDING)
            foreach(_c IN LISTS _ndk_side)
                _ndk_try_root("${_c}" _NDK_ROOT)
            endforeach()
        endif()
        # Legacy ndk-bundle
        _ndk_try_root("${_sdk}/ndk-bundle" _NDK_ROOT)
    endif()
endforeach()

# 5. Common standalone install locations
if(NOT _NDK_ROOT)
    if(WIN32)
        _ndk_try_root("C:/Android/ndk" _NDK_ROOT)
    else()
        if(DEFINED ENV{HOME})
            _ndk_try_root("$ENV{HOME}/Android/Sdk/ndk-bundle" _NDK_ROOT)
        endif()
        _ndk_try_root("/opt/android-ndk" _NDK_ROOT)
    endif()
endif()

# ---------------------------------------------------------------------------
if(_NDK_ROOT)
    message(STATUS "ndk-auto: found Android NDK at ${_NDK_ROOT}")
    set(ANDROID_NDK "${_NDK_ROOT}" CACHE PATH "Android NDK (auto-discovered)" FORCE)
else()
    message(WARNING
        "ndk-auto: Android NDK not found. Install the NDK and either:\n"
        "  - set ANDROID_NDK_ROOT environment variable, or\n"
        "  - set the path in dependency_base.txt and place the NDK under it\n"
    )
endif()

unset(_NDK_ROOT)
