foreach(
    pair IN
    ITEMS "include/trigger.h|8f539027666ce768f467226381e82fd62c07a3cd3642fe97f857980091cf4dfd"
          "gameio/savegame.cpp|4784770058b2d4880c33a81eb1a7611738959113a80bce48de248eee7a54a795"
          "main/wall.cpp|05eaa2cab8dd7f5661afd210e0edc8dea4fcd2bb2075c00d3012bec395ab0c42")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Trigger capacity reference changed: ${path}")
    endif()
endforeach()
file(STRINGS "${XL_REFERENCE_GEOMETRY_ROOT}/include/trigger.h" constants
     REGEX "^#define[ \t]+(MAX_TRIGGERS|MAX_OBJ_TRIGGERS)[ \t]+")
list(JOIN constants "\n" constants_text)
set(extracted "${constants_text}\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/wall.cpp" "void CWall::Read (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/gameio/savegame.cpp"
                      "int32_t CSaveGameManager::ReadBoundedInt (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_trigger_capacity.h"
     CONTENT "${extracted}")
add_executable(xl_trigger_capacity_reference tests/reference_trigger_capacity.cpp)
target_include_directories(xl_trigger_capacity_reference PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(xl_trigger_capacity_reference PRIVATE cxx_std_17)
add_test(NAME xl_trigger_capacity_reference COMMAND xl_trigger_capacity_reference)
