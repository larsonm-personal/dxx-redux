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
endfunction()
