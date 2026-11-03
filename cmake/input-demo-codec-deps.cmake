include_guard(GLOBAL)

include(${CMAKE_CURRENT_LIST_DIR}/dxx-verified-dependencies.cmake)

function(dxx_input_demo_prepare_codec_deps)
    if(NOT TARGET input_demo_picosha2)
        dxx_verified_fetchcontent_declare(input_demo_picosha2_src INPUT_DEMO_PICOSHA2)
        FetchContent_MakeAvailable(input_demo_picosha2_src)
        add_library(input_demo_picosha2 INTERFACE)
        target_include_directories(input_demo_picosha2
                                   INTERFACE ${input_demo_picosha2_src_SOURCE_DIR})
    endif()

    if(NOT TARGET input_demo_cpp_base64)
        dxx_verified_fetchcontent_declare(input_demo_cpp_base64_src INPUT_DEMO_CPP_BASE64)
        FetchContent_MakeAvailable(input_demo_cpp_base64_src)
        add_library(input_demo_cpp_base64 STATIC ${input_demo_cpp_base64_src_SOURCE_DIR}/base64.cpp)
        target_compile_features(input_demo_cpp_base64 PUBLIC cxx_std_11)
        target_include_directories(input_demo_cpp_base64
                                   PUBLIC ${input_demo_cpp_base64_src_SOURCE_DIR})
    endif()
endfunction()

function(dxx_input_demo_apply_codec_deps target)
    dxx_input_demo_prepare_codec_deps()
    target_link_libraries(${target} PRIVATE input_demo_picosha2 input_demo_cpp_base64)
    # Byte-exact SHA-256 must remain enabled in debug playtests without the unoptimized header-only
    # hashing cost on large mission archives
    if(ANDROID)
        foreach(unit input_demo_assets input_demo_codec mod_save_compat)
            set_property(
                SOURCE
                    "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../android/app/src/main/cpp/shared/${unit}.cpp"
                    TARGET_DIRECTORY
                    ${target}
                APPEND
                PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:-O2>")
        endforeach()
        # All instantiations share inline SHA symbols at link time; an unoptimized copy in another
        # client can otherwise replace the optimized hash loop
        set(hash_root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/..")
        foreach(
            unit
            d2/main/d1_in_d2/d1_in_d2_asset_identity.cpp
            android/XL_compat/runtime/classic_audio.cpp
            android/XL_compat/runtime/level_messages.cpp
            android/XL_compat/runtime/named_audio.cpp
            android/XL_compat/runtime/resource_validation.cpp
            android/XL_compat/runtime/lifecycle_test.cpp)
            set_property(SOURCE "${hash_root}/${unit}" TARGET_DIRECTORY ${target} APPEND
                         PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:-O2>")
        endforeach()
    endif()
endfunction()
