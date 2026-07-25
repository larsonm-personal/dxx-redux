#ifndef COOP_PLAYER_SESSION_H
#define COOP_PLAYER_SESSION_H

#include <string.h>

#include "player.h"

/*
 * A coop save owns gameplay state, but the running network session owns player
 * identity, connection state, object slots, and packet counters.
 */
static inline void coop_restore_player_game_state(
    player *live_player, const player *restored_player)
{
	player live_session = *live_player;

	memcpy(live_player, restored_player, sizeof(*live_player));
	memcpy(live_player->callsign, live_session.callsign,
	       sizeof(live_player->callsign));
	memcpy(live_player->net_address, live_session.net_address,
	       sizeof(live_player->net_address));
	live_player->connected = live_session.connected;
	live_player->objnum = live_session.objnum;
	live_player->n_packets_got = live_session.n_packets_got;
	live_player->n_packets_sent = live_session.n_packets_sent;
}

static inline int coop_sync_identity_matches(
    const char *live_client_id, const char *candidate_client_id,
    int legacy_identity_matches)
{
	return (live_client_id && live_client_id[0] && candidate_client_id &&
	        candidate_client_id[0] &&
	        !strcmp(live_client_id, candidate_client_id)) ||
	       legacy_identity_matches;
}

#endif /* COOP_PLAYER_SESSION_H */
