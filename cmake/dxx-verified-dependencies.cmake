include_guard(GLOBAL)

include(FetchContent)

get_filename_component(_DXX_VERIFIED_DEPS_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
if(NOT DEFINED DXX_DEPENDENCY_MANIFEST)
    set(DXX_DEPENDENCY_MANIFEST "${_DXX_VERIFIED_DEPS_ROOT}/android/get_deps/tool_versions.conf")
endif()
if(NOT DEFINED DXX_DEPENDENCY_CACHE_DIR)
    set(DXX_DEPENDENCY_CACHE_DIR "${CMAKE_BINARY_DIR}/dxx_dependency_cache")
endif()

function(dxx_dependency_value key output)
    file(STRINGS "${DXX_DEPENDENCY_MANIFEST}" value REGEX "^${key}=" LIMIT_COUNT 1)
    if(NOT value)
        message(FATAL_ERROR "${key} is missing from ${DXX_DEPENDENCY_MANIFEST}")
    endif()
    string(REGEX REPLACE "^[^=]+=" "" value "${value}")
    set(${output} "${value}" PARENT_SCOPE)
endfunction()

function(dxx_verified_cached_file prefix suffix output)
    dxx_dependency_value(${prefix}_URL dependency_url)
    dxx_dependency_value(${prefix}_SHA256 dependency_sha256)
    string(TOLOWER "${dependency_sha256}" dependency_sha256)
    file(MAKE_DIRECTORY "${DXX_DEPENDENCY_CACHE_DIR}")
    set(cached_file "${DXX_DEPENDENCY_CACHE_DIR}/${prefix}-${dependency_sha256}.${suffix}")
    set(lock_file "${DXX_DEPENDENCY_CACHE_DIR}/.${prefix}.lock")
    file(LOCK "${lock_file}" GUARD FUNCTION TIMEOUT 400)

    if(EXISTS "${cached_file}")
        file(SHA256 "${cached_file}" cached_sha256)
        string(TOLOWER "${cached_sha256}" cached_sha256)
        if(NOT cached_sha256 STREQUAL dependency_sha256)
            message(
                FATAL_ERROR
                    "Cached ${prefix} SHA-256 mismatch: expected ${dependency_sha256}, got ${cached_sha256}"
            )
        endif()
    else()
        string(RANDOM LENGTH 16 ALPHABET 0123456789abcdef operation_id)
        set(temporary_file "${cached_file}.${operation_id}.tmp")
        # Check the hash only after transport success: EXPECTED_HASH aborts CMake on a partial
        # transfer before STATUS can drive a retry
        foreach(attempt RANGE 1 3 1)
            message(STATUS "Downloading ${prefix}, attempt ${attempt}/3 (120s limit)")
            file(
                DOWNLOAD "${dependency_url}" "${temporary_file}"
                TLS_VERIFY ON
                TIMEOUT 120
                INACTIVITY_TIMEOUT 30
                STATUS download_status)
            list(GET download_status 0 download_code)
            if(download_code EQUAL 0)
                break()
            endif()
            file(REMOVE "${temporary_file}")
            # Retry transient DNS, connection, partial transfer and timeout errors
            #
            # HTTP, TLS and local filesystem failures remain fail-closed
            if(attempt EQUAL 3 OR NOT download_code MATCHES "^(5|6|7|18|28|52|55|56)$")
                message(
                    FATAL_ERROR
                        "Failed to download ${prefix} after ${attempt} attempt(s): ${download_status}"
                )
            endif()
            math(EXPR retry_delay "${attempt} * 2")
            message(STATUS "${prefix}: ${download_status}; retrying in ${retry_delay}s")
            execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep "${retry_delay}")
        endforeach()
        file(SHA256 "${temporary_file}" downloaded_sha256)
        if(NOT downloaded_sha256 STREQUAL dependency_sha256)
            file(REMOVE "${temporary_file}")
            message(
                FATAL_ERROR
                    "Downloaded ${prefix} SHA-256 mismatch: expected ${dependency_sha256}, got ${downloaded_sha256}"
            )
        endif()
        file(RENAME "${temporary_file}" "${cached_file}")
    endif()
    set(${output} "${cached_file}" PARENT_SCOPE)
endfunction()

function(dxx_verified_fetchcontent_declare name prefix)
    dxx_verified_cached_file(${prefix} "tar.gz" dependency_archive)
    dxx_dependency_value(${prefix}_SHA256 dependency_sha256)
    FetchContent_Declare(${name} URL "${dependency_archive}" URL_HASH "SHA256=${dependency_sha256}"
                         DOWNLOAD_EXTRACT_TIMESTAMP FALSE)
endfunction()

function(dxx_verified_source prefix destination)
    dxx_verified_cached_file(${prefix} "source" cached_source)
    configure_file("${cached_source}" "${destination}" COPYONLY)
endfunction()
