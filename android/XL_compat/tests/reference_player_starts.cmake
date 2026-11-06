file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/include/player.h" player_hash)
if(NOT player_hash STREQUAL "4e134411347c2dee70f22c7b60c63717390496022964a77c1eb8dc5b5ff15cf4")
    message(FATAL_ERROR "Reference player capacity changed")
endif()
# Parent HXM setup already verifies the exact loadgame.cpp SHA
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/gameio/loadgame.cpp" starts_source)
string(FIND "${starts_source}" "void GameStartInitNetworkPlayers (void)" start)
string(SUBSTRING "${starts_source}" ${start} -1 starts_source)
string(FIND "${starts_source}" "#if DBG" end)
string(SUBSTRING "${starts_source}" 0 ${end} extracted)
string(APPEND extracted "}\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/gameio/loadgame.cpp"
                      "void GameStartRemoveUnusedPlayers (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_player_starts.h"
     CONTENT "${extracted}")
target_sources(xl_hxm_reference PRIVATE tests/reference_player_starts.cpp)
