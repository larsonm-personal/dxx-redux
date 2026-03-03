# cmake/vcpkg-auto.cmake
#
# Auto-discover vcpkg and include its CMake toolchain.
# Use this as CMAKE_TOOLCHAIN_FILE instead of pointing directly at vcpkg.cmake.
#
# Search order:
#   1. VCPKG_ROOT environment variable (standard vcpkg convention)
#   2. VCPKG_INSTALLATION_ROOT environment variable (GitHub Actions / VS Dev Shell)
#   3. Visual Studio bundled vcpkg (all editions, all versions)
#   4. Common user install locations (C:/vcpkg, ~/vcpkg, etc.)
#   5. vcpkg on PATH (find_program fallback)
#
# Once found, includes vcpkg.cmake transparently. If not found, issues a
# warning but does not hard-fail, so non-vcpkg builds still work.

if(DEFINED _VCPKG_AUTO_INCLUDED)
    return()
endif()
set(_VCPKG_AUTO_INCLUDED TRUE)

# Use a function (not a macro) to avoid CMake macro textual substitution,
# which would re-parse backslashes in Windows paths as escape sequences.
function(_vcpkg_try_root candidate result_var)
    if(${result_var})
        return()
    endif()
    file(TO_CMAKE_PATH "${candidate}" _vcpkg_candidate)
    if(EXISTS "${_vcpkg_candidate}/scripts/buildsystems/vcpkg.cmake")
        set(${result_var} "${_vcpkg_candidate}" PARENT_SCOPE)
    endif()
endfunction()

set(_VCPKG_ROOT "")

# 1 & 2. Environment variables
if(DEFINED ENV{VCPKG_ROOT})
    _vcpkg_try_root("$ENV{VCPKG_ROOT}" _VCPKG_ROOT)
endif()
if(DEFINED ENV{VCPKG_INSTALLATION_ROOT})
    _vcpkg_try_root("$ENV{VCPKG_INSTALLATION_ROOT}" _VCPKG_ROOT)
endif()

# 3. Visual Studio bundled vcpkg (glob across all VS versions/editions)
if(NOT _VCPKG_ROOT AND WIN32)
    foreach(_vs_base
        "C:/Program Files/Microsoft Visual Studio"
        "C:/Program Files (x86)/Microsoft Visual Studio"
    )
        if(IS_DIRECTORY "${_vs_base}")
            file(GLOB _vs_versions LIST_DIRECTORIES true "${_vs_base}/*")
            foreach(_vs_ver IN LISTS _vs_versions)
                foreach(_edition Community Professional Enterprise Preview BuildTools)
                    _vcpkg_try_root("${_vs_ver}/${_edition}/VC/vcpkg" _VCPKG_ROOT)
                endforeach()
            endforeach()
        endif()
    endforeach()
endif()

# 4. Common user install locations
if(NOT _VCPKG_ROOT)
    if(WIN32)
        _vcpkg_try_root("C:/vcpkg" _VCPKG_ROOT)
        _vcpkg_try_root("C:/src/vcpkg" _VCPKG_ROOT)
        if(DEFINED ENV{USERPROFILE})
            _vcpkg_try_root("$ENV{USERPROFILE}/vcpkg" _VCPKG_ROOT)
        endif()
        if(DEFINED ENV{LOCALAPPDATA})
            _vcpkg_try_root("$ENV{LOCALAPPDATA}/vcpkg" _VCPKG_ROOT)
        endif()
    else()
        if(DEFINED ENV{HOME})
            _vcpkg_try_root("$ENV{HOME}/vcpkg" _VCPKG_ROOT)
        endif()
        _vcpkg_try_root("/usr/local/vcpkg" _VCPKG_ROOT)
        _vcpkg_try_root("/opt/vcpkg" _VCPKG_ROOT)
    endif()
endif()

# 5. find_program fallback — derive root from executable location
if(NOT _VCPKG_ROOT)
    find_program(_VCPKG_EXE vcpkg PATHS
        ENV PATH
        DOC "vcpkg package manager"
    )
    if(_VCPKG_EXE)
        get_filename_component(_vcpkg_dir "${_VCPKG_EXE}" DIRECTORY)
        if(EXISTS "${_vcpkg_dir}/scripts/buildsystems/vcpkg.cmake")
            set(_VCPKG_ROOT "${_vcpkg_dir}")
        endif()
    endif()
    unset(_VCPKG_EXE CACHE)
endif()

# --- include or warn ---
if(_VCPKG_ROOT)
    message(STATUS "vcpkg-auto: found vcpkg at ${_VCPKG_ROOT}")
    set(VCPKG_ROOT "${_VCPKG_ROOT}" CACHE PATH "vcpkg root (auto-discovered)" FORCE)
    include("${_VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
else()
    message(WARNING
        "vcpkg-auto: vcpkg not found. Install vcpkg and either:\n"
        "  - set the VCPKG_ROOT environment variable, or\n"
        "  - install to a standard location (C:/vcpkg, ~/vcpkg, etc.)\n"
        "Continuing without vcpkg — find_package calls may fail."
    )
endif()

unset(_VCPKG_ROOT)
