include_guard(GLOBAL)

include(${CMAKE_CURRENT_LIST_DIR}/dxx-verified-dependencies.cmake)

function(dxx_audio_tag_prepare_deps)
    if(TARGET tag)
        return()
    endif()

    dxx_verified_fetchcontent_declare(utf8cpp UTF8CPP)
    FetchContent_MakeAvailable(utf8cpp)
    set(utf8cpp_INCLUDE_DIR "${utf8cpp_SOURCE_DIR}/source" CACHE PATH "" FORCE)

    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(BUILD_BINDINGS OFF CACHE BOOL "" FORCE)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(WITH_APE OFF CACHE BOOL "" FORCE)
    set(WITH_ASF OFF CACHE BOOL "" FORCE)
    set(WITH_DSF OFF CACHE BOOL "" FORCE)
    set(WITH_MATROSKA OFF CACHE BOOL "" FORCE)
    set(WITH_MOD OFF CACHE BOOL "" FORCE)
    set(WITH_MP4 OFF CACHE BOOL "" FORCE)
    set(WITH_RIFF OFF CACHE BOOL "" FORCE)
    set(WITH_SHORTEN OFF CACHE BOOL "" FORCE)
    set(WITH_TRUEAUDIO OFF CACHE BOOL "" FORCE)
    set(WITH_VORBIS ON CACHE BOOL "" FORCE)
    set(WITH_ZLIB OFF CACHE BOOL "" FORCE)
    set(VISIBILITY_HIDDEN ON CACHE BOOL "" FORCE)
    dxx_verified_fetchcontent_declare(taglib TAGLIB)
    FetchContent_MakeAvailable(taglib)
endfunction()

function(dxx_audio_tag_apply target)
    dxx_audio_tag_prepare_deps()
    get_target_property(_tag_source_dir tag SOURCE_DIR)
    get_target_property(_tag_binary_dir tag BINARY_DIR)
    target_sources(
        ${target}
        PRIVATE
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../android/app/src/main/cpp/shared/audio_tag_metadata.cpp
    )
    target_include_directories(
        ${target}
        PRIVATE ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../android/app/src/main/cpp/shared
                ${_tag_source_dir}
                ${_tag_source_dir}/toolkit
                ${_tag_source_dir}/mpeg
                ${_tag_source_dir}/mpeg/id3v1
                ${_tag_source_dir}/mpeg/id3v2
                ${_tag_source_dir}/mpeg/id3v2/frames
                ${_tag_source_dir}/ogg
                ${_tag_source_dir}/ogg/flac
                ${_tag_source_dir}/ogg/vorbis
                ${_tag_source_dir}/flac
                ${_tag_binary_dir}/..)
    target_compile_features(${target} PRIVATE cxx_std_17)
    target_link_libraries(${target} PRIVATE tag)
endfunction()
