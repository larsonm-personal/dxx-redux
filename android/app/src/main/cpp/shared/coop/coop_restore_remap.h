#ifndef COOP_RESTORE_REMAP_H
#define COOP_RESTORE_REMAP_H

#ifdef ANDROID

#include "object.h"
#include "player.h"
#include "rewind_file.h"

int coop_remap_restored_players(rewind_file *file,
                                const player restore_players[MAX_PLAYERS],
                                const object restore_objects[MAX_PLAYERS],
                                const unsigned char restore_object_valid[MAX_PLAYERS],
                                int coop_player_got[MAX_PLAYERS]);
void coop_normalize_restored_netgame_players(const char *game_name);

#endif

#endif
