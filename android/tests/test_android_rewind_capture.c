/* Run the real rewind manager with a checkpoint backend that can reject or
 * asynchronously fail captures, rather than testing a copy of its scheduler */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "android_rewind.h"
#include "state_checkpoint.h"
#include "coop/coop_campaign.h"
#include "game.h"
#include "gameseq.h"
#include "mission.h"
#include "multi.h"
#include "newdemo.h"

#define CHECK(condition)                                            \
	do {                                                            \
		if (!(condition)) {                                         \
			fprintf(stderr, "line %d: %s\n", __LINE__, #condition); \
			exit(1);                                                \
		}                                                           \
	} while (0)

int Game_mode, Player_num, N_players, Player_is_dead, Endlevel_sequence;
int Current_level_num = 10, Newdemo_state;
fix64 GameTime64;
struct netgame_info Netgame;
#ifdef DXX_BUILD_DESCENT_II
player Players[MAX_PLAYERS + 4];
#else
player Players[MAX_PLAYERS];
#endif
Mission *Current_mission;
static coop_campaign campaign;
static int submissions, reject_capture = 1, fail_completion;
static uint64_t completion_token;
static state_checkpoint_callback completion;

int state_checkpoint_initialize(void)
{
	return 1;
}
int state_checkpoint_submit(const char *description, int kind, uint64_t token,
                            state_checkpoint_callback callback)
{
	(void) kind;
	CHECK(!strcmp(description, "REWIND") && !completion);
	++submissions;
	if (reject_capture) return 0;
	completion_token = token;
	completion = callback;
	return 1;
}
void state_checkpoint_poll(void)
{
	if (completion) {
		unsigned char data[] = { 0x5a, 0x19 };
		rewind_memory_buffer buffer = { data, sizeof(data), sizeof(data), 0 };
		state_checkpoint_callback callback = completion;
		completion = NULL;
		callback(completion_token, !fail_completion, &buffer);
	}
}
int coop_briefing_active(void)
{
	return 0;
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
const coop_campaign *coop_campaign_current(void)
{
	return &campaign;
}
int64_t collide_get_collision_delay_last_play_time(void)
{
	return 0;
}
int input_demo_recorder_is_active(void)
{
	return 0;
}
uint32_t input_demo_recorder_frame_count(void)
{
	return 0;
}
size_t input_demo_rng_trace_event_count(void)
{
	return 0;
}
int input_demo_recorder_cancel_for_rewind(void)
{
	return 0;
}
int input_demo_recorder_truncate(uint32_t count)
{
	(void) count;
	return 1;
}
int input_demo_rng_trace_truncate(size_t count)
{
	(void) count;
	return 1;
}
void android_send_overlay_line(const char *text)
{
	(void) text;
}
int HUD_init_message(int flags, const char *format, ...)
{
	(void) flags;
	(void) format;
	return 0;
}
int state_restore_from_memory(const rewind_memory_buffer *buffer)
{
	(void) buffer;
	return 1;
}
int state_restore_coop_from_memory(const rewind_memory_buffer *buffer)
{
	(void) buffer;
	return 1;
}
void multi_send_score(void) {}

static void frames_until(int seconds)
{
	for (; GameTime64 < (fix64) seconds * F1_0; GameTime64 += F1_0 / 128)
		android_rewind_maybe_capture_frame();
}

int main(void)
{
	Mission mission = { 0 };
	int count;
	android_rewind_authoritative_restore restore;
	mission.filename = "descent";
	Current_mission = &mission;
	android_rewind_reset_level();

	/* Persistent rejection before the first snapshot must not retry every frame */
	frames_until(10);
	CHECK(submissions == 2);
	android_rewind_get_history(&count, NULL, NULL);
	CHECK(count == 0);

	/* Once the backend recovers, the next due capture completes normally */
	reject_capture = 0;
	frames_until(15);
	CHECK(submissions == 3);
	android_rewind_get_history(&count, NULL, NULL);
	CHECK(count == 1);

	/* Further failures preserve the previous valid snapshot for restoration */
	reject_capture = 1;
	frames_until(30);
	CHECK(submissions == 6);
	CHECK(android_rewind_select_restore(&restore) == ANDROID_REWIND_STATUS_RESTORED);
	CHECK(restore.game_time64 == 10 * F1_0 && restore.buffer.size == 2);
	CHECK(restore.buffer.data[0] == 0x5a && restore.buffer.data[1] == 0x19);

	/* An asynchronous failure obeys the same deadline and preserves history */
	reject_capture = 0;
	fail_completion = 1;
	frames_until(35);
	CHECK(submissions == 7);
	android_rewind_get_history(&count, NULL, NULL);
	CHECK(count == 1);
	fail_completion = 0;
	frames_until(40);
	CHECK(submissions == 8);
	android_rewind_get_history(&count, NULL, NULL);
	CHECK(count == 2);

	/* A new world can capture immediately without inheriting the old deadline */
	Current_level_num = -1;
	android_rewind_maybe_capture_frame();
	CHECK(submissions == 9);
	state_checkpoint_poll();
	android_rewind_get_history(&count, NULL, NULL);
	CHECK(count == 1);
	puts("PASS: rewind capture rejection, recovery, retained snapshots and world reset");
	return 0;
}
