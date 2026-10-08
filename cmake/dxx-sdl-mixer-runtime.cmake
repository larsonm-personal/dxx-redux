include_guard(GLOBAL)

function(dxx_copy_sdl_mixer_runtime target)
    if(SDLMIXER AND MSVC AND VCPKG_INSTALLED_DIR AND VCPKG_TARGET_TRIPLET)
        set(VCPKG_PREFIX "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
        # SDL_mixer loads these codecs dynamically, so vcpkg's import scan misses them
        set(MUSIC_DLL_DIR "${VCPKG_PREFIX}/$<IF:$<CONFIG:Debug>,debug/bin,bin>")
        foreach(lib mpg123 FLAC mikmod ogg vorbis vorbisfile)
            if(EXISTS "${VCPKG_PREFIX}/bin/${lib}.dll")
                add_custom_command(
                    TARGET ${target} POST_BUILD COMMENT "Copying SDL_mixer codec ${lib}"
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MUSIC_DLL_DIR}/${lib}.dll"
                            $<TARGET_FILE_DIR:${target}>)
                install(FILES "${MUSIC_DLL_DIR}/${lib}.dll" DESTINATION .)
            endif()
        endforeach()
    endif()
endfunction()
