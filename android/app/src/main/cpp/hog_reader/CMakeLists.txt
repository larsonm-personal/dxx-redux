cmake_minimum_required(VERSION 3.16)
project(dxx_hog_reader LANGUAGES C)

# The same small JNI bridge runs on Android and in launcher JVM tests
add_library(dxx_hog_reader SHARED jni_hog_reader.c)
if(NOT ANDROID)
    find_package(JNI REQUIRED)
    target_include_directories(dxx_hog_reader PRIVATE ${JNI_INCLUDE_DIRS})
    set_target_properties(
        dxx_hog_reader
        PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
                   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
                   LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/lib"
                   RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/lib")
endif()
if(MSVC)
    target_compile_options(dxx_hog_reader PRIVATE /W4 /WX)
else()
    target_compile_options(dxx_hog_reader PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
