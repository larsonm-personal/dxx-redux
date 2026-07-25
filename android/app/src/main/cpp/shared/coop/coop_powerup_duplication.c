#include <stdlib.h>
#include <string.h>

#include "byteswap.h"
#include "coop_powerup_duplication.h"
#include "game.h"
#include "multi.h"
#include "player.h"
#include "powerup.h"
#include "strutil.h"

static coop_powerup_collection *collections;
static size_t collection_count;
static size_t collection_capacity;
static coop_powerup_collection *pending_collections;
static size_t pending_count;
static coop_powerup_collection *snapshot_collections;
static size_t snapshot_count;
static size_t snapshot_expected_count;
static int snapshot_receiving;

static void prune_collections(void)
{
	size_t read_index;
	size_t write_index = 0;

	for (read_index = 0; read_index < collection_count; read_index++) {
		const coop_powerup_collection *item = &collections[read_index];
		const object *powerup;

		if (item->object_index < 0 ||
		    item->object_index > Highest_object_index)
			continue;
		powerup = &Objects[item->object_index];
		if (powerup->signature != item->object_signature ||
		    powerup->id != item->powerup_id ||
		    (powerup->flags & OF_SHOULD_BE_DEAD) ||
		    !coop_powerup_duplication_eligible(powerup))
			continue;
		if (write_index != read_index)
			collections[write_index] = collections[read_index];
		write_index++;
	}
	collection_count = write_index;
}

static void collection_identity(int pnum, char *client_id, char *callsign)
{
	client_id[0] = '\0';
	callsign[0] = '\0';
	if (pnum < 0 || pnum >= MAX_PLAYERS)
		return;
	strncpy(client_id, Netgame.players[pnum].client_id,
	        COOP_POWERUP_CLIENT_ID_LEN);
	client_id[COOP_POWERUP_CLIENT_ID_LEN] = '\0';
	strncpy(callsign, Players[pnum].callsign, COOP_POWERUP_CALLSIGN_LEN);
	callsign[COOP_POWERUP_CALLSIGN_LEN] = '\0';
}

static int same_identity(const coop_powerup_collection *item,
                         const char *client_id, const char *callsign)
{
	if (client_id[0] && item->client_id[0])
		return !strcmp(item->client_id, client_id);
	return callsign[0] && item->callsign[0] &&
	       !d_stricmp(item->callsign, callsign);
}

static int same_collection(const coop_powerup_collection *item,
                           const object *powerup, const char *client_id,
                           const char *callsign)
{
	return item->object_signature == powerup->signature &&
	       item->powerup_id == powerup->id &&
	       same_identity(item, client_id, callsign);
}

static int reserve_collections(size_t needed)
{
	coop_powerup_collection *grown;
	size_t capacity;

	if (needed <= collection_capacity)
		return 1;
	capacity = collection_capacity ? collection_capacity * 2 : 32;
	while (capacity < needed)
		capacity *= 2;
	grown = (coop_powerup_collection *) realloc(
	    collections, capacity * sizeof(*collections));
	if (!grown)
		return 0;
	collections = grown;
	collection_capacity = capacity;
	return 1;
}

int coop_powerup_duplication_active(void)
{
	return (Game_mode & GM_MULTI_COOP) &&
	       Netgame.DuplicateEnergyShields;
}

int coop_powerup_duplication_eligible(const object *powerup)
{
	if (!coop_powerup_duplication_active() || !powerup ||
	    powerup->type != OBJ_POWERUP ||
	    (powerup->id != POW_ENERGY &&
	     powerup->id != POW_SHIELD_BOOST) ||
	    (powerup->flags & OF_PLAYER_DROPPED))
		return 0;
#ifdef DXX_BUILD_DESCENT_II
	if (powerup->ctype.powerup_info.flags & PF_SPAT_BY_PLAYER)
		return 0;
#endif
	return 1;
}

int coop_powerup_duplication_collected(const object *powerup, int pnum)
{
	char client_id[COOP_POWERUP_CLIENT_ID_LEN + 1];
	char callsign[COOP_POWERUP_CALLSIGN_LEN + 1];
	size_t i;

	if (!coop_powerup_duplication_eligible(powerup))
		return 0;
	collection_identity(pnum, client_id, callsign);
	for (i = 0; i < collection_count; i++)
		if (same_collection(&collections[i], powerup, client_id, callsign))
			return 1;
	return 0;
}

int coop_powerup_duplication_record(const object *powerup, int pnum)
{
	coop_powerup_collection *item;
	char client_id[COOP_POWERUP_CLIENT_ID_LEN + 1];
	char callsign[COOP_POWERUP_CALLSIGN_LEN + 1];

	if (!coop_powerup_duplication_eligible(powerup) ||
	    pnum < 0 || pnum >= MAX_PLAYERS)
		return 0;
	if (coop_powerup_duplication_collected(powerup, pnum))
		return 1;
	collection_identity(pnum, client_id, callsign);
	if (!client_id[0] && !callsign[0])
		return 0;
	if (!reserve_collections(collection_count + 1))
		return 0;
	item = &collections[collection_count++];
	memset(item, 0, sizeof(*item));
	item->object_index = (int16_t) (powerup - Objects);
	item->object_signature = powerup->signature;
	item->powerup_id = (uint8_t) powerup->id;
	memcpy(item->client_id, client_id, sizeof(item->client_id));
	memcpy(item->callsign, callsign, sizeof(item->callsign));
	return 1;
}

int coop_powerup_duplication_hide(const object *powerup)
{
	return coop_powerup_duplication_collected(powerup, Player_num);
}

void coop_powerup_duplication_reset(void)
{
	free(collections);
	free(pending_collections);
	free(snapshot_collections);
	collections = NULL;
	pending_collections = NULL;
	snapshot_collections = NULL;
	collection_count = 0;
	collection_capacity = 0;
	pending_count = 0;
	snapshot_count = 0;
	snapshot_expected_count = 0;
	snapshot_receiving = 0;
}

size_t coop_powerup_duplication_count(void)
{
	prune_collections();
	return collection_count;
}

const coop_powerup_collection *coop_powerup_duplication_data(void)
{
	return collections;
}

int coop_powerup_duplication_replace(const coop_powerup_collection *items,
                                     size_t count)
{
	coop_powerup_collection *replacement = NULL;

	if (count) {
		if (!items || count > SIZE_MAX / sizeof(*items))
			return 0;
		replacement = (coop_powerup_collection *) malloc(
		    count * sizeof(*items));
		if (!replacement)
			return 0;
		memcpy(replacement, items, count * sizeof(*items));
	}
	free(collections);
	collections = replacement;
	collection_count = count;
	collection_capacity = count;
	return 1;
}

int coop_powerup_duplication_set_pending(
    const coop_powerup_collection *items, size_t count)
{
	coop_powerup_collection *replacement = NULL;

	if (count) {
		if (!items || count > SIZE_MAX / sizeof(*items))
			return 0;
		replacement = (coop_powerup_collection *) malloc(
		    count * sizeof(*items));
		if (!replacement)
			return 0;
		memcpy(replacement, items, count * sizeof(*items));
	}
	free(pending_collections);
	pending_collections = replacement;
	pending_count = count;
	return 1;
}

int coop_powerup_duplication_apply_pending(void)
{
	coop_powerup_collection *validated = NULL;
	size_t validated_count = 0;
	size_t i;

	if (!Netgame.DuplicateEnergyShields && pending_count)
		return 0;
	if (pending_count) {
		validated = (coop_powerup_collection *) malloc(
		    pending_count * sizeof(*validated));
		if (!validated)
			return 0;
	}
	for (i = 0; i < pending_count; i++) {
		coop_powerup_collection item = pending_collections[i];
		int objnum = item.object_index;
		int j;

		if (!memchr(item.client_id, '\0', sizeof(item.client_id)) ||
		    !memchr(item.callsign, '\0', sizeof(item.callsign)) ||
		    (!item.client_id[0] && !item.callsign[0])) {
			free(validated);
			return 0;
		}
		if (objnum < 0 || objnum > Highest_object_index ||
		    Objects[objnum].signature != item.object_signature) {
			objnum = -1;
			for (j = 0; j <= Highest_object_index; j++)
				if (Objects[j].signature == item.object_signature) {
					if (objnum >= 0) {
						free(validated);
						return 0;
					}
					objnum = j;
				}
		}
		if (objnum < 0 || Objects[objnum].id != item.powerup_id ||
		    !coop_powerup_duplication_eligible(&Objects[objnum])) {
			free(validated);
			return 0;
		}
		item.object_index = (int16_t) objnum;
		for (j = 0; j < (int) validated_count; j++)
			if (validated[j].object_signature == item.object_signature &&
			    validated[j].powerup_id == item.powerup_id &&
			    same_identity(&validated[j], item.client_id,
			                  item.callsign)) {
				free(validated);
				return 0;
			}
		validated[validated_count++] = item;
	}
	if (!coop_powerup_duplication_replace(validated, validated_count)) {
		free(validated);
		return 0;
	}
	free(validated);
	free(pending_collections);
	pending_collections = NULL;
	pending_count = 0;
	return 1;
}

void coop_powerup_duplication_send(const object *powerup)
{
	sbyte owner;
	short remote_objnum;

	if (!coop_powerup_duplication_eligible(powerup))
		return;
	remote_objnum = (short) objnum_local_to_remote(
	    (int) (powerup - Objects), &owner);
	multibuf[0] = MULTI_COOP_POWERUP_COLLECTED;
	PUT_INTEL_SHORT(multibuf + 1, remote_objnum);
	multibuf[3] = (ubyte) owner;
	multibuf[4] = (ubyte) Player_num;
	PUT_INTEL_INT(multibuf + 5, powerup->signature);
	multibuf[9] = (ubyte) powerup->id;
	multi_send_data(multibuf, 10, 2);
}

void coop_powerup_duplication_receive(const ubyte *buf)
{
	int local_objnum;
	int pnum;
	int powerup_id;
	sbyte owner;
	object *powerup;

	if (!coop_powerup_duplication_active())
		return;
	owner = (sbyte) buf[3];
	pnum = buf[4];
	powerup_id = buf[9];
	if (pnum < 0 || pnum >= MAX_PLAYERS ||
	    Players[pnum].connected == CONNECT_DISCONNECTED)
		return;
	local_objnum = objnum_remote_to_local(GET_INTEL_SHORT(buf + 1), owner);
	if (local_objnum < 0 || local_objnum > Highest_object_index)
		return;
	powerup = &Objects[local_objnum];
	if (powerup->id != powerup_id ||
	    !coop_powerup_duplication_eligible(powerup))
		return;
	coop_powerup_duplication_record(powerup, pnum);
}

void coop_powerup_duplication_send_snapshot(int pnum)
{
	size_t i;

	if (!coop_powerup_duplication_active() || !multi_i_am_master() ||
	    pnum < 0 || pnum >= MAX_PLAYERS)
		return;
	prune_collections();
	multibuf[0] = MULTI_COOP_POWERUP_SNAPSHOT_BEGIN;
	PUT_INTEL_INT(multibuf + 1, (int) collection_count);
	multi_send_data_direct(multibuf, 5, pnum, 2);
	for (i = 0; i < collection_count; i++) {
		const coop_powerup_collection *item = &collections[i];
		sbyte owner;
		short remote_objnum;

		if (item->object_index < 0 ||
		    item->object_index > Highest_object_index)
			return;
		remote_objnum = (short) objnum_local_to_remote(
		    item->object_index, &owner);
		multibuf[0] = MULTI_COOP_POWERUP_SNAPSHOT_ENTRY;
		PUT_INTEL_SHORT(multibuf + 1, remote_objnum);
		multibuf[3] = (ubyte) owner;
		PUT_INTEL_INT(multibuf + 4, item->object_signature);
		multibuf[8] = item->powerup_id;
		memcpy(multibuf + 9, item->client_id, sizeof(item->client_id));
		memcpy(multibuf + 46, item->callsign, sizeof(item->callsign));
		multi_send_data_direct(multibuf, 55, pnum, 2);
	}
	multibuf[0] = MULTI_COOP_POWERUP_SNAPSHOT_END;
	PUT_INTEL_INT(multibuf + 1, (int) collection_count);
	multi_send_data_direct(multibuf, 5, pnum, 2);
}

void coop_powerup_duplication_receive_snapshot_begin(const ubyte *buf)
{
	int count;

	free(snapshot_collections);
	snapshot_collections = NULL;
	snapshot_count = 0;
	snapshot_expected_count = 0;
	snapshot_receiving = 0;
	if (!coop_powerup_duplication_active())
		return;
	count = GET_INTEL_INT(buf + 1);
	if (count < 0 || count > MAX_OBJECTS * MAX_PLAYERS)
		return;
	if (count) {
		snapshot_collections = (coop_powerup_collection *) calloc(
		    (size_t) count, sizeof(*snapshot_collections));
		if (!snapshot_collections)
			return;
	}
	snapshot_expected_count = (size_t) count;
	snapshot_receiving = 1;
}

void coop_powerup_duplication_receive_snapshot_entry(const ubyte *buf)
{
	coop_powerup_collection *item;
	int local_objnum;
	sbyte owner;

	if (!snapshot_receiving ||
	    snapshot_count >= snapshot_expected_count)
		return;
	owner = (sbyte) buf[3];
	local_objnum = objnum_remote_to_local(GET_INTEL_SHORT(buf + 1), owner);
	if (local_objnum < 0 || local_objnum > Highest_object_index) {
		snapshot_receiving = 0;
		return;
	}
	item = &snapshot_collections[snapshot_count];
	item->object_index = (int16_t) local_objnum;
	item->object_signature = Objects[local_objnum].signature;
	item->powerup_id = buf[8];
	memcpy(item->client_id, buf + 9, sizeof(item->client_id));
	memcpy(item->callsign, buf + 46, sizeof(item->callsign));
	item->client_id[COOP_POWERUP_CLIENT_ID_LEN] = '\0';
	item->callsign[COOP_POWERUP_CALLSIGN_LEN] = '\0';
	snapshot_count++;
}

void coop_powerup_duplication_receive_snapshot_end(const ubyte *buf)
{
	int count = GET_INTEL_INT(buf + 1);

	if (snapshot_receiving && count >= 0 &&
	    (size_t) count == snapshot_expected_count &&
	    snapshot_count == snapshot_expected_count &&
	    coop_powerup_duplication_set_pending(snapshot_collections,
	                                         snapshot_count))
		coop_powerup_duplication_apply_pending();
	free(snapshot_collections);
	snapshot_collections = NULL;
	snapshot_count = 0;
	snapshot_expected_count = 0;
	snapshot_receiving = 0;
}
