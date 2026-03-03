# Android native build for DXX-Redux
#
# For now this is a hello-world JNI library that verifies the NDK toolchain.
# The next step will be to replace this with the actual d1/d2 game code.

cmake_minimum_required(VERSION 3.22)
project(dxx-redux-android LANGUAGES C CXX)

# Include the NDK auto-discovery helper (sets ANDROID_NDK if not already set).
# When invoked through Gradle's externalNativeBuild the NDK is already
# configured, but this is useful for standalone cmake invocations.
include(${CMAKE_CURRENT_SOURCE_DIR}/../../../../../cmake/ndk-auto.cmake OPTIONAL)

# --- Hello-world JNI library (smoke test) ---
add_library(dxx-redux SHARED native-lib.cpp)

target_link_libraries(dxx-redux
    android   # native Android APIs
    log       # __android_log_print
)

# TODO: Replace the above with the real d1/d2 build by including the game
# CMakeLists.txt and adjusting dependencies for Android (OpenGL ES, SDL2, etc.)
