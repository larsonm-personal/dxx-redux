/* Exercise the real level-start capture loop with a failing serializer */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "coop_level_restart.h"
#include "coop_campaign.h"
#include "game.h"
#include "gameseq.h"
#include "mission.h"
#include "multi.h"

#define CHECK(c)                                            \
	do {                                                    \
		if (!(c)) {                                         \
			fprintf(stderr, "line %d: %s\n", __LINE__, #c); \
			exit(1);                                        \
		}                                                   \
	} while (0)

int Game_mode = GM_MULTI_COOP, Player_num, N_players = 1;
int Current_level_num = -1;
char Current_level_name[LEVEL_NAME_LEN];
Mission *Current_mission;
struct netgame_info Netgame;
#ifdef DXX_BUILD_DESCENT_II
player Players[MAX_PLAYERS + 4];
#else
player Players[MAX_PLAYERS];
#endif
static coop_campaign campaign;
static fix64 now;
static int attempts, fail_capture = 1, barrier;

fix64 timer_query(void)
{
	return now;
}
int coop_briefing_active(void)
{
	return barrier;
}
int coop_travel_blocks_state_actions(void)
{
	return 0;
}
int multi_save_transfer_busy(void)
{
	return 0;
}
int multi_i_am_master(void)
{
	return 1;
}
int multi_all_players_alive(void)
{
	return 1;
}
bool is_observer(void)
{
	return 0;
}
const coop_campaign *coop_campaign_current(void)
{
	return &campaign;
}
int state_save_to_memory(rewind_memory_buffer *buffer, const char *desc, int kind, int blank)
{
	(void) kind;
	CHECK(!strcmp(desc, "LEVEL START") && blank);
	++attempts;
	buffer->data = (unsigned char *) malloc(1);
	CHECK(buffer->data);
	buffer->size = buffer->capacity = 1;
	buffer->data[0] = (unsigned char) attempts;
	return !fail_capture;
}
int state_restore_coop_from_memory(const rewind_memory_buffer *buffer)
{
	CHECK(buffer->size == 1 && buffer->data[0] == 3);
	return 1;
}
int multi_send_level_restart_transfer(const rewind_memory_buffer *buffer)
{
	CHECK(buffer && buffer->size == 1);
	return 1;
}
void multi_send_score(void) {}
void coop_restore_status_complete(void) {}
void coop_restore_status_failed(void) {}
int state_android_build_coop_sidecar_filename(char *path, size_t size, const char *name)
{
	(void) path;
	(void) size;
	(void) name;
	return 0;
}
void state_android_ensure_parent_dirs_for_path(const char *path)
{
	(void) path;
}
void android_save_set_sanitize_component(char *dst, size_t size, const char *src, const char *fallback)
{
	(void) fallback;
	snprintf(dst, size, "%s", src);
}
int PHYSFSX_rename(const char *a, const char *b)
{
	(void) a;
	(void) b;
	return 0;
}
int PHYSFSX_exists(const char *path, int ignorecase)
{
	(void) path;
	(void) ignorecase;
	return 0;
}

static void frames_until(int seconds)
{
	for (; now < (fix64) seconds * F1_0; now += F1_0 / 128)
		coop_level_restart_maybe_capture_ready();
}

int main(void)
{
	Mission mission = { 0 };
	mission.filename = "descent";
	Current_mission = &mission;
	Players[0].connected = CONNECT_PLAYING;
	strcpy(Players[0].callsign, "host");
	coop_level_restart_note_natural_level(Current_level_num);
	frames_until(10);
	CHECK(attempts == 2 && !coop_level_restart_buffer());
	CHECK(coop_level_restart_get_state() == COOP_LEVEL_RESTART_CAPTURING);
	fail_capture = 0;
	frames_until(15);
	CHECK(attempts == 3 && coop_level_restart_get_state() == COOP_LEVEL_RESTART_READY);
	CHECK(coop_level_restart_apply_host());

	/* A new mine resets the deadline but preserves old bytes on capture failure */
	Current_level_num = -2;
	fail_capture = 1;
	coop_level_restart_note_natural_level(Current_level_num);
	coop_level_restart_maybe_capture_ready();
	CHECK(attempts == 4 && coop_level_restart_buffer()->data[0] == 3);
	barrier = 1;
	frames_until(25);
	CHECK(attempts == 4);
	barrier = 0;
	fail_capture = 0;
	coop_level_restart_maybe_capture_ready();
	CHECK(attempts == 5 && coop_level_restart_buffer()->data[0] == 5);

	coop_level_restart_note_restore_begin();
	frames_until(35);
	CHECK(attempts == 5);
	coop_level_restart_clear();
	CHECK(!coop_level_restart_buffer());
	puts("PASS: level-start rejected captures are bounded, recover and retain previous bytes");
	return 0;
}
