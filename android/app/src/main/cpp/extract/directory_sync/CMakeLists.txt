cmake_minimum_required(VERSION 3.16)
project(graphics_directory_sync C)
# Descriptor inventory uses /proc on Linux and Android
enable_testing()
add_executable(test_graphics_directory_sync main.c)
target_compile_features(test_graphics_directory_sync PRIVATE c_std_11)
target_compile_definitions(test_graphics_directory_sync PRIVATE NDEBUG
                                                                GRAPHICS_CONFIG_TRANSACTION_TESTING)
target_compile_options(test_graphics_directory_sync PRIVATE -Wall -Wextra -Werror)
add_test(NAME graphics_directory_sync COMMAND test_graphics_directory_sync)
