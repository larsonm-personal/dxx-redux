#ifndef REJOIN_DIAGNOSTICS_H
#define REJOIN_DIAGNOSTICS_H

/* Android rejoin diagnostics in the launcher's optional Network log */
#ifdef __ANDROID__
#include <stdint.h>
#include "android_log.h"
#include "game.h"
#include "gameseq.h"
#include "multi.h"
#include "object.h"
#include "player.h"

#define REJOIN_LOG(fmt, ...) debug_log(DLOG_NETWORK, "[REJOIN] " fmt, ##__VA_ARGS__)

static inline void rejoin_diag_world(const char *stage)
{
	if (!debug_log_enabled[DLOG_NETWORK] || !(Game_mode & GM_MULTI_COOP)) return;
	int live = 0, ships = 0;
	for (int i = 0; i < MAX_OBJECTS; ++i) {
		live += Objects[i].type != OBJ_NONE;
		ships += Objects[i].type == OBJ_PLAYER || Objects[i].type == OBJ_GHOST;
	}
	REJOIN_LOG("world stage=%s local=%d master=%d level=%d status=%d rejoined=%d players=%d starts=%d allocated=%d live=%d highest=%d ships=%d sending=%d cursor=%d mode=%d verify=%d",
	           stage, Player_num, multi_who_is_master(), Current_level_num, Network_status,
	           Network_rejoined, N_players, NumNetPlayerPositions, num_objects, live,
	           Highest_object_index, ships, Network_send_objects, Network_send_objnum,
	           Network_send_object_mode, VerifyPlayerJoined);
	for (int i = 0; i < MAX_PLAYERS; ++i) {
		const int index = Players[i].objnum;
		const object *obj = index >= 0 && index < MAX_OBJECTS ? &Objects[index] : NULL;
		REJOIN_LOG("player stage=%s slot=%d connected=%d obj=%d type=%d id=%d seg=%d owner=%d",
		           stage, i, Players[i].connected, index, obj ? obj->type : -1,
		           obj ? obj->id : -1, obj ? obj->segnum : -1, obj ? object_owner[index] : -2);
	}
}

/* Only the UDP translation unit needs packet layout diagnostics */
#ifdef REJOIN_DIAGNOSTICS_UDP
#include "byteswap.h"
#include "net_udp.h"
#include "net_udp_join_wait.h"

static inline void rejoin_diag_packet(const char *stage, const void *buffer, int length, int result)
{
	if (!debug_log_enabled[DLOG_NETWORK]) return;
	const ubyte *data = (const ubyte *) buffer;
	if (length < 1 || (data[0] != UPID_JOIN_DATA && data[0] != UPID_OBJECT_DATA && data[0] != UPID_SYNC)) return;
	uint32_t hash = 2166136261u;
	for (int i = 0; i < length; ++i) hash = (hash ^ data[i]) * 16777619u;
	const int outer = data[0];
	uint32_t attempt = 0;
	if (outer == UPID_JOIN_DATA && length > UPID_JOIN_DATA_HEADER) {
		attempt = (uint32_t) GET_INTEL_INT(data + 5);
		data += UPID_JOIN_DATA_HEADER;
	}
	const int inner_length = length - (int) (data - (const ubyte *) buffer);
#ifdef DXX_BUILD_DESCENT_II
	const int header = UPID_OBJECT_HEADER_SIZE;
#else
	const int header = 9;
#endif
	const int objects = data[0] == UPID_OBJECT_DATA;
	const int count = objects && inner_length >= 9 ? GET_INTEL_INT(data + 5) : -1;
	const int first = objects && inner_length >= header + 9 ? GET_INTEL_INT(data + header) : -3;
	const int owner = objects && inner_length >= header + 9 ? (signed char) data[header + 4] : -2;
	const int remote = objects && inner_length >= header + 9 ? GET_INTEL_INT(data + header + 5) : -1;
	REJOIN_LOG("packet stage=%s bytes=%d result=%d hash=%08x outer=%d inner=%d attempt=%u nobj=%d first=%d owner=%d remote=%d local=%d master=%d status=%d",
	           stage, length, result, (unsigned) hash, outer, data[0], (unsigned) attempt,
	           count, first, owner, remote, Player_num, multi_who_is_master(), Network_status);
}
#endif
#endif
#endif
