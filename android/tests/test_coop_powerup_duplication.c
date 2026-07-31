#include <stdio.h>
#include <string.h>
#ifndef _WIN32
#include <strings.h>
#endif

#include "coop_powerup_duplication.h"
#include "game.h"
#include "multi.h"
#include "player.h"
#include "powerup.h"

#define CHECK(condition)                                                     \
	do {                                                                     \
		if (!(condition)) {                                                  \
			fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, \
			        #condition);                                             \
			return 0;                                                        \
		}                                                                    \
	} while (0)

object Objects[MAX_OBJECTS];
int Highest_object_index;
int Game_mode;
struct netgame_info Netgame;
#ifdef DXX_BUILD_DESCENT_II
player Players[MAX_PLAYERS + 4];
#else
player Players[MAX_PLAYERS];
#endif
int Player_num;
ubyte multibuf[MAX_MULTI_MESSAGE_LEN + 4];

int d_stricmp(const char *left, const char *right)
{
#ifdef _WIN32
	return _stricmp(left, right);
#else
	return strcasecmp(left, right);
#endif
}

int objnum_local_to_remote(int local_obj, sbyte *owner)
{
	*owner = -1;
	return local_obj;
}

int objnum_remote_to_local(int remote_obj, int owner)
{
	(void) owner;
	return remote_obj;
}

int multi_i_am_master(void)
{
	return 1;
}

void multi_send_data(unsigned char *buf, int len, int priority)
{
	(void) buf;
	(void) len;
	(void) priority;
}

#ifdef DXX_BUILD_DESCENT_II
void multi_send_data_direct(unsigned char *buf, int len, int pnum, int priority)
#else
void multi_send_data_direct(const ubyte *buf, int len, int pnum, int priority)
#endif
{
	(void) buf;
	(void) len;
	(void) pnum;
	(void) priority;
}

static void reset_test_state(void)
{
	coop_powerup_duplication_reset();
	memset(Objects, 0, sizeof(Objects));
	memset(&Netgame, 0, sizeof(Netgame));
	memset(Players, 0, sizeof(Players));
	Game_mode = GM_MULTI_COOP;
	Netgame.DuplicateEnergyShields = 1;
	Highest_object_index = 8;
}

static void set_powerup(int objnum, int signature, int id)
{
	Objects[objnum].signature = signature;
	Objects[objnum].type = OBJ_POWERUP;
	Objects[objnum].id = (ubyte) id;
	Objects[objnum].control_type = CT_POWERUP;
}

static int test_restore_remaps_and_prunes_stale_records(void)
{
	coop_powerup_collection pending[3] = { 0 };
	const coop_powerup_collection *restored;

	reset_test_state();
	set_powerup(3, 100, POW_ENERGY);
	memcpy(pending[0].callsign, "touch", sizeof("touch"));
	pending[0].object_index = 7;
	pending[0].object_signature = 100;
	pending[0].powerup_id = POW_ENERGY;
	memcpy(pending[1].callsign, "touch", sizeof("touch"));
	pending[1].object_index = 4;
	pending[1].object_signature = 200;
	pending[1].powerup_id = POW_SHIELD_BOOST;
	pending[2] = pending[0];

	CHECK(coop_powerup_duplication_set_pending(pending, 3));
	CHECK(coop_powerup_duplication_apply_pending());
	CHECK(coop_powerup_duplication_count() == 1);
	restored = coop_powerup_duplication_data();
	CHECK(restored != NULL);
	CHECK(restored[0].object_index == 3);
	CHECK(restored[0].object_signature == 100);
	return 1;
}

static int test_restore_rejects_invalid_identity(void)
{
	coop_powerup_collection pending = { 0 };

	reset_test_state();
	set_powerup(2, 50, POW_ENERGY);
	pending.object_index = 2;
	pending.object_signature = 50;
	pending.powerup_id = POW_ENERGY;
	memset(pending.callsign, 'x', sizeof(pending.callsign));

	CHECK(coop_powerup_duplication_set_pending(&pending, 1));
	CHECK(!coop_powerup_duplication_apply_pending());
	return 1;
}

int main(void)
{
	if (!test_restore_remaps_and_prunes_stale_records() ||
	    !test_restore_rejects_invalid_identity())
		return 1;
	coop_powerup_duplication_reset();
	puts("coop powerup duplication tests passed");
	return 0;
}
