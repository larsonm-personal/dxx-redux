foreach(
    pair IN
    ITEMS "main/trigger.cpp|74f1284260fb4a4bb7396bc45eeb83868a7f2dfa5e3fdf0bcb767b94f1b12852"
          "objects/player.cpp|bab2ca407a6ac51993fb1194b785caa8101fe0a8a5896bd8a010cf68e07c09e5"
          "include/player.h|4e134411347c2dee70f22c7b60c63717390496022964a77c1eb8dc5b5ff15cf4")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Player-resource source differs: ${path}")
    endif()
endforeach()
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/player.h" player_header)
string(REGEX MATCH "inline fix Update \\(fix delta\\)[^{]*\\{[^}]*\\}" update "${player_header}")
if(NOT update)
    message(FATAL_ERROR "Missing pinned inline resource update")
endif()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_resource_update.h"
     CONTENT "${update}")
set(extracted "")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/objects/player.cpp" "bool CShipEnergy::Set (")
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/main/trigger.cpp" source)
string(FIND "${source}" "\tcase TT_SHIELD_DAMAGE:" start)
if(start LESS 0)
    message(FATAL_ERROR "Missing player-resource switch cases")
endif()
string(SUBSTRING "${source}" ${start} -1 tail)
string(FIND "${tail}" "\tcase TT_CHANGE_TEXTURE:" end)
if(end LESS 0)
    message(FATAL_ERROR "Missing player-resource switch boundary")
endif()
string(SUBSTRING "${tail}" 0 ${end} cases)
string(
    APPEND extracted
    "\nvoid reference_resource_action(int type, int nTrigger) {\nswitch(type) {\n${cases}\n}\n}\n")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_player_resources.h"
     CONTENT "${extracted}")
add_executable(xl_player_resources_reference tests/reference_player_resources.cpp)
target_compile_features(xl_player_resources_reference PRIVATE cxx_std_17)
target_include_directories(xl_player_resources_reference PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME xl_player_resource_reference COMMAND xl_player_resources_reference)
