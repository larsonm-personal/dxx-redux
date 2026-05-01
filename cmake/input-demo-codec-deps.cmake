include_guard(GLOBAL)

include(FetchContent)

function(dxx_input_demo_prepare_codec_deps)
    if(NOT TARGET input_demo_picosha2)
        FetchContent_Declare(
            input_demo_picosha2_src GIT_REPOSITORY https://github.com/okdshin/PicoSHA2.git
            GIT_TAG 161cb3fc4170fa7a3eca9e582cebd27cc4d1fe29 GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(input_demo_picosha2_src)
        add_library(input_demo_picosha2 INTERFACE)
        target_include_directories(input_demo_picosha2
                                   INTERFACE ${input_demo_picosha2_src_SOURCE_DIR})
    endif()

    if(NOT TARGET input_demo_cpp_base64)
        FetchContent_Declare(
            input_demo_cpp_base64_src
            GIT_REPOSITORY https://github.com/renenyffenegger/cpp-base64.git
            GIT_TAG 951de609dbe27ce8864dfe47323c4ade96bee86e GIT_SHALLOW TRUE)
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
endfunction()
