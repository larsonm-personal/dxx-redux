/*
THE COMPUTER CODE CONTAINED HEREIN IS THE SOLE PROPERTY OF PARALLAX
SOFTWARE CORPORATION ("PARALLAX").  PARALLAX, IN DISTRIBUTING THE CODE TO
END-USERS, AND SUBJECT TO ALL OF THE TERMS AND CONDITIONS HEREIN, GRANTS A
ROYALTY-FREE, PERPETUAL LICENSE TO SUCH END-USERS FOR USE BY SUCH END-USERS
IN USING, DISPLAYING,  AND CREATING DERIVATIVE WORKS THEREOF, SO LONG AS
SUCH USE, DISPLAY OR CREATION IS FOR NON-COMMERCIAL, ROYALTY OR REVENUE
FREE PURPOSES.  IN NO EVENT SHALL THE END-USER USE THE COMPUTER CODE
CONTAINED HEREIN FOR REVENUE-BEARING PURPOSES.  THE END-USER UNDERSTANDS
AND AGREES TO THE TERMS HEREIN AND ACCEPTS THE SAME BY USE OF THIS FILE.
COPYRIGHT 1993-1999 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/

/*
 *
 * Escort robot behavior.
 *
 */

#include <stdio.h>		// for printf()
#include <stdlib.h>		// for rand() and qsort()
#include <string.h>		// for memset()

#include "window.h"
#include "inferno.h"
#include "console.h"
#include "fix.h"
#include "vecmat.h"
#include "gr.h"
#include "3d.h"
#include "palette.h"
#include "timer.h"

#include "object.h"
#include "dxxerror.h"
#include "ai.h"
#include "robot.h"
#include "fvi.h"
#include "physics.h"
#include "wall.h"
#include "player.h"
#include "fireball.h"
#include "game.h"
#include "powerup.h"
#include "cntrlcen.h"
#include "gauges.h"
#include "key.h"
#include "fuelcen.h"
#include "sounds.h"
#include "screens.h"
#include "text.h"
#include "gamefont.h"
#include "newmenu.h"
#include "playsave.h"
#include "gameseq.h"
#include "automap.h"
#include "laser.h"
#include "escort.h"
#include "collide.h"
#include "maths.h"
#include "input_demo_debug_logging.h"
#include "input_demo_recorder.h"
#include "input_demo_replay.h"

#ifdef NETWORK
#include "multi.h"
#endif

#ifdef __ANDROID__
#include <android/log.h>
#define ESCORT_DIAG(fmt, ...) __android_log_print(ANDROID_LOG_INFO, "DXX-ESCORT", fmt, ##__VA_ARGS__)
#else
#define ESCORT_DIAG(fmt, ...) ((void)0)
#endif

#ifdef EDITOR
#include "editor/editor.h"
#endif

extern void multi_send_stolen_items();
void say_escort_goal(int goal_num);
void show_escort_menu(char *msg);
extern fix64 Buddy_last_seen_player, Buddy_last_player_path_created;

static int input_demo_trace_escort_active(void)
{
	return input_demo_debug_is_enabled() &&
		(input_demo_recorder_is_active() || input_demo_replay_is_loaded());
}

static unsigned int input_demo_trace_frame_index(void)
{
	if (input_demo_replay_is_loaded())
		return (unsigned int)input_demo_replay_next_frame_index();
	if (input_demo_recorder_is_active()) {
		const uint32_t frame_count = input_demo_recorder_frame_count();

		return frame_count ? (unsigned int)(frame_count - 1) : 0;
	}
	return 0;
}

typedef struct input_demo_trace_key_snapshot {
	int valid;
	unsigned int state_key;
} input_demo_trace_key_snapshot;

static unsigned int input_demo_trace_hash_label(const char *label)
{
	unsigned int key = 0;

	if (!label)
		return 0;

	while (*label) {
		key = key * 131u + (unsigned int)(unsigned char)(*label);
		label++;
	}

	return key;
}

static int input_demo_trace_fix_bucket(fix value)
{
	if (value < 0)
		return -1;

	return f2i(value);
}

static int input_demo_trace_key_snapshot_should_log(input_demo_trace_key_snapshot *snapshot, unsigned int state_key)
{
	if (!snapshot)
		return 1;

	if (snapshot->valid && snapshot->state_key == state_key)
		return 0;

	snapshot->valid = 1;
	snapshot->state_key = state_key;
	return 1;
}

static void input_demo_log_escort_rng_progress(const char *label, unsigned int *rng_before, unsigned int *rng_call_count_before)
{
	unsigned int rng_after;
	unsigned int rng_call_count_after;

	if (!d_rand_get_state(&rng_after))
		return;
	rng_call_count_after = d_rand_get_call_count();
	if (rng_after == *rng_before && rng_call_count_after == *rng_call_count_before)
		return;
	con_printf(CON_NORMAL,
		"Input demo replay escort rng progress: frame=%u step=%s calls=%u->%u before=%u after=%u\n",
		input_demo_trace_frame_index(),
		label,
		*rng_call_count_before,
		rng_call_count_after,
		*rng_before,
		rng_after);
	*rng_before = rng_after;
	*rng_call_count_before = rng_call_count_after;
}

static void input_demo_log_escort_path_state(const char *label, object *objp)
{
	char segs[512];
	int objnum;
	ai_static *aip;
	ai_local *ailp;
	int limit;
	int offset;
	int i;
	int written;

	if (!input_demo_trace_escort_active() || !objp)
		return;

	objnum = objp - Objects;
	aip = &objp->ctype.ai_info;
	ailp = &Ai_local_info[objnum];
	if ((aip->hide_index < 0) || (aip->path_length <= 0)) {
		strncpy(segs, "<none>", sizeof(segs));
		segs[sizeof(segs) - 1] = 0;
	} else {
		limit = aip->path_length < 24 ? aip->path_length : 24;
		offset = 0;
		segs[0] = 0;
		for (i=0; i<limit; i++) {
			written = snprintf(segs + offset, sizeof(segs) - offset, "%s%d", i ? "," : "", Point_segs[aip->hide_index + i].segnum);
			if (written < 0)
				break;
			if (written >= (int)(sizeof(segs) - offset)) {
				offset = sizeof(segs) - 1;
				break;
			}
			offset += written;
		}
		if ((limit < aip->path_length) && (offset < (int)sizeof(segs)))
			snprintf(segs + offset, sizeof(segs) - offset, ",...");
	}

	con_printf(CON_NORMAL,
		"Input demo replay escort path state: frame=%u step=%s obj=%d seg=%d mode=%d behavior=%d goal=%d special=%d goal_seg=%d cur_path=%d/%d hide=%d segs=%s\n",
		input_demo_trace_frame_index(),
		label,
		objnum,
		objp->segnum,
		ailp->mode,
		aip->behavior,
		Escort_goal_object,
		Escort_special_goal,
		ailp->goal_segment,
		aip->cur_path_index,
		aip->path_length,
		aip->hide_index,
		segs);
}

typedef struct input_demo_escort_visit_snapshot {
	int valid;
	int mode;
	int buddy_seg;
	int player_seg;
	int believed_seg;
	int cur_path_index;
	int path_length;
	fix64 buddy_last_seen_player;
	fix64 buddy_last_player_path_created;
	fix64 escort_last_path_created;
	int away_gate;
	int recent_path_gate;
	int goto_player_gate;
	int same_seg_gate;
	int early_path_gate;
	int visit;
} input_demo_escort_visit_snapshot;

typedef struct input_demo_escort_state_snapshot {
	int valid;
	int player_visibility;
	int mode;
	int buddy_allowed_to_talk;
	int buddy_seg;
	int player_seg;
	int cur_path_index;
	int path_length;
	int hide_index;
	fix64 buddy_last_seen_player;
	fix64 buddy_last_player_path_created;
	fix64 escort_last_path_created;
	fix64 time_player_seen;
	int escort_goal_object;
	int escort_special_goal;
	int should_visit_player;
	int away_gate;
	int recent_path_gate;
	int goto_player_gate;
	int same_seg_gate;
	int early_path_gate;
} input_demo_escort_state_snapshot;

static input_demo_escort_visit_snapshot g_input_demo_escort_visit_snapshot;
static input_demo_escort_state_snapshot g_input_demo_escort_state_snapshot;
static input_demo_trace_key_snapshot g_input_demo_snipe_entry_snapshot;
static input_demo_trace_key_snapshot g_input_demo_snipe_exit_snapshot;
static input_demo_trace_key_snapshot g_input_demo_thief_entry_snapshot;
static input_demo_trace_key_snapshot g_input_demo_thief_exit_snapshot;
static int g_input_demo_escort_segment_snapshot_valid = 0;
static int g_input_demo_escort_segment_snapshot_player_seg = -1;
static int g_input_demo_escort_segment_snapshot_believed_seg = -1;

static void input_demo_reset_escort_state_probes(void)
{
	memset(&g_input_demo_escort_visit_snapshot, 0, sizeof(g_input_demo_escort_visit_snapshot));
	memset(&g_input_demo_escort_state_snapshot, 0, sizeof(g_input_demo_escort_state_snapshot));
	memset(&g_input_demo_snipe_entry_snapshot, 0, sizeof(g_input_demo_snipe_entry_snapshot));
	memset(&g_input_demo_snipe_exit_snapshot, 0, sizeof(g_input_demo_snipe_exit_snapshot));
	memset(&g_input_demo_thief_entry_snapshot, 0, sizeof(g_input_demo_thief_entry_snapshot));
	memset(&g_input_demo_thief_exit_snapshot, 0, sizeof(g_input_demo_thief_exit_snapshot));
	g_input_demo_escort_segment_snapshot_valid = 0;
	g_input_demo_escort_segment_snapshot_player_seg = -1;
	g_input_demo_escort_segment_snapshot_believed_seg = -1;
}

static void input_demo_log_escort_restore_normalization(object *objp, ai_local *ailp,
	fix64 raw_time_player_seen, fix64 raw_escort_last_path_created)
{
	ai_static *aip;

	if (!input_demo_trace_escort_active() || !objp || !ailp)
		return;

	aip = &objp->ctype.ai_info;
	con_printf(CON_NORMAL,
		"Input demo replay escort restore normalize: gt=%lld obj=%d seg=%d mode=%d prev_vis=%d raw_seen=%lld raw_escort_last_path=%lld final_seen=%lld final_last_player_path=%lld final_escort_last_path=%lld cur_path=%d/%d hide_index=%d\n",
		(long long)GameTime64,
		(int)(objp - Objects),
		objp->segnum,
		ailp->mode,
		ailp->previous_visibility,
		(long long)raw_time_player_seen,
		(long long)raw_escort_last_path_created,
		(long long)Buddy_last_seen_player,
		(long long)Buddy_last_player_path_created,
		(long long)Escort_last_path_created,
		aip->cur_path_index,
		aip->path_length,
		aip->hide_index);
}

static void input_demo_log_escort_segment_change(object *objp, ai_local *ailp, ai_static *aip,
	int player_seg, int believed_seg)
{
	if (!input_demo_trace_escort_active() || !objp || !ailp || !aip)
		return;

	if (!g_input_demo_escort_segment_snapshot_valid) {
		g_input_demo_escort_segment_snapshot_valid = 1;
		g_input_demo_escort_segment_snapshot_player_seg = player_seg;
		g_input_demo_escort_segment_snapshot_believed_seg = believed_seg;
		return;
	}

	if ((player_seg == g_input_demo_escort_segment_snapshot_player_seg) &&
		(believed_seg == g_input_demo_escort_segment_snapshot_believed_seg))
		return;

	con_printf(CON_NORMAL,
		"Input demo replay escort segment change: gt=%lld player_seg=%d->%d believed_seg=%d->%d buddy_seg=%d mode=%d cur_path=%d/%d prev_vis=%d last_seen=%lld last_player_path=%lld\n",
		(long long)GameTime64,
		g_input_demo_escort_segment_snapshot_player_seg,
		player_seg,
		g_input_demo_escort_segment_snapshot_believed_seg,
		believed_seg,
		objp->segnum,
		ailp->mode,
		aip->cur_path_index,
		aip->path_length,
		ailp->previous_visibility,
		(long long)Buddy_last_seen_player,
		(long long)Buddy_last_player_path_created);

	g_input_demo_escort_segment_snapshot_player_seg = player_seg;
	g_input_demo_escort_segment_snapshot_believed_seg = believed_seg;
}

static void input_demo_log_escort_visit_change(object *objp, ai_local *ailp, ai_static *aip,
	int player_seg, int believed_seg, int away_gate, int recent_path_gate,
	int goto_player_gate, int same_seg_gate, int early_path_gate, int visit)
{
	input_demo_escort_visit_snapshot current;

	if (!input_demo_trace_escort_active() || !objp || !ailp || !aip)
		return;

	current.valid = 1;
	current.mode = ailp->mode;
	current.buddy_seg = objp->segnum;
	current.player_seg = player_seg;
	current.believed_seg = believed_seg;
	current.cur_path_index = aip->cur_path_index;
	current.path_length = aip->path_length;
	current.buddy_last_seen_player = Buddy_last_seen_player;
	current.buddy_last_player_path_created = Buddy_last_player_path_created;
	current.escort_last_path_created = Escort_last_path_created;
	current.away_gate = away_gate;
	current.recent_path_gate = recent_path_gate;
	current.goto_player_gate = goto_player_gate;
	current.same_seg_gate = same_seg_gate;
	current.early_path_gate = early_path_gate;
	current.visit = visit;

	if (!g_input_demo_escort_visit_snapshot.valid) {
		g_input_demo_escort_visit_snapshot = current;
		return;
	}

	if ((current.mode == g_input_demo_escort_visit_snapshot.mode) &&
		(current.buddy_seg == g_input_demo_escort_visit_snapshot.buddy_seg) &&
		(current.player_seg == g_input_demo_escort_visit_snapshot.player_seg) &&
		(current.believed_seg == g_input_demo_escort_visit_snapshot.believed_seg) &&
		(current.cur_path_index == g_input_demo_escort_visit_snapshot.cur_path_index) &&
		(current.path_length == g_input_demo_escort_visit_snapshot.path_length) &&
		(current.buddy_last_player_path_created == g_input_demo_escort_visit_snapshot.buddy_last_player_path_created) &&
		(current.escort_last_path_created == g_input_demo_escort_visit_snapshot.escort_last_path_created) &&
		(current.away_gate == g_input_demo_escort_visit_snapshot.away_gate) &&
		(current.recent_path_gate == g_input_demo_escort_visit_snapshot.recent_path_gate) &&
		(current.goto_player_gate == g_input_demo_escort_visit_snapshot.goto_player_gate) &&
		(current.same_seg_gate == g_input_demo_escort_visit_snapshot.same_seg_gate) &&
		(current.early_path_gate == g_input_demo_escort_visit_snapshot.early_path_gate) &&
		(current.visit == g_input_demo_escort_visit_snapshot.visit))
		return;

	con_printf(CON_NORMAL,
		"Input demo replay escort visit change: gt=%lld mode=%d->%d buddy_seg=%d->%d player_seg=%d->%d believed_seg=%d->%d cur_path=%d/%d->%d/%d last_seen=%lld->%lld last_player_path=%lld->%lld escort_last_path=%lld->%lld away=%d->%d recent=%d->%d goto=%d->%d same=%d->%d early=%d->%d visit=%d->%d\n",
		(long long)GameTime64,
		g_input_demo_escort_visit_snapshot.mode,
		current.mode,
		g_input_demo_escort_visit_snapshot.buddy_seg,
		current.buddy_seg,
		g_input_demo_escort_visit_snapshot.player_seg,
		current.player_seg,
		g_input_demo_escort_visit_snapshot.believed_seg,
		current.believed_seg,
		g_input_demo_escort_visit_snapshot.cur_path_index,
		g_input_demo_escort_visit_snapshot.path_length,
		current.cur_path_index,
		current.path_length,
		(long long)g_input_demo_escort_visit_snapshot.buddy_last_seen_player,
		(long long)current.buddy_last_seen_player,
		(long long)g_input_demo_escort_visit_snapshot.buddy_last_player_path_created,
		(long long)current.buddy_last_player_path_created,
		(long long)g_input_demo_escort_visit_snapshot.escort_last_path_created,
		(long long)current.escort_last_path_created,
		g_input_demo_escort_visit_snapshot.away_gate,
		current.away_gate,
		g_input_demo_escort_visit_snapshot.recent_path_gate,
		current.recent_path_gate,
		g_input_demo_escort_visit_snapshot.goto_player_gate,
		current.goto_player_gate,
		g_input_demo_escort_visit_snapshot.same_seg_gate,
		current.same_seg_gate,
		g_input_demo_escort_visit_snapshot.early_path_gate,
		current.early_path_gate,
		g_input_demo_escort_visit_snapshot.visit,
		current.visit);

	g_input_demo_escort_visit_snapshot = current;
}

static void input_demo_log_escort_state(object *objp, ai_local *ailp, ai_static *aip,
	fix dist_to_player, int player_visibility, int should_visit_player,
	fix64 since_seen, fix64 since_player_path, int away_gate,
	int recent_path_gate, int goto_player_gate, int same_seg_gate,
	int early_path_gate)
{
	input_demo_escort_state_snapshot current;

	if (!input_demo_trace_escort_active() || !objp || !ailp || !aip)
		return;

	current.valid = 1;
	current.player_visibility = player_visibility;
	current.mode = ailp->mode;
	current.buddy_allowed_to_talk = Buddy_allowed_to_talk;
	current.buddy_seg = objp->segnum;
	current.player_seg = ConsoleObject ? ConsoleObject->segnum : -1;
	current.cur_path_index = aip->cur_path_index;
	current.path_length = aip->path_length;
	current.hide_index = aip->hide_index;
	current.buddy_last_seen_player = Buddy_last_seen_player;
	current.buddy_last_player_path_created = Buddy_last_player_path_created;
	current.escort_last_path_created = Escort_last_path_created;
	current.time_player_seen = ailp->time_player_seen;
	current.escort_goal_object = Escort_goal_object;
	current.escort_special_goal = Escort_special_goal;
	current.should_visit_player = should_visit_player;
	current.away_gate = away_gate;
	current.recent_path_gate = recent_path_gate;
	current.goto_player_gate = goto_player_gate;
	current.same_seg_gate = same_seg_gate;
	current.early_path_gate = early_path_gate;

	if (g_input_demo_escort_state_snapshot.valid &&
		(current.player_visibility == g_input_demo_escort_state_snapshot.player_visibility) &&
		(current.mode == g_input_demo_escort_state_snapshot.mode) &&
		(current.buddy_allowed_to_talk == g_input_demo_escort_state_snapshot.buddy_allowed_to_talk) &&
		(current.buddy_seg == g_input_demo_escort_state_snapshot.buddy_seg) &&
		(current.player_seg == g_input_demo_escort_state_snapshot.player_seg) &&
		(current.cur_path_index == g_input_demo_escort_state_snapshot.cur_path_index) &&
		(current.path_length == g_input_demo_escort_state_snapshot.path_length) &&
		(current.hide_index == g_input_demo_escort_state_snapshot.hide_index) &&
		(current.buddy_last_seen_player == g_input_demo_escort_state_snapshot.buddy_last_seen_player) &&
		(current.buddy_last_player_path_created == g_input_demo_escort_state_snapshot.buddy_last_player_path_created) &&
		(current.escort_last_path_created == g_input_demo_escort_state_snapshot.escort_last_path_created) &&
		(current.time_player_seen == g_input_demo_escort_state_snapshot.time_player_seen) &&
		(current.escort_goal_object == g_input_demo_escort_state_snapshot.escort_goal_object) &&
		(current.escort_special_goal == g_input_demo_escort_state_snapshot.escort_special_goal) &&
		(current.should_visit_player == g_input_demo_escort_state_snapshot.should_visit_player) &&
		(current.away_gate == g_input_demo_escort_state_snapshot.away_gate) &&
		(current.recent_path_gate == g_input_demo_escort_state_snapshot.recent_path_gate) &&
		(current.goto_player_gate == g_input_demo_escort_state_snapshot.goto_player_gate) &&
		(current.same_seg_gate == g_input_demo_escort_state_snapshot.same_seg_gate) &&
		(current.early_path_gate == g_input_demo_escort_state_snapshot.early_path_gate))
		return;

	con_printf(CON_NORMAL,
		"Input demo replay escort state: frame=%u gt=%lld vis=%d mode=%d talk=%d dist=%d buddy_seg=%d player_seg=%d cur_path=%d/%d hide_index=%d last_seen=%lld last_player_path=%lld escort_last_path=%lld seen=%lld goal=%d special=%d visit=%d since_seen=%lld since_player_path=%lld away_gate=%d recent_path_gate=%d goto_player_gate=%d same_seg_gate=%d early_path_gate=%d\n",
			input_demo_trace_frame_index(),
			(long long)GameTime64,
			player_visibility,
			ailp->mode,
			Buddy_allowed_to_talk,
			dist_to_player,
			objp->segnum,
			ConsoleObject ? ConsoleObject->segnum : -1,
			aip->cur_path_index,
			aip->path_length,
			aip->hide_index,
			(long long)Buddy_last_seen_player,
			(long long)Buddy_last_player_path_created,
			(long long)Escort_last_path_created,
			(long long)ailp->time_player_seen,
			Escort_goal_object,
			Escort_special_goal,
			should_visit_player,
			(long long)since_seen,
			(long long)since_player_path,
			away_gate,
			recent_path_gate,
			goto_player_gate,
			same_seg_gate,
			early_path_gate);

	g_input_demo_escort_state_snapshot = current;
}

static void input_demo_log_snipe_detail_probe(input_demo_trace_key_snapshot *snapshot,
	const char *step, object *objp, ai_local *ailp,
	int player_visibility, fix dist_to_player)
{
	ai_static *aip;
	unsigned int state_key;

	if (!input_demo_trace_escort_active() || !snapshot || !objp || !ailp)
		return;

	aip = &objp->ctype.ai_info;
	state_key = input_demo_trace_hash_label(step);
	state_key = state_key * 131u + (unsigned int)ailp->mode;
	state_key = state_key * 131u + (unsigned int)input_demo_trace_fix_bucket(ailp->next_action_time);
	state_key = state_key * 131u + (unsigned int)player_visibility;
	state_key = state_key * 131u + (unsigned int)aip->cur_path_index;
	state_key = state_key * 131u + (unsigned int)aip->path_length;
	state_key = state_key * 131u + (unsigned int)aip->hide_index;
	state_key = state_key * 131u + (unsigned int)objp->segnum;

	if (!input_demo_trace_key_snapshot_should_log(snapshot, state_key))
		return;

	con_printf(CON_NORMAL,
		"Input demo replay snipe detail: frame=%u obj=%d step=%s mode=%d next_action=%d vis=%d dist=%d path=%d/%d hide=%d seg=%d pos=(%d,%d,%d)\n",
		input_demo_trace_frame_index(),
		(int)(objp - Objects),
		step,
		ailp->mode,
		ailp->next_action_time,
		player_visibility,
		dist_to_player,
		aip->cur_path_index,
		aip->path_length,
		aip->hide_index,
		objp->segnum,
		objp->pos.x,
		objp->pos.y,
		objp->pos.z);
}

static void input_demo_log_thief_detail_probe(input_demo_trace_key_snapshot *snapshot,
	const char *step, object *objp, ai_local *ailp,
	int player_visibility, fix dist_to_player)
{
	ai_static *aip;
	unsigned int state_key;

	if (!input_demo_trace_escort_active() || !snapshot || !objp || !ailp)
		return;

	aip = &objp->ctype.ai_info;
	state_key = input_demo_trace_hash_label(step);
	state_key = state_key * 131u + (unsigned int)ailp->mode;
	state_key = state_key * 131u + (unsigned int)input_demo_trace_fix_bucket(ailp->next_action_time);
	state_key = state_key * 131u + (unsigned int)player_visibility;
	state_key = state_key * 131u + (unsigned int)ailp->player_awareness_type;
	state_key = state_key * 131u + (unsigned int)aip->cur_path_index;
	state_key = state_key * 131u + (unsigned int)aip->path_length;
	state_key = state_key * 131u + (unsigned int)aip->hide_index;
	state_key = state_key * 131u + (unsigned int)objp->segnum;

	if (!input_demo_trace_key_snapshot_should_log(snapshot, state_key))
		return;

	con_printf(CON_NORMAL,
		"Input demo replay thief detail: frame=%u obj=%d step=%s mode=%d next_action=%d vis=%d aware=%d dist=%d path=%d/%d hide=%d seg=%d pos=(%d,%d,%d)\n",
		input_demo_trace_frame_index(),
		(int)(objp - Objects),
		step,
		ailp->mode,
		ailp->next_action_time,
		player_visibility,
		ailp->player_awareness_type,
		dist_to_player,
		aip->cur_path_index,
		aip->path_length,
		aip->hide_index,
		objp->segnum,
		objp->pos.x,
		objp->pos.y,
		objp->pos.z);
}


static const char *const Escort_goal_text[MAX_ESCORT_GOALS] = {
	"BLUE KEY",
	"YELLOW KEY",
	"RED KEY",
	"REACTOR",
	"EXIT",
	"ENERGY",
	"ENERGYCEN",
	"SHIELD",
	"POWERUP",
	"ROBOT",
	"HOSTAGES",
	"SPEW",
	"SCRAM",
	"EXIT",
	"BOSS",
	"MARKER 1",
	"MARKER 2",
	"MARKER 3",
	"MARKER 4",
	"MARKER 5",
	"MARKER 6",
	"MARKER 7",
	"MARKER 8",
	"MARKER 9",
// -- too much work -- 	"KAMIKAZE  "
};

int	Max_escort_length = 200;
int	Escort_kill_object = -1;
ubyte Stolen_items[MAX_STOLEN_ITEMS];
int	Stolen_item_index;
fix64	Escort_last_path_created = 0;
int	Escort_goal_object = ESCORT_GOAL_UNSPECIFIED, Escort_special_goal = -1, Escort_goal_index = -1, Buddy_messages_suppressed = 0;
fix64	Buddy_sorry_time;
int	Buddy_objnum, Buddy_allowed_to_talk;
int	Looking_for_marker;
int	Last_buddy_key;
#ifdef NETWORK
int	Escort_owner_player = -1;
#endif

fix64	Last_buddy_message_time;

void init_buddy_for_level(void)
{
	int	i;

	input_demo_reset_escort_state_probes();

	Buddy_allowed_to_talk = 0;
	Buddy_objnum = -1;
	Escort_goal_object = ESCORT_GOAL_UNSPECIFIED;
	Escort_special_goal = -1;
	Escort_goal_index = -1;
	Buddy_messages_suppressed = 0;
#ifdef NETWORK
	Escort_owner_player = -1;
#endif

	for (i=0; i<=Highest_object_index; i++)
		if (Robot_info[Objects[i].id].companion)
			break;
	if (i <= Highest_object_index)
		Buddy_objnum = i;

	Buddy_sorry_time = -F1_0;

	Looking_for_marker = -1;
	Last_buddy_key = -1;
}

//	-----------------------------------------------------------------------------
//	See if segment from curseg through sidenum is reachable.
//	Return true if it is reachable, else return false.
int segment_is_reachable(int curseg, int sidenum)
{
	int		wall_num, rval;
	segment	*segp = &Segments[curseg];

	if (!IS_CHILD(segp->children[sidenum]))
		return 0;

	wall_num = segp->sides[sidenum].wall_num;

	//	If no wall, then it is reachable
	if (wall_num == -1)
		return 1;

	rval = ai_door_is_openable(NULL, segp, sidenum);

	return rval;

// -- MK, 10/17/95 -- 
// -- MK, 10/17/95 -- 	//	Hmm, a closed wall.  I think this mean not reachable.
// -- MK, 10/17/95 -- 	if (Walls[wall_num].type == WALL_CLOSED)
// -- MK, 10/17/95 -- 		return 0;
// -- MK, 10/17/95 -- 
// -- MK, 10/17/95 -- 	if (Walls[wall_num].type == WALL_DOOR) {
// -- MK, 10/17/95 -- 		if (Walls[wall_num].keys == KEY_NONE) {
// -- MK, 10/17/95 -- 			return 1;		//	@MK, 10/17/95: Be consistent with ai_door_is_openable
// -- MK, 10/17/95 -- // -- 			if (Walls[wall_num].flags & WALL_DOOR_LOCKED)
// -- MK, 10/17/95 -- // -- 				return 0;
// -- MK, 10/17/95 -- // -- 			else
// -- MK, 10/17/95 -- // -- 				return 1;
// -- MK, 10/17/95 -- 		} else if (Walls[wall_num].keys == KEY_BLUE)
// -- MK, 10/17/95 -- 			return (Players[Player_num].flags & PLAYER_FLAGS_BLUE_KEY);
// -- MK, 10/17/95 -- 		else if (Walls[wall_num].keys == KEY_GOLD)
// -- MK, 10/17/95 -- 			return (Players[Player_num].flags & PLAYER_FLAGS_GOLD_KEY);
// -- MK, 10/17/95 -- 		else if (Walls[wall_num].keys == KEY_RED)
// -- MK, 10/17/95 -- 			return (Players[Player_num].flags & PLAYER_FLAGS_RED_KEY);
// -- MK, 10/17/95 -- 		else
// -- MK, 10/17/95 -- 			Int3();	//	Impossible!  Doesn't have no key, but doesn't have any key!
// -- MK, 10/17/95 -- 	} else
// -- MK, 10/17/95 -- 		return 1;
// -- MK, 10/17/95 -- 
// -- MK, 10/17/95 -- 	Int3();	//	Hmm, thought 'if' above had to return!
// -- MK, 10/17/95 -- 	return 0;

}


//	-----------------------------------------------------------------------------
//	Create a breadth-first list of segments reachable from current segment.
//	max_segs is maximum number of segments to search.  Use MAX_SEGMENTS to search all.
//	On exit, *length <= max_segs.
//	Input:
//		start_seg
//	Output:
//		bfs_list:	array of shorts, each reachable segment.  Includes start segment.
//		length:		number of elements in bfs_list
void create_bfs_list(int start_seg, short bfs_list[], int *length, int max_segs)
{
	int	head, tail;
	sbyte   visited[MAX_SEGMENTS];

	for (unsigned s=0; s<sizeof(visited)/sizeof(visited[0]); s++)
		visited[s] = 0;

	head = 0;
	tail = 0;

	bfs_list[head++] = start_seg;
	visited[start_seg] = 1;

	while ((head != tail) && (head < max_segs)) {
		int		i;
		int		curseg;
		segment	*cursegp;

		curseg = bfs_list[tail++];
		cursegp = &Segments[curseg];

		for (i=0; i<MAX_SIDES_PER_SEGMENT; i++) {
			int	connected_seg;

			connected_seg = cursegp->children[i];

			if (IS_CHILD(connected_seg) && (visited[connected_seg] == 0)) {
				if (segment_is_reachable(curseg, i)) {
					bfs_list[head++] = connected_seg;
					if (head >= max_segs)
						break;
					visited[connected_seg] = 1;
					Assert(head < MAX_SEGMENTS);
				}
			}
		}
	}

	*length = head;
	
}

//	-----------------------------------------------------------------------------
//	Return true if ok for buddy to talk, else return false.
//	Buddy is allowed to talk if the segment he is in does not contain a blastable wall that has not been blasted
//	AND he has never yet, since being initialized for level, been allowed to talk.
int ok_for_buddy_to_talk(void)
{
	int		i;
	segment	*segp;

	if (Buddy_objnum == -1)
		return 0;

	if (Objects[Buddy_objnum].type != OBJ_ROBOT) {
		Buddy_allowed_to_talk = 0;
		return 0;
	}

	if (Buddy_allowed_to_talk)
		return 1;

	if ((Objects[Buddy_objnum].type == OBJ_ROBOT) && (Buddy_objnum <= Highest_object_index) && !Robot_info[Objects[Buddy_objnum].id].companion) {
		for (i=0; i<=Highest_object_index; i++)
			if (Robot_info[Objects[i].id].companion)
				break;
		if (i > Highest_object_index)
			return 0;
		else
			Buddy_objnum = i;
	}

	segp = &Segments[Objects[Buddy_objnum].segnum];

	for (i=0; i<MAX_SIDES_PER_SEGMENT; i++) {
		int	wall_num = segp->sides[i].wall_num;

		if (wall_num != -1) {
			if ((Walls[wall_num].type == WALL_BLASTABLE) && !(Walls[wall_num].flags & WALL_BLASTED))
				return 0;
		}

		//	Check one level deeper.
		if (IS_CHILD(segp->children[i])) {
			int		j;
			segment	*csegp = &Segments[segp->children[i]];

			for (j=0; j<MAX_SIDES_PER_SEGMENT; j++) {
				int	wall2 = csegp->sides[j].wall_num;

				if (wall2 != -1) {
					if ((Walls[wall2].type == WALL_BLASTABLE) && !(Walls[wall2].flags & WALL_BLASTED))
						return 0;
				}
			}
		}
	}

	Buddy_allowed_to_talk = 1;

#ifdef NETWORK
	// android port: first player to free the guidebot becomes its owner in coop
	if ((Game_mode & GM_MULTI_COOP) && Escort_owner_player == -1) {
		Escort_owner_player = Player_num;
		if (Buddy_objnum >= 0 && Buddy_objnum <= Highest_object_index)
			Objects[Buddy_objnum].ctype.ai_info.REMOTE_OWNER = (sbyte)Player_num;
		multi_send_escort_owner(Player_num);
		HUD_init_message_literal(HM_DEFAULT, "Guide-Bot: you have control");
		ESCORT_DIAG("ownership claimed by player %d", Player_num);
	}
#endif

	return 1;
}

//	--------------------------------------------------------------------------------------------
void detect_escort_goal_accomplished(int index)
{
	int	i,j;
	int	detected = 0;

	if (!Buddy_allowed_to_talk)
		return;

	//	If goal is to go away, how can it be achieved?
	if (Escort_special_goal == ESCORT_GOAL_SCRAM)
		return;

//	See if goal found was a key.  Need to handle default goals differently.
//	Note, no buddy_met_goal sound when blow up reactor or exit.  Not great, but ok
//	since for reactor, noisy, for exit, buddy is disappearing.
if ((Escort_special_goal == -1) && (Escort_goal_index == index)) {
	detected = 1;
	goto dega_ok;
}

if ((Escort_goal_index <= ESCORT_GOAL_RED_KEY) && (index >= 0)) {
	if (Objects[index].type == OBJ_POWERUP)  {
		if (Objects[index].id == POW_KEY_BLUE) {
			if (Escort_goal_index == ESCORT_GOAL_BLUE_KEY) {
				detected = 1;
				goto dega_ok;
			}
		} else if (Objects[index].id == POW_KEY_GOLD) {
			if (Escort_goal_index == ESCORT_GOAL_GOLD_KEY) {
				detected = 1;
				goto dega_ok;
			}
		} else if (Objects[index].id == POW_KEY_RED) {
			if (Escort_goal_index == ESCORT_GOAL_RED_KEY) {
				detected = 1;
				goto dega_ok;
			}
		}
	}
}
	if (Escort_special_goal != -1)
	{
		if (Escort_special_goal == ESCORT_GOAL_ENERGYCEN) {
			if (index == -4)
				detected = 1;
			else {
				for (i=0; i<MAX_SIDES_PER_SEGMENT; i++)
					if (Segments[index].children[i] == Escort_goal_index) {
						detected = 1;
						goto dega_ok;
					} else {
						for (j=0; j<MAX_SIDES_PER_SEGMENT; j++)
							if (Segments[i].children[j] == Escort_goal_index) {
								detected = 1;
								goto dega_ok;
							}
					}
			}
		} else if ((Objects[index].type == OBJ_POWERUP) && (Escort_special_goal == ESCORT_GOAL_POWERUP))
			detected = 1;	//	Any type of powerup picked up will do.
		else if ((Objects[index].type == Objects[Escort_goal_index].type) && (Objects[index].id == Objects[Escort_goal_index].id)) {
			//	Note: This will help a little bit in making the buddy believe a goal is satisfied.  Won't work for a general goal like "find any powerup"
			// because of the insistence of both type and id matching.
			detected = 1;
		}
	}

dega_ok: ;
	if (detected && ok_for_buddy_to_talk()) {
		digi_play_sample_once(SOUND_BUDDY_MET_GOAL, F1_0);
		Escort_goal_index = -1;
		Escort_goal_object = ESCORT_GOAL_UNSPECIFIED;
		Escort_special_goal = -1;
		Looking_for_marker = -1;
	}

}

void change_guidebot_name()
{
	newmenu_item m[2];
	char text[GUIDEBOT_NAME_LEN+1]="";
	int item;

	strcpy(text,PlayerCfg.GuidebotName);

	m[0].type=NM_TYPE_INPUT; m[0].text_len = GUIDEBOT_NAME_LEN; m[0].text = text;
	m[1].type=NM_TYPE_MENU; m[1].text = TXT_OK;
	item = newmenu_do( NULL, "Enter Guide-bot name:", 2, m, NULL, NULL );

	if (item != -1) {
		strcpy(PlayerCfg.GuidebotName,text);
		strcpy(PlayerCfg.GuidebotNameReal,text);
		write_player_file();
	}
}

//	-----------------------------------------------------------------------------
void buddy_message(char * format, ... )
{
	if (Buddy_messages_suppressed)
		return;

#ifdef NETWORK
	if ((Game_mode & GM_MULTI) && !(Game_mode & GM_MULTI_COOP))
		return;
	if ((Game_mode & GM_MULTI_COOP) && Escort_owner_player != Player_num)
		return;
#endif

	if (Last_buddy_message_time + F1_0 < GameTime64) {
		if (ok_for_buddy_to_talk()) {
			char	gb_str[16], new_format[128];
			va_list	args;
			int t;

			va_start(args, format );
			vsprintf(new_format, format, args);
			va_end(args);

			gb_str[0] = CC_COLOR;
			gb_str[1] = BM_XRGB(28, 0, 0);
			strcpy(&gb_str[2], PlayerCfg.GuidebotName);
			t = strlen(gb_str);
			gb_str[t] = ':';
			gb_str[t+1] = CC_COLOR;
			gb_str[t+2] = BM_XRGB(0, 31, 0);
			gb_str[t+3] = 0;

			HUD_init_message(HM_DEFAULT, "%s %s", gb_str, new_format);

			Last_buddy_message_time = GameTime64;
		}
	}

}

//	-----------------------------------------------------------------------------
void thief_message(char * format, ... )
{

	char	gb_str[16], new_format[128];
	va_list	args;

	va_start(args, format );
	vsprintf(new_format, format, args);
	va_end(args);

	gb_str[0] = 1;
	gb_str[1] = BM_XRGB(28, 0, 0);
	strcpy(&gb_str[2], "THIEF:");
	gb_str[8] = 1;
	gb_str[9] = BM_XRGB(0, 31, 0);
	gb_str[10] = 0;

	HUD_init_message(HM_DEFAULT, "%s %s", gb_str, new_format);

}

//	-----------------------------------------------------------------------------
//	Return true if marker #id has been placed.
int marker_exists_in_mine(int id)
{
	int	i;

	for (i=0; i<=Highest_object_index; i++)
		if (Objects[i].type == OBJ_MARKER)
			if (Objects[i].id == id)
				return 1;

	return 0;
}

//	-----------------------------------------------------------------------------
void set_escort_special_goal(int special_key)
{
	int marker_key;

	Buddy_messages_suppressed = 0;

	if (!Buddy_allowed_to_talk) {
		ok_for_buddy_to_talk();
		if (!Buddy_allowed_to_talk) {
			int	i;

			for (i=0; i<=Highest_object_index; i++)
				if ((Objects[i].type == OBJ_ROBOT) && Robot_info[Objects[i].id].companion) {
					HUD_init_message(HM_DEFAULT, "%s has not been released.",PlayerCfg.GuidebotName);
					break;
				}
			if (i == Highest_object_index+1)
				HUD_init_message_literal(HM_DEFAULT, "No Guide-Bot in mine.");

			return;
		}
	}

	special_key = special_key & (~KEY_SHIFTED);

	marker_key = special_key;
	
	if (Last_buddy_key == special_key)
	{
		if ((Looking_for_marker == -1) && (special_key != KEY_0)) {
			if (marker_exists_in_mine(marker_key - KEY_1))
				Looking_for_marker = marker_key - KEY_1;
			else {
				Last_buddy_message_time = 0;	//	Force this message to get through.
				buddy_message("Marker %i not placed.", marker_key - KEY_1 + 1);
				Looking_for_marker = -1;
			}
		} else {
			Looking_for_marker = -1;
		}
	}

	Last_buddy_key = special_key;

	if (special_key == KEY_0)
		Looking_for_marker = -1;
		
	if ( Looking_for_marker != -1 ) {
		Escort_special_goal = ESCORT_GOAL_MARKER1 + marker_key - KEY_1;
	} else {
		switch (special_key) {
			case KEY_1:	Escort_special_goal = ESCORT_GOAL_ENERGY;			break;
			case KEY_2:	Escort_special_goal = ESCORT_GOAL_ENERGYCEN;		break;
			case KEY_3:	Escort_special_goal = ESCORT_GOAL_SHIELD;			break;
			case KEY_4:	Escort_special_goal = ESCORT_GOAL_POWERUP;		break;
			case KEY_5:	Escort_special_goal = ESCORT_GOAL_ROBOT;			break;
			case KEY_6:	Escort_special_goal = ESCORT_GOAL_HOSTAGE;		break;
			case KEY_7:	Escort_special_goal = ESCORT_GOAL_SCRAM;			break;
			case KEY_8:	Escort_special_goal = ESCORT_GOAL_PLAYER_SPEW;	break;
			case KEY_9:	Escort_special_goal = ESCORT_GOAL_EXIT;			break;
			case KEY_0:	Escort_special_goal = -1;								break;
			default:
				Int3();		//	Oops, called with illegal key value.
		}
	}

	Last_buddy_message_time = GameTime64 - 2*F1_0;	//	Allow next message to come through.

	say_escort_goal(Escort_special_goal);
	// -- Escort_goal_object = escort_set_goal_object();

	Escort_goal_object = ESCORT_GOAL_UNSPECIFIED;
}

void input_demo_apply_recorded_guidebot_goal(int special_key, int from_menu)
{
	if (from_menu) {
		Looking_for_marker = -1;
		Last_buddy_key = -1;
		set_escort_special_goal(special_key);
		Last_buddy_key = -1;
		return;
	}
	set_escort_special_goal(special_key);
}

//	-----------------------------------------------------------------------------
//	Return id of boss.
int get_boss_id(void)
{
	int	i;

	for (i=0; i<=Highest_object_index; i++)
		if (Objects[i].type == OBJ_ROBOT)
			if (Robot_info[Objects[i].id].boss_flag)
				return Objects[i].id;

	return -1;
}

//	-----------------------------------------------------------------------------
//	Return object index if object of objtype, objid exists in mine, else return -1
//	"special" is used to find objects spewed by player which is hacked into flags field of powerup.
int exists_in_mine_2(int segnum, int objtype, int objid, int special)
{
	if (Segments[segnum].objects != -1) {
		int		objnum = Segments[segnum].objects;

		while (objnum != -1) {
			object	*curobjp = &Objects[objnum];

			if (special == ESCORT_GOAL_PLAYER_SPEW) {
				if (curobjp->flags & OF_PLAYER_DROPPED)
					return objnum;
			}

			if (curobjp->type == objtype) {
				//	Don't find escort robots if looking for robot!
				if ((curobjp->type == OBJ_ROBOT) && (Robot_info[curobjp->id].companion))
					;
				else if (objid == -1) {
					if ((objtype == OBJ_POWERUP) && (curobjp->id != POW_KEY_BLUE) && (curobjp->id != POW_KEY_GOLD) && (curobjp->id != POW_KEY_RED))
						return objnum;
					else
						return objnum;
				} else if (curobjp->id == objid)
					return objnum;
			}

			if (objtype == OBJ_POWERUP)
				if (curobjp->contains_count)
					if (curobjp->contains_type == OBJ_POWERUP)
						if (curobjp->contains_id == objid)
							return objnum;

			objnum = curobjp->next;
		}
	}

	return -1;
}

//	-----------------------------------------------------------------------------
//	Return nearest object of interest.
//	If special == ESCORT_GOAL_PLAYER_SPEW, then looking for any object spewed by player.
//	-1 means object does not exist in mine.
//	-2 means object does exist in mine, but buddy-bot can't reach it (eg, behind triggered wall)
int exists_in_mine(int start_seg, int objtype, int objid, int special)
{
	int	segindex, segnum;
	short	bfs_list[MAX_SEGMENTS];
	int	length;

	create_bfs_list(start_seg, bfs_list, &length, MAX_SEGMENTS);

	if (objtype == FUELCEN_CHECK) {
		for (segindex=0; segindex<length; segindex++) {
			segnum = bfs_list[segindex];
			if (Segment2s[segnum].special == SEGMENT_IS_FUELCEN)
				return segnum;
		}
	} else {
		for (segindex=0; segindex<length; segindex++) {
			int	objnum;

			segnum = bfs_list[segindex];

			objnum = exists_in_mine_2(segnum, objtype, objid, special);
			if (objnum != -1)
				return objnum;

		}
	}

	//	Couldn't find what we're looking for by looking at connectivity.
	//	See if it's in the mine.  It could be hidden behind a trigger or switch
	//	which the buddybot doesn't understand.
	if (objtype == FUELCEN_CHECK) {
		for (segnum=0; segnum<=Highest_segment_index; segnum++)
			if (Segment2s[segnum].special == SEGMENT_IS_FUELCEN)
				return -2;
	} else {
		for (segnum=0; segnum<=Highest_segment_index; segnum++) {
			int	objnum;

			objnum = exists_in_mine_2(segnum, objtype, objid, special);
			if (objnum != -1)
				return -2;
		}
	}

	return -1;
}

//	-----------------------------------------------------------------------------
//	Return true if it happened, else return false.
int find_exit_segment(void)
{
	int	i,j;

	//	---------- Find exit doors ----------
	for (i=0; i<=Highest_segment_index; i++)
		for (j=0; j<MAX_SIDES_PER_SEGMENT; j++)
			if (Segments[i].children[j] == -2) {
				return i;
			}

	return -1;
}

#define	BUDDY_MARKER_TEXT_LEN	25

//	-----------------------------------------------------------------------------
void say_escort_goal(int goal_num)
{
	if (Player_is_dead)
		return;

	switch (goal_num) {
		case ESCORT_GOAL_BLUE_KEY:		buddy_message("Finding BLUE KEY");			break;
		case ESCORT_GOAL_GOLD_KEY:		buddy_message("Finding YELLOW KEY");		break;
		case ESCORT_GOAL_RED_KEY:		buddy_message("Finding RED KEY");			break;
		case ESCORT_GOAL_CONTROLCEN:	buddy_message("Finding REACTOR");			break;
		case ESCORT_GOAL_EXIT:			buddy_message("Finding EXIT");				break;
		case ESCORT_GOAL_ENERGY:		buddy_message("Finding ENERGY");				break;
		case ESCORT_GOAL_ENERGYCEN:	buddy_message("Finding ENERGY CENTER");	break;
		case ESCORT_GOAL_SHIELD:		buddy_message("Finding a SHIELD");			break;
		case ESCORT_GOAL_POWERUP:		buddy_message("Finding a POWERUP");			break;
		case ESCORT_GOAL_ROBOT:			buddy_message("Finding a ROBOT");			break;
		case ESCORT_GOAL_HOSTAGE:		buddy_message("Finding a HOSTAGE");			break;
		case ESCORT_GOAL_SCRAM:			buddy_message("Staying away...");			break;
		case ESCORT_GOAL_BOSS:			buddy_message("Finding BOSS robot");		break;
		case ESCORT_GOAL_PLAYER_SPEW:	buddy_message("Finding your powerups");	break;
		case ESCORT_GOAL_MARKER1:
		case ESCORT_GOAL_MARKER2:
		case ESCORT_GOAL_MARKER3:
		case ESCORT_GOAL_MARKER4:
		case ESCORT_GOAL_MARKER5:
		case ESCORT_GOAL_MARKER6:
		case ESCORT_GOAL_MARKER7:
		case ESCORT_GOAL_MARKER8:
		case ESCORT_GOAL_MARKER9:
			{ char marker_text[BUDDY_MARKER_TEXT_LEN];
			strncpy(marker_text, MarkerMessage[goal_num-ESCORT_GOAL_MARKER1], BUDDY_MARKER_TEXT_LEN-1);
			marker_text[BUDDY_MARKER_TEXT_LEN-1] = 0;
			buddy_message("Finding marker %i: '%s'", goal_num-ESCORT_GOAL_MARKER1+1, marker_text);
			break;
			}
	}
}

//	-----------------------------------------------------------------------------
void escort_create_path_to_goal(object *objp)
{
	int	goal_seg = -1;
	int			objnum = objp-Objects;
	ai_static	*aip = &objp->ctype.ai_info;
	ai_local		*ailp = &Ai_local_info[objnum];
	int			replay_goal_probe_active = input_demo_trace_escort_active() &&
		objnum == 28;

	if (replay_goal_probe_active)
		con_printf(CON_NORMAL,
			"Input demo replay escort goal probe: frame=%u step=entry obj=%d seg=%d mode=%d behavior=%d goal=%d special=%d cur_path=%d/%d hide=%d\n",
			input_demo_trace_frame_index(),
			objnum,
			objp->segnum,
			ailp->mode,
			aip->behavior,
			Escort_goal_object,
			Escort_special_goal,
			aip->cur_path_index,
			aip->path_length,
			aip->hide_index);

	if (Escort_special_goal != -1)
		Escort_goal_object = Escort_special_goal;

	Escort_kill_object = -1;

	if (Looking_for_marker != -1) {

		Escort_goal_index = exists_in_mine(objp->segnum, OBJ_MARKER, Escort_goal_object-ESCORT_GOAL_MARKER1, -1);
		if (Escort_goal_index > -1)
			goal_seg = Objects[Escort_goal_index].segnum;
	} else {
		switch (Escort_goal_object) {
			case ESCORT_GOAL_BLUE_KEY:
				Escort_goal_index = exists_in_mine(objp->segnum, OBJ_POWERUP, POW_KEY_BLUE, -1);
				if (Escort_goal_index > -1) goal_seg = Objects[Escort_goal_index].segnum;
				break;
			case ESCORT_GOAL_GOLD_KEY:
				Escort_goal_index = exists_in_mine(objp->segnum, OBJ_POWERUP, POW_KEY_GOLD, -1);
				if (Escort_goal_index > -1) goal_seg = Objects[Escort_goal_index].segnum;
				break;
			case ESCORT_GOAL_RED_KEY:
				Escort_goal_index = exists_in_mine(objp->segnum, OBJ_POWERUP, POW_KEY_RED, -1);
				if (Escort_goal_index > -1) goal_seg = Objects[Escort_goal_index].segnum;
				break;
			case ESCORT_GOAL_CONTROLCEN:
				Escort_goal_index = exists_in_mine(objp->segnum, OBJ_CNTRLCEN, -1, -1);
				if (Escort_goal_index > -1) goal_seg = Objects[Escort_goal_index].segnum;
				break;
			case ESCORT_GOAL_EXIT:
			case ESCORT_GOAL_EXIT2:
				goal_seg = find_exit_segment();
				Escort_goal_index = goal_seg;
				break;
			case ESCORT_GOAL_ENERGY:
				Escort_goal_index = exists_in_mine(objp->segnum, OBJ_POWERUP, POW_ENERGY, -1);
				if (Escort_goal_index > -1) goal_seg = Objects[Escort_goal_index].segnum;
				break;
			case ESCORT_GOAL_ENERGYCEN:
				goal_seg = exists_in_mine(objp->segnum, FUELCEN_CHECK, -1, -1);
				Escort_goal_index = goal_seg;
				break;
			case ESCORT_GOAL_SHIELD:
				Escort_goal_index = exists_in_mine(objp->segnum, OBJ_POWERUP, POW_SHIELD_BOOST, -1);
				if (Escort_goal_index > -1) goal_seg = Objects[Escort_goal_index].segnum;
				break;
			case ESCORT_GOAL_POWERUP:
				Escort_goal_index = exists_in_mine(objp->segnum, OBJ_POWERUP, -1, -1);
				if (Escort_goal_index > -1) goal_seg = Objects[Escort_goal_index].segnum;
				break;
			case ESCORT_GOAL_ROBOT:
				Escort_goal_index = exists_in_mine(objp->segnum, OBJ_ROBOT, -1, -1);
				if (Escort_goal_index > -1) goal_seg = Objects[Escort_goal_index].segnum;
				break;
			case ESCORT_GOAL_HOSTAGE:
				Escort_goal_index = exists_in_mine(objp->segnum, OBJ_HOSTAGE, -1, -1);
				if (Escort_goal_index > -1) goal_seg = Objects[Escort_goal_index].segnum;
				break;
			case ESCORT_GOAL_PLAYER_SPEW:
				Escort_goal_index = exists_in_mine(objp->segnum, -1, -1, ESCORT_GOAL_PLAYER_SPEW);
				if (Escort_goal_index > -1) goal_seg = Objects[Escort_goal_index].segnum;
				break;
			case ESCORT_GOAL_SCRAM:
				goal_seg = -3;		//	Kinda a hack.
				Escort_goal_index = goal_seg;
				break;
			case ESCORT_GOAL_BOSS: {
				int	boss_id;
	
				boss_id = get_boss_id();
				Assert(boss_id != -1);
				Escort_goal_index = exists_in_mine(objp->segnum, OBJ_ROBOT, boss_id, -1);
				if (Escort_goal_index > -1) goal_seg = Objects[Escort_goal_index].segnum;
				break;
			}
			default:
				Int3();	//	Oops, Illegal value in Escort_goal_object.
				goal_seg = 0;
				break;
		}
	}

	if (replay_goal_probe_active)
		con_printf(CON_NORMAL,
			"Input demo replay escort goal probe: frame=%u step=resolved obj=%d goal=%d special=%d goal_index=%d goal_seg=%d cur_path=%d/%d hide=%d\n",
			input_demo_trace_frame_index(),
			objnum,
			Escort_goal_object,
			Escort_special_goal,
			Escort_goal_index,
			goal_seg,
			aip->cur_path_index,
			aip->path_length,
			aip->hide_index);

	if ((Escort_goal_index < 0) && (Escort_goal_index != -3)) {	//	I apologize for this statement -- MK, 09/22/95
		if (Escort_goal_index == -1) {
			Last_buddy_message_time = 0;	//	Force this message to get through.
			buddy_message("No %s in mine.", Escort_goal_text[Escort_goal_object-1]);
			Looking_for_marker = -1;
		} else if (Escort_goal_index == -2) {
			Last_buddy_message_time = 0;	//	Force this message to get through.
			buddy_message("Can't reach %s.", Escort_goal_text[Escort_goal_object-1]);
			Looking_for_marker = -1;
		} else
			Int3();

		Escort_goal_object = ESCORT_GOAL_UNSPECIFIED;
		Escort_special_goal = -1;
	} else {
		if (goal_seg == -3) {
			create_n_segment_path(objp, 16 + d_rand() * 16, -1);
			aip->path_length = polish_path(objp, &Point_segs[aip->hide_index], aip->path_length);
			input_demo_log_escort_path_state("escort_create_path_to_goal scram", objp);
		} else {
			create_path_to_segment(objp, goal_seg, Max_escort_length, 1);	//	MK!: Last parm (safety_flag) used to be 1!!
			if (aip->path_length > 3)
				aip->path_length = polish_path(objp, &Point_segs[aip->hide_index], aip->path_length);
			input_demo_log_escort_path_state("escort_create_path_to_goal to_segment", objp);
			if ((aip->path_length > 0) && (Point_segs[aip->hide_index + aip->path_length - 1].segnum != goal_seg)) {
				fix	dist_to_player;
				Last_buddy_message_time = 0;	//	Force this message to get through.
				buddy_message("Can't reach %s.", Escort_goal_text[Escort_goal_object-1]);
				Looking_for_marker = -1;
				Escort_goal_object = ESCORT_GOAL_SCRAM;
				dist_to_player = find_connected_distance(&objp->pos, objp->segnum, &Believed_player_pos, Believed_player_seg, 100, WID_FLY_FLAG);
				if (dist_to_player > MIN_ESCORT_DISTANCE)
					create_path_to_player(objp, Max_escort_length, 1);	//	MK!: Last parm used to be 1!
				else {
					create_n_segment_path(objp, 8 + d_rand() * 8, -1);
					aip->path_length = polish_path(objp, &Point_segs[aip->hide_index], aip->path_length);
					input_demo_log_escort_path_state("escort_create_path_to_goal fallback_scram", objp);
				}
			}
		}

		ailp->mode = AIM_GOTO_OBJECT;

		say_escort_goal(Escort_goal_object);
	}

}

//	-----------------------------------------------------------------------------
//	Escort robot chooses goal object based on player's keys, location.
//	Returns goal object.
int escort_set_goal_object(void)
{
	if (Escort_special_goal != -1)
		return ESCORT_GOAL_UNSPECIFIED;
	else if (!(ConsoleObject->flags & PLAYER_FLAGS_BLUE_KEY) && (exists_in_mine(ConsoleObject->segnum, OBJ_POWERUP, POW_KEY_BLUE, -1) != -1))
		return ESCORT_GOAL_BLUE_KEY;
	else if (!(ConsoleObject->flags & PLAYER_FLAGS_GOLD_KEY) && (exists_in_mine(ConsoleObject->segnum, OBJ_POWERUP, POW_KEY_GOLD, -1) != -1))
		return ESCORT_GOAL_GOLD_KEY;
	else if (!(ConsoleObject->flags & PLAYER_FLAGS_RED_KEY) && (exists_in_mine(ConsoleObject->segnum, OBJ_POWERUP, POW_KEY_RED, -1) != -1))
		return ESCORT_GOAL_RED_KEY;
	else if (Control_center_destroyed == 0) {
		if (Num_boss_teleport_segs)
			return ESCORT_GOAL_BOSS;
		else
			return ESCORT_GOAL_CONTROLCEN;
	} else
		return ESCORT_GOAL_EXIT;
	
}

#define	MAX_ESCORT_TIME_AWAY		(F1_0*4)

fix64	Buddy_last_seen_player = 0, Buddy_last_player_path_created;

//	-----------------------------------------------------------------------------
int time_to_visit_player(object *objp, ai_local *ailp, ai_static *aip)
{
	//	Note: This one has highest priority because, even if already going towards player,
	//	might be necessary to create a new path, as player can move.
	if (GameTime64 - Buddy_last_seen_player > MAX_ESCORT_TIME_AWAY)
		if (GameTime64 - Buddy_last_player_path_created > F1_0)
			return 1;

	if (ailp->mode == AIM_GOTO_PLAYER)
		return 0;

	if (objp->segnum == ConsoleObject->segnum)
		return 0;

	if (aip->cur_path_index < aip->path_length/2)
		return 0;
	
	return 1;
}

int	Buddy_objnum;
fix64	Last_come_back_message_time = 0;

fix64	Buddy_last_missile_time;
extern fix64	Re_init_thief_time;

void escort_get_input_demo_checkpoint_state(input_demo_checkpoint_escort_state *escort_state)
{
	if (!escort_state)
		return;

	input_demo_checkpoint_escort_state_clear(escort_state);
	escort_state->valid = 1;
	escort_state->buddy_allowed_to_talk = Buddy_allowed_to_talk;
	escort_state->buddy_last_seen_player = Buddy_last_seen_player;
	escort_state->buddy_last_player_path_created = Buddy_last_player_path_created;
	escort_state->escort_kill_object = Escort_kill_object;
	escort_state->escort_last_path_created = Escort_last_path_created;
	escort_state->escort_goal_object = Escort_goal_object;
	escort_state->escort_special_goal = Escort_special_goal;
	escort_state->escort_goal_index = Escort_goal_index;
	escort_state->buddy_messages_suppressed = Buddy_messages_suppressed;
	escort_state->buddy_sorry_time = Buddy_sorry_time;
	escort_state->looking_for_marker = Looking_for_marker;
	escort_state->last_buddy_key = Last_buddy_key;
	escort_state->last_buddy_message_time = Last_buddy_message_time;
	escort_state->last_come_back_message_time = Last_come_back_message_time;
	escort_state->buddy_last_missile_time = Buddy_last_missile_time;
#ifdef NETWORK
	escort_state->escort_owner_player = Escort_owner_player;
#endif
}

void escort_get_input_demo_checkpoint_thief_state(input_demo_checkpoint_thief_state *thief_state)
{
	if (!thief_state)
		return;

	input_demo_checkpoint_thief_state_clear(thief_state);
	thief_state->valid = 1;
	thief_state->stolen_item_index = Stolen_item_index;
	thief_state->re_init_thief_time = Re_init_thief_time;
	thief_state->last_thief_hit_time = Last_thief_hit_time;
}

void escort_rebuild_runtime_state_after_restore(void)
{
	ai_local *ailp = NULL;
	object *buddy_objp = NULL;
	input_demo_checkpoint_escort_state checkpoint_escort_state;
	input_demo_checkpoint_thief_state checkpoint_thief_state;
	int have_checkpoint_escort_state;
	int have_checkpoint_thief_state;
	fix64 raw_time_player_seen;
	fix64 raw_escort_last_path_created;
	int i;

	input_demo_reset_escort_state_probes();
	input_demo_checkpoint_escort_state_clear(&checkpoint_escort_state);
	input_demo_checkpoint_thief_state_clear(&checkpoint_thief_state);
	have_checkpoint_escort_state = input_demo_replay_get_checkpoint_escort_state(&checkpoint_escort_state);
	have_checkpoint_thief_state = input_demo_replay_get_checkpoint_thief_state(&checkpoint_thief_state);

	Buddy_objnum = -1;
	Buddy_last_seen_player = 0;
	Buddy_last_player_path_created = 0;
	Last_come_back_message_time = 0;
	Buddy_last_missile_time = 0;

	for (i = 0; i <= Highest_object_index; i++)
		if ((Objects[i].type == OBJ_ROBOT) && Robot_info[Objects[i].id].companion) {
			Buddy_objnum = i;
			break;
		}

	if (Buddy_objnum != -1) {
		buddy_objp = &Objects[Buddy_objnum];
		ailp = &Ai_local_info[Buddy_objnum];
	}

	if (have_checkpoint_thief_state) {
		Stolen_item_index = checkpoint_thief_state.stolen_item_index;
		Re_init_thief_time = checkpoint_thief_state.re_init_thief_time;
		Last_thief_hit_time = checkpoint_thief_state.last_thief_hit_time;
	}

	if (have_checkpoint_escort_state) {
		Buddy_allowed_to_talk = checkpoint_escort_state.buddy_allowed_to_talk;
		Buddy_last_seen_player = checkpoint_escort_state.buddy_last_seen_player;
		Buddy_last_player_path_created = checkpoint_escort_state.buddy_last_player_path_created;
		if (checkpoint_escort_state.escort_kill_object != INPUT_DEMO_CHECKPOINT_ESCORT_INT_UNSET)
			Escort_kill_object = checkpoint_escort_state.escort_kill_object;
		Escort_last_path_created = checkpoint_escort_state.escort_last_path_created;
		if (checkpoint_escort_state.escort_goal_object != INPUT_DEMO_CHECKPOINT_ESCORT_INT_UNSET)
			Escort_goal_object = checkpoint_escort_state.escort_goal_object;
		if (checkpoint_escort_state.escort_special_goal != INPUT_DEMO_CHECKPOINT_ESCORT_INT_UNSET)
			Escort_special_goal = checkpoint_escort_state.escort_special_goal;
		if (checkpoint_escort_state.escort_goal_index != INPUT_DEMO_CHECKPOINT_ESCORT_INT_UNSET)
			Escort_goal_index = checkpoint_escort_state.escort_goal_index;
		if (checkpoint_escort_state.buddy_messages_suppressed != INPUT_DEMO_CHECKPOINT_ESCORT_INT_UNSET)
			Buddy_messages_suppressed = checkpoint_escort_state.buddy_messages_suppressed;
		if (checkpoint_escort_state.buddy_sorry_time != INPUT_DEMO_CHECKPOINT_ESCORT_I64_UNSET)
			Buddy_sorry_time = checkpoint_escort_state.buddy_sorry_time;
		if (checkpoint_escort_state.looking_for_marker != INPUT_DEMO_CHECKPOINT_ESCORT_INT_UNSET)
			Looking_for_marker = checkpoint_escort_state.looking_for_marker;
		if (checkpoint_escort_state.last_buddy_key != INPUT_DEMO_CHECKPOINT_ESCORT_INT_UNSET)
			Last_buddy_key = checkpoint_escort_state.last_buddy_key;
		if (checkpoint_escort_state.last_buddy_message_time != INPUT_DEMO_CHECKPOINT_ESCORT_I64_UNSET)
			Last_buddy_message_time = checkpoint_escort_state.last_buddy_message_time;
		Last_come_back_message_time = checkpoint_escort_state.last_come_back_message_time;
		Buddy_last_missile_time = checkpoint_escort_state.buddy_last_missile_time;
	#ifdef NETWORK
		if (checkpoint_escort_state.escort_owner_player != INPUT_DEMO_CHECKPOINT_ESCORT_INT_UNSET)
			Escort_owner_player = checkpoint_escort_state.escort_owner_player;
	#endif
		if (input_demo_trace_escort_active())
			con_printf(CON_NORMAL,
				"Input demo replay escort restore checkpoint: gt=%lld obj=%d seg=%d mode=%d talk=%d cur_path=%d/%d hide_index=%d last_seen=%lld last_player_path=%lld kill=%d escort_last_path=%lld goal=%d/%d/%d suppress=%d sorry=%lld marker=%d last_key=%d last_msg=%lld come_back=%lld last_missile=%lld seen=%lld owner=%d thief_valid=%d thief_index=%d thief_reinit=%lld thief_last_hit=%lld\n",
				(long long)GameTime64,
				Buddy_objnum,
				buddy_objp ? buddy_objp->segnum : -1,
				ailp ? ailp->mode : -1,
				Buddy_allowed_to_talk,
				buddy_objp ? buddy_objp->ctype.ai_info.cur_path_index : -1,
				buddy_objp ? buddy_objp->ctype.ai_info.path_length : -1,
				buddy_objp ? buddy_objp->ctype.ai_info.hide_index : -1,
				(long long)Buddy_last_seen_player,
				(long long)Buddy_last_player_path_created,
				Escort_kill_object,
				(long long)Escort_last_path_created,
				Escort_goal_object,
				Escort_special_goal,
				Escort_goal_index,
				Buddy_messages_suppressed,
				(long long)Buddy_sorry_time,
				Looking_for_marker,
				Last_buddy_key,
				(long long)Last_buddy_message_time,
				(long long)Last_come_back_message_time,
				(long long)Buddy_last_missile_time,
				(long long)(ailp ? ailp->time_player_seen : -1),
				Escort_owner_player,
				have_checkpoint_thief_state,
				Stolen_item_index,
				(long long)Re_init_thief_time,
				(long long)Last_thief_hit_time);
		return;
	}

	if (Buddy_objnum == -1)
		return;

	raw_time_player_seen = ailp->time_player_seen;
	raw_escort_last_path_created = Escort_last_path_created;
	Buddy_allowed_to_talk = 0;
	ok_for_buddy_to_talk();
	Buddy_last_seen_player = ailp->time_player_seen;
	if (Buddy_last_seen_player > GameTime64)
		Buddy_last_seen_player = GameTime64;
	if ((Buddy_last_seen_player < GameTime64 - MAX_ESCORT_TIME_AWAY) && ailp->previous_visibility)
		Buddy_last_seen_player = GameTime64;
	if (Buddy_last_seen_player < 0)
		Buddy_last_seen_player = 0;

	Buddy_last_player_path_created = Escort_last_path_created;
	if ((ailp->mode == AIM_GOTO_PLAYER) && (Buddy_last_player_path_created < GameTime64 - F1_0))
		Buddy_last_player_path_created = GameTime64;
	if (Buddy_last_player_path_created > GameTime64)
		Buddy_last_player_path_created = GameTime64;
	if (Buddy_last_player_path_created < 0)
		Buddy_last_player_path_created = 0;

	Last_come_back_message_time = Buddy_last_player_path_created;
	input_demo_log_escort_restore_normalization(buddy_objp, ailp, raw_time_player_seen, raw_escort_last_path_created);
	if (input_demo_trace_escort_active())
		con_printf(CON_NORMAL,
			"Input demo replay escort restore: gt=%lld obj=%d seg=%d mode=%d talk=%d cur_path=%d/%d hide_index=%d last_seen=%lld last_player_path=%lld escort_last_path=%lld seen=%lld\n",
			(long long)GameTime64,
			Buddy_objnum,
			Objects[Buddy_objnum].segnum,
			ailp->mode,
			Buddy_allowed_to_talk,
			Objects[Buddy_objnum].ctype.ai_info.cur_path_index,
			Objects[Buddy_objnum].ctype.ai_info.path_length,
			Objects[Buddy_objnum].ctype.ai_info.hide_index,
			(long long)Buddy_last_seen_player,
			(long long)Buddy_last_player_path_created,
			(long long)Escort_last_path_created,
			(long long)ailp->time_player_seen);
}

//	-----------------------------------------------------------------------------
void bash_buddy_weapon_info(int weapon_objnum)
{
	object	*objp = &Objects[weapon_objnum];

	objp->ctype.laser_info.parent_num = ConsoleObject-Objects;
	objp->ctype.laser_info.parent_type = OBJ_PLAYER;
	objp->ctype.laser_info.parent_signature = ConsoleObject->signature;
}

//	-----------------------------------------------------------------------------
int maybe_buddy_fire_mega(int objnum)
{
	object	*objp = &Objects[objnum];
	object	*buddy_objp = &Objects[Buddy_objnum];
	fix		dist, dot;
	vms_vector	vec_to_robot;
	int		weapon_objnum;

	vm_vec_sub(&vec_to_robot, &buddy_objp->pos, &objp->pos);
	dist = vm_vec_normalize_quick(&vec_to_robot);

	if (dist > F1_0*100)
		return 0;

	dot = vm_vec_dot(&vec_to_robot, &buddy_objp->orient.fvec);

	if (dot < F1_0/2)
		return 0;

	if (!object_to_object_visibility(buddy_objp, objp, FQ_TRANSWALL))
		return 0;

	if (Weapon_info[MEGA_ID].render_type == 0) {
		con_printf(CON_VERBOSE, "Buddy can't fire mega (shareware)\n");
		buddy_message("CLICK!");
		return 0;
	}

	buddy_message("GAHOOGA!");

	weapon_objnum = Laser_create_new_easy( &buddy_objp->orient.fvec, &buddy_objp->pos, objnum, MEGA_ID, 1);

	if (weapon_objnum != -1)
		bash_buddy_weapon_info(weapon_objnum);

	return 1;
}

//-----------------------------------------------------------------------------
int maybe_buddy_fire_smart(int objnum)
{
	object	*objp = &Objects[objnum];
	object	*buddy_objp = &Objects[Buddy_objnum];
	fix		dist;
	int		weapon_objnum;

	dist = vm_vec_dist_quick(&buddy_objp->pos, &objp->pos);

	if (dist > F1_0*80)
		return 0;

	if (!object_to_object_visibility(buddy_objp, objp, FQ_TRANSWALL))
		return 0;

	buddy_message("WHAMMO!");

	weapon_objnum = Laser_create_new_easy( &buddy_objp->orient.fvec, &buddy_objp->pos, objnum, SMART_ID, 1);

	if (weapon_objnum != -1)
		bash_buddy_weapon_info(weapon_objnum);

	return 1;
}

//	-----------------------------------------------------------------------------
void do_buddy_dude_stuff(void)
{
	int	i;

	if (!ok_for_buddy_to_talk())
		return;

	if (Buddy_last_missile_time > GameTime64)
		Buddy_last_missile_time = 0;

	if (Buddy_last_missile_time + F1_0*2 < GameTime64) {
		//	See if a robot potentially in view cone
		for (i=0; i<=Highest_object_index; i++)
			if ((Objects[i].type == OBJ_ROBOT) && !Robot_info[Objects[i].id].companion)
				if (maybe_buddy_fire_mega(i)) {
					Buddy_last_missile_time = GameTime64;
					return;
				}

		//	See if a robot near enough that buddy should fire smart missile
		for (i=0; i<=Highest_object_index; i++)
			if ((Objects[i].type == OBJ_ROBOT) && !Robot_info[Objects[i].id].companion)
				if (maybe_buddy_fire_smart(i)) {
					Buddy_last_missile_time = GameTime64;
					return;
				}

	}
}

//	-----------------------------------------------------------------------------
//	Called every frame (or something).
void do_escort_frame(object *objp, fix dist_to_player, int player_visibility)
{
	int			objnum = objp-Objects;
	ai_static	*aip = &objp->ctype.ai_info;
	ai_local		*ailp = &Ai_local_info[objnum];
	unsigned int replay_rng_state = 0;
	unsigned int replay_rng_call_count = 0;
	int replay_should_visit_player = 0;
	fix64 replay_since_seen = 0;
	fix64 replay_since_player_path = 0;
	int replay_visit_away_gate = 0;
	int replay_visit_recent_path_gate = 0;
	int replay_visit_goto_player_gate = 0;
	int replay_visit_same_seg_gate = 0;
	int replay_visit_early_path_gate = 0;
	int replay_player_seg = -1;
	int replay_believed_seg = -1;
	int replay_state_probe_active = input_demo_trace_escort_active() &&
		Robot_info[objp->id].companion;
	int replay_rng_probe_active = input_demo_trace_escort_active() &&
		Robot_info[objp->id].companion && d_rand_get_state(&replay_rng_state);
	if (replay_rng_probe_active)
		replay_rng_call_count = d_rand_get_call_count();

	Buddy_objnum = objp-Objects;

	if (player_visibility) {
		Buddy_last_seen_player = GameTime64;
		if (Players[Player_num].flags & PLAYER_FLAGS_HEADLIGHT_ON)	//	DAMN! MK, stupid bug, fixed 12/08/95, changed PLAYER_FLAGS_HEADLIGHT to PLAYER_FLAGS_HEADLIGHT_ON
			if (f2i(Players[Player_num].energy) < 40)
				if ((f2i(Players[Player_num].energy)/2) & 2)
					if (!Player_is_dead)
						buddy_message("Hey, your headlight's on!");

	}

	if (cheats.buddyangry)
		do_buddy_dude_stuff();

	if (Buddy_sorry_time + F1_0 > GameTime64) {
		Last_buddy_message_time = 0;	//	Force this message to get through.
		if (Buddy_sorry_time < GameTime64 + F1_0*2)
			buddy_message("Oops, sorry 'bout that...");
		Buddy_sorry_time = -F1_0*2;
	}

	//	If buddy not allowed to talk, then he is locked in his room.  Make him mostly do nothing unless you're nearby.
	if (!Buddy_allowed_to_talk)
		if (dist_to_player > F1_0*100)
			aip->SKIP_AI_COUNT = (F1_0/4)/FrameTime;

	//	AIM_WANDER has been co-opted for buddy behavior (didn't want to modify aistruct.h)
	//	It means the object has been told to get lost and has come to the end of its path.
	//	If the player is now visible, then create a path.
	if (ailp->mode == AIM_WANDER)
		if (player_visibility) {
			create_n_segment_path(objp, 16 + d_rand() * 16, -1);
			aip->path_length = polish_path(objp, &Point_segs[aip->hide_index], aip->path_length);
			if (replay_rng_probe_active)
				input_demo_log_escort_rng_progress("after AIM_WANDER create_n_segment_path", &replay_rng_state, &replay_rng_call_count);
		}

	if (Escort_special_goal == ESCORT_GOAL_SCRAM) {
		if (player_visibility)
			if (Escort_last_path_created + F1_0*3 < GameTime64) {
				create_n_segment_path(objp, 10 + d_rand() * 16, ConsoleObject->segnum);
				Escort_last_path_created = GameTime64;
				if (replay_rng_probe_active)
					input_demo_log_escort_rng_progress("after ESCORT_GOAL_SCRAM create_n_segment_path", &replay_rng_state, &replay_rng_call_count);
			}

		return;
	}

	//	Force checking for new goal every 5 seconds, and create new path, if necessary.
	if (((Escort_special_goal != ESCORT_GOAL_SCRAM) && ((Escort_last_path_created + F1_0*5) < GameTime64)) ||
		((Escort_special_goal == ESCORT_GOAL_SCRAM) && ((Escort_last_path_created + F1_0*15) < GameTime64))) {
		if (replay_state_probe_active)
			con_printf(CON_NORMAL,
				"Input demo replay escort goal reset: frame=%u gt=%lld goal=%d special=%d last_path=%lld\n",
				input_demo_trace_frame_index(),
				(long long)GameTime64,
				Escort_goal_object,
				Escort_special_goal,
				(long long)Escort_last_path_created);
		Escort_goal_object = ESCORT_GOAL_UNSPECIFIED;
		Escort_last_path_created = GameTime64;
	}
	if (replay_state_probe_active) {
		replay_player_seg = ConsoleObject->segnum;
		replay_believed_seg = Believed_player_seg;
		input_demo_log_escort_segment_change(objp, ailp, aip, replay_player_seg, replay_believed_seg);
		replay_since_seen = GameTime64 - Buddy_last_seen_player;
		replay_since_player_path = GameTime64 - Buddy_last_player_path_created;
		replay_visit_away_gate = replay_since_seen > MAX_ESCORT_TIME_AWAY;
		replay_visit_recent_path_gate = replay_since_player_path <= F1_0;
		replay_visit_goto_player_gate = ailp->mode == AIM_GOTO_PLAYER;
		replay_visit_same_seg_gate = objp->segnum == replay_player_seg;
		replay_visit_early_path_gate = aip->cur_path_index < aip->path_length/2;
		replay_should_visit_player = (Escort_special_goal != ESCORT_GOAL_SCRAM) && time_to_visit_player(objp, ailp, aip);
		input_demo_log_escort_visit_change(objp, ailp, aip, replay_player_seg, replay_believed_seg,
			replay_visit_away_gate, replay_visit_recent_path_gate, replay_visit_goto_player_gate,
			replay_visit_same_seg_gate, replay_visit_early_path_gate, replay_should_visit_player);
	}
	if (replay_state_probe_active)
		input_demo_log_escort_state(objp, ailp, aip, dist_to_player, player_visibility,
			replay_should_visit_player, replay_since_seen, replay_since_player_path,
			replay_visit_away_gate, replay_visit_recent_path_gate,
			replay_visit_goto_player_gate, replay_visit_same_seg_gate,
			replay_visit_early_path_gate);

	if ((Escort_special_goal != ESCORT_GOAL_SCRAM) && (replay_state_probe_active ? replay_should_visit_player : time_to_visit_player(objp, ailp, aip))) {
		int	max_len;

		Buddy_last_player_path_created = GameTime64;
		ailp->mode = AIM_GOTO_PLAYER;
		if (!player_visibility) {
			if ((Last_come_back_message_time + F1_0 < GameTime64) || (Last_come_back_message_time > GameTime64)) {
				buddy_message("Coming back to get you.");
				Last_come_back_message_time = GameTime64;
			}
		}
		//	No point in Buddy creating very long path if he's not allowed to talk.  Really kills framerate.
		max_len = Max_escort_length;
		if (!Buddy_allowed_to_talk)
			max_len = 3;
		create_path_to_player(objp, max_len, 1);	//	MK!: Last parm used to be 1!
		aip->path_length = polish_path(objp, &Point_segs[aip->hide_index], aip->path_length);
		input_demo_log_escort_path_state("time_to_visit_player final", objp);
		if (replay_rng_probe_active)
			input_demo_log_escort_rng_progress("after time_to_visit_player create_path_to_player", &replay_rng_state, &replay_rng_call_count);
		ailp->mode = AIM_GOTO_PLAYER;
	} else if (GameTime64 - Buddy_last_seen_player > MAX_ESCORT_TIME_AWAY) {
		//	This is to prevent buddy from looking for a goal, which he will do because we only allow path creation once/second.
		return;
	} else if ((ailp->mode == AIM_GOTO_PLAYER) &&
		(aip->cur_path_index >= aip->path_length/2) &&
		(dist_to_player < MIN_ESCORT_DISTANCE - F1_0/4)) {
		Escort_goal_object = escort_set_goal_object();
		ailp->mode = AIM_GOTO_OBJECT;		//	May look stupid to be before path creation, but ai_door_is_openable uses mode to determine what doors can be got through
		escort_create_path_to_goal(objp);
		if (replay_rng_probe_active)
			input_demo_log_escort_rng_progress("after AIM_GOTO_PLAYER escort_create_path_to_goal", &replay_rng_state, &replay_rng_call_count);
		aip->path_length = polish_path(objp, &Point_segs[aip->hide_index], aip->path_length);
			input_demo_log_escort_path_state("AIM_GOTO_PLAYER final", objp);
		if (aip->path_length < 3) {
			create_n_segment_path(objp, 5, Believed_player_seg);
				input_demo_log_escort_path_state("AIM_GOTO_PLAYER fallback", objp);
			if (replay_rng_probe_active)
				input_demo_log_escort_rng_progress("after AIM_GOTO_PLAYER fallback create_n_segment_path", &replay_rng_state, &replay_rng_call_count);
		}
		ailp->mode = AIM_GOTO_OBJECT;
	} else if (Escort_goal_object == ESCORT_GOAL_UNSPECIFIED) {
		if ((ailp->mode != AIM_GOTO_PLAYER) ||
			((aip->cur_path_index >= aip->path_length/2) &&
			 (dist_to_player < MIN_ESCORT_DISTANCE - F1_0/4))) {
			Escort_goal_object = escort_set_goal_object();
			ailp->mode = AIM_GOTO_OBJECT;		//	May look stupid to be before path creation, but ai_door_is_openable uses mode to determine what doors can be got through
			escort_create_path_to_goal(objp);
			if (replay_rng_probe_active)
				input_demo_log_escort_rng_progress("after unspecified escort_create_path_to_goal", &replay_rng_state, &replay_rng_call_count);
			aip->path_length = polish_path(objp, &Point_segs[aip->hide_index], aip->path_length);
			input_demo_log_escort_path_state("unspecified goal final", objp);
			if (aip->path_length < 3) {
				create_n_segment_path(objp, 5, Believed_player_seg);
				input_demo_log_escort_path_state("unspecified fallback", objp);
				if (replay_rng_probe_active)
					input_demo_log_escort_rng_progress("after unspecified fallback create_n_segment_path", &replay_rng_state, &replay_rng_call_count);
			}
			ailp->mode = AIM_GOTO_OBJECT;
		}
	}

}

void invalidate_escort_goal(void)
{
	Escort_goal_object = -1;
}

//	-------------------------------------------------------------------------------------------------
void do_snipe_frame(object *objp, fix dist_to_player, int player_visibility, vms_vector *vec_to_player)
{
	int			objnum = objp-Objects;
	ai_local		*ailp = &Ai_local_info[objnum];
	fix			connected_distance;
	int			replay_snipe_probe_active = input_demo_trace_escort_active() && objnum == 15;

	if (replay_snipe_probe_active)
		input_demo_log_snipe_detail_probe(&g_input_demo_snipe_entry_snapshot,
			"entry", objp, ailp, player_visibility, dist_to_player);

	if (dist_to_player > F1_0*500)
		return;

	switch (ailp->mode) {
		case AIM_SNIPE_WAIT:
			if ((dist_to_player > F1_0*50) && (ailp->next_action_time > 0))
				return;

			ailp->next_action_time = SNIPE_WAIT_TIME;

			connected_distance = find_connected_distance(&objp->pos, objp->segnum, &Believed_player_pos, Believed_player_seg, 30, WID_FLY_FLAG);
			if (connected_distance < F1_0*500) {
				create_path_to_player(objp, 30, 1);
				ailp->mode = AIM_SNIPE_ATTACK;
				ailp->next_action_time = SNIPE_ATTACK_TIME;	//	have up to 10 seconds to find player.
			}
			break;

		case AIM_SNIPE_RETREAT:
		case AIM_SNIPE_RETREAT_BACKWARDS:
			if (ailp->next_action_time < 0) {
				ailp->mode = AIM_SNIPE_WAIT;
				ailp->next_action_time = SNIPE_WAIT_TIME;
			} else if ((player_visibility == 0) || (ailp->next_action_time > SNIPE_ABORT_RETREAT_TIME)) {
				ai_follow_path(objp, player_visibility, player_visibility, vec_to_player);
				ailp->mode = AIM_SNIPE_RETREAT_BACKWARDS;
			} else {
				ailp->mode = AIM_SNIPE_FIRE;
				ailp->next_action_time = SNIPE_FIRE_TIME/2;
			}
			break;

		case AIM_SNIPE_ATTACK:
			if (ailp->next_action_time < 0) {
				ailp->mode = AIM_SNIPE_RETREAT;
				ailp->next_action_time = SNIPE_WAIT_TIME;
			} else {
				ai_follow_path(objp, player_visibility, player_visibility, vec_to_player);
				if (player_visibility) {
					ailp->mode = AIM_SNIPE_FIRE;
					ailp->next_action_time = SNIPE_FIRE_TIME;
				} else
					ailp->mode = AIM_SNIPE_ATTACK;
			}
			break;

		case AIM_SNIPE_FIRE:
			if (ailp->next_action_time < 0) {
				ai_static	*aip = &objp->ctype.ai_info;
				create_n_segment_path(objp, 10 + d_rand()/2048, ConsoleObject->segnum);
				aip->path_length = polish_path(objp, &Point_segs[aip->hide_index], aip->path_length);
				if (d_rand() < 8192)
					ailp->mode = AIM_SNIPE_RETREAT_BACKWARDS;
				else
					ailp->mode = AIM_SNIPE_RETREAT;
				ailp->next_action_time = SNIPE_RETREAT_TIME;
			} else {
			}
			break;

		default:
			Int3();	//	Oops, illegal mode for snipe behavior.
			ailp->mode = AIM_SNIPE_ATTACK;
			ailp->next_action_time = F1_0;
			break;
	}

	if (replay_snipe_probe_active)
		input_demo_log_snipe_detail_probe(&g_input_demo_snipe_exit_snapshot,
			"exit", objp, ailp, player_visibility, dist_to_player);

}

#define	THIEF_DEPTH	20

extern int pick_connected_segment(object *objp, int max_depth);

//	------------------------------------------------------------------------------------------------------
//	Choose segment to recreate thief in.
int choose_thief_recreation_segment(void)
{
	int	segnum = -1;
	int	cur_drop_depth;

	cur_drop_depth = THIEF_DEPTH;

	while ((segnum == -1) && (cur_drop_depth > THIEF_DEPTH/2)) {
		segnum = pick_connected_segment(&Objects[Players[Player_num].objnum], cur_drop_depth);
		if (Segment2s[segnum].special == SEGMENT_IS_CONTROLCEN)
			segnum = -1;
		cur_drop_depth--;
	}

	if (segnum == -1) {
		return (d_rand() * Highest_segment_index) >> 15;
	} else
		return segnum;

}

extern object * create_morph_robot( segment *segp, vms_vector *object_pos, int object_id);

fix64	Re_init_thief_time = 0x3f000000;

//	----------------------------------------------------------------------
void recreate_thief(object *objp)
{
	int			segnum;
	vms_vector	center_point;
	object		*new_obj;

	segnum = choose_thief_recreation_segment();
	compute_segment_center(&center_point, &Segments[segnum]);

	new_obj = create_morph_robot( &Segments[segnum], &center_point, objp->id);
	init_ai_object(new_obj-Objects, AIB_SNIPE, -1);
	Re_init_thief_time = GameTime64 + F1_0*10;		//	In 10 seconds, re-initialize thief.
}

//	----------------------------------------------------------------------------
#define	THIEF_ATTACK_TIME		(F1_0*10)

fix	Thief_wait_times[NDL] = {F1_0*30, F1_0*25, F1_0*20, F1_0*15, F1_0*10};

//	-------------------------------------------------------------------------------------------------
void do_thief_frame(object *objp, fix dist_to_player, int player_visibility, vms_vector *vec_to_player)
{
	int			objnum = objp-Objects;
	ai_local		*ailp = &Ai_local_info[objnum];
	fix			connected_distance;
	int			replay_thief_probe_active = input_demo_trace_escort_active() && objnum == 15;

	if ((Current_level_num < 0) && (Re_init_thief_time < GameTime64)) {
		if (Re_init_thief_time > GameTime64 - F1_0*2)
			init_thief_for_level();
		Re_init_thief_time = 0x3f000000;
	}

	if ((dist_to_player > F1_0*500) && (ailp->next_action_time > 0))
		return;

	if (replay_thief_probe_active)
		input_demo_log_thief_detail_probe(&g_input_demo_thief_entry_snapshot,
			"entry", objp, ailp, player_visibility, dist_to_player);

	if (Player_is_dead)
		ailp->mode = AIM_THIEF_RETREAT;

	switch (ailp->mode) {
		case AIM_THIEF_WAIT:
			if (ailp->player_awareness_type >= PA_PLAYER_COLLISION) {
				ailp->player_awareness_type = 0;
				create_path_to_player(objp, 30, 1);
				ailp->mode = AIM_THIEF_ATTACK;
				ailp->next_action_time = THIEF_ATTACK_TIME/2;
				return;
			} else if (player_visibility) {
				create_n_segment_path(objp, 15, ConsoleObject->segnum);
				ailp->mode = AIM_THIEF_RETREAT;
				return;
			}

			if ((dist_to_player > F1_0*50) && (ailp->next_action_time > 0))
				return;

			ailp->next_action_time = Thief_wait_times[Difficulty_level]/2;

			connected_distance = find_connected_distance(&objp->pos, objp->segnum, &Believed_player_pos, Believed_player_seg, 30, WID_FLY_FLAG);
			if (connected_distance < F1_0*500) {
				create_path_to_player(objp, 30, 1);
				ailp->mode = AIM_THIEF_ATTACK;
				ailp->next_action_time = THIEF_ATTACK_TIME;	//	have up to 10 seconds to find player.
			}

			break;

		case AIM_THIEF_RETREAT:
			if (ailp->next_action_time < 0) {
				ailp->mode = AIM_THIEF_WAIT;
				ailp->next_action_time = Thief_wait_times[Difficulty_level];
			} else if ((dist_to_player < F1_0*100) || player_visibility || (ailp->player_awareness_type >= PA_PLAYER_COLLISION)) {
				ai_follow_path(objp, player_visibility, player_visibility, vec_to_player);
				if ((dist_to_player < F1_0*100) || (ailp->player_awareness_type >= PA_PLAYER_COLLISION)) {
					ai_static	*aip = &objp->ctype.ai_info;
					if (((aip->cur_path_index <=1) && (aip->PATH_DIR == -1)) || ((aip->cur_path_index >= aip->path_length-1) && (aip->PATH_DIR == 1))) {
						ailp->player_awareness_type = 0;
						create_n_segment_path(objp, 10, ConsoleObject->segnum);

						//	If path is real short, try again, allowing to go through player's segment
						if (aip->path_length < 4) {
							create_n_segment_path(objp, 10, -1);
						} else if (objp->shields* 4 < Robot_info[objp->id].strength) {
							//	If robot really low on hits, will run through player with even longer path
							if (aip->path_length < 8) {
								create_n_segment_path(objp, 10, -1);
							}
						}

						ailp->mode = AIM_THIEF_RETREAT;
					}
				} else
					ailp->mode = AIM_THIEF_RETREAT;

			}

			break;

		//	This means the thief goes from wherever he is to the player.
		//	Note: When thief successfully steals something, his action time is forced negative and his mode is changed
		//			to retreat to get him out of attack mode.
		case AIM_THIEF_ATTACK:
			if (ailp->player_awareness_type >= PA_PLAYER_COLLISION) {
				ailp->player_awareness_type = 0;
				if (d_rand() > 8192) {
					create_n_segment_path(objp, 10, ConsoleObject->segnum);
					Ai_local_info[objp-Objects].next_action_time = Thief_wait_times[Difficulty_level]/2;
					Ai_local_info[objp-Objects].mode = AIM_THIEF_RETREAT;
				}
			} else if (ailp->next_action_time < 0) {
				//	This forces him to create a new path every second.
				ailp->next_action_time = F1_0;
				create_path_to_player(objp, 100, 0);
				ailp->mode = AIM_THIEF_ATTACK;
			} else {
				if (player_visibility && (dist_to_player < F1_0*100)) {
					//	If the player is close to looking at the thief, thief shall run away.
					//	No more stupid thief trying to sneak up on you when you're looking right at him!
					if (dist_to_player > F1_0*60) {
						fix	dot = vm_vec_dot(vec_to_player, &ConsoleObject->orient.fvec);
						if (dot < -F1_0/2) {	//	Looking at least towards thief, so thief will run!
							create_n_segment_path(objp, 10, ConsoleObject->segnum);
							Ai_local_info[objp-Objects].next_action_time = Thief_wait_times[Difficulty_level]/2;
							Ai_local_info[objp-Objects].mode = AIM_THIEF_RETREAT;
						}
					} 
					ai_turn_towards_vector(vec_to_player, objp, F1_0/4);
					move_towards_player(objp, vec_to_player);
				} else {
					ai_static	*aip = &objp->ctype.ai_info;
					//	If path length == 0, then he will keep trying to create path, but he is probably stuck in his closet.
					if ((aip->path_length > 1) || ((d_tick_count & 0x0f) == 0)) {
						ai_follow_path(objp, player_visibility, player_visibility, vec_to_player);
						ailp->mode = AIM_THIEF_ATTACK;
					}
				}
			}
			break;

		default:
			ailp->mode = AIM_THIEF_ATTACK;
			ailp->next_action_time = F1_0;
			break;
	}

	if (replay_thief_probe_active)
		input_demo_log_thief_detail_probe(&g_input_demo_thief_exit_snapshot,
			"exit", objp, ailp, player_visibility, dist_to_player);

}

//	----------------------------------------------------------------------------
//	Return true if this item (whose presence is indicated by Players[player_num].flags) gets stolen.
int maybe_steal_flag_item(int player_num, int flagval)
{
	if (Players[player_num].flags & flagval) {
		if (d_rand() < THIEF_PROBABILITY) {
			int	powerup_index=-1;
			Players[player_num].flags &= (~flagval);
			switch (flagval) {
				case PLAYER_FLAGS_INVULNERABLE:
					powerup_index = POW_INVULNERABILITY;
					thief_message("Invulnerability stolen!");
					break;
				case PLAYER_FLAGS_CLOAKED:
					powerup_index = POW_CLOAK;
					thief_message("Cloak stolen!");
					break;
				case PLAYER_FLAGS_MAP_ALL:
					powerup_index = POW_FULL_MAP;
					thief_message("Full map stolen!");
					break;
				case PLAYER_FLAGS_QUAD_LASERS:
					powerup_index = POW_QUAD_FIRE;
					thief_message("Quad lasers stolen!");
					break;
				case PLAYER_FLAGS_AFTERBURNER:
					powerup_index = POW_AFTERBURNER;
					thief_message("Afterburner stolen!");
					break;
// --				case PLAYER_FLAGS_AMMO_RACK:
// --					powerup_index = POW_AMMO_RACK;
// --					thief_message("Ammo Rack stolen!");
// --					break;
				case PLAYER_FLAGS_CONVERTER:
					powerup_index = POW_CONVERTER;
					thief_message("Converter stolen!");
					break;
				case PLAYER_FLAGS_HEADLIGHT:
					powerup_index = POW_HEADLIGHT;
					thief_message("Headlight stolen!");
				   Players[Player_num].flags &= ~PLAYER_FLAGS_HEADLIGHT_ON;
					break;
			}
			Assert(powerup_index != -1);
			Stolen_items[Stolen_item_index] = powerup_index;

			digi_play_sample_once(SOUND_WEAPON_STOLEN, F1_0);
			return 1;
		}
	}

	return 0;
}

//	----------------------------------------------------------------------------
int maybe_steal_secondary_weapon(int player_num, int weapon_num)
{
	if ((Players[player_num].secondary_weapon_flags & HAS_FLAG(weapon_num)) && Players[player_num].secondary_ammo[weapon_num])
		if (d_rand() < THIEF_PROBABILITY) {
			if (weapon_num == PROXIMITY_INDEX)
				if (d_rand() > 8192)		//	Come in groups of 4, only add 1/4 of time.
					return 0;
			Players[player_num].secondary_ammo[weapon_num]--;

			//	Smart mines and proxbombs don't get dropped because they only come in 4 packs.
			if ((weapon_num != PROXIMITY_INDEX) && (weapon_num != SMART_MINE_INDEX)) {
				Stolen_items[Stolen_item_index] = Secondary_weapon_to_powerup[weapon_num];
			}

			thief_message("%s stolen!", SECONDARY_WEAPON_NAMES(weapon_num));		//	Danger! Danger! Use of literal!  Danger!
			if (Players[Player_num].secondary_ammo[weapon_num] == 0)
				auto_select_weapon(1);

			// -- compress_stolen_items();
			digi_play_sample_once(SOUND_WEAPON_STOLEN, F1_0);
			return 1;
		}

	return 0;
}

//	----------------------------------------------------------------------------
int maybe_steal_primary_weapon(int player_num, int weapon_num)
{
	if ((Players[player_num].primary_weapon_flags & HAS_FLAG(weapon_num)) && Players[player_num].primary_ammo[weapon_num]) {
		if (d_rand() < THIEF_PROBABILITY) {
			if (weapon_num == 0) {
				if (Players[player_num].laser_level > 0) {
					if (Players[player_num].laser_level > 3) {
						Stolen_items[Stolen_item_index] = POW_SUPER_LASER;
					} else {
						Stolen_items[Stolen_item_index] = Primary_weapon_to_powerup[weapon_num];
					}
					thief_message("%s level decreased!", PRIMARY_WEAPON_NAMES(weapon_num));		//	Danger! Danger! Use of literal!  Danger!
					Players[player_num].laser_level--;
					digi_play_sample_once(SOUND_WEAPON_STOLEN, F1_0);
					return 1;
				}
			} else if (Players[player_num].primary_weapon_flags & (1 << weapon_num)) {
				Players[player_num].primary_weapon_flags &= ~(1 << weapon_num);
				Stolen_items[Stolen_item_index] = Primary_weapon_to_powerup[weapon_num];

				thief_message("%s stolen!", PRIMARY_WEAPON_NAMES(weapon_num));		//	Danger! Danger! Use of literal!  Danger!
				auto_select_weapon(0);
				digi_play_sample_once(SOUND_WEAPON_STOLEN, F1_0);
				return 1;
			}
		}
	}

	return 0;
}



//	----------------------------------------------------------------------------
//	Called for a thief-type robot.
//	If a item successfully stolen, returns true, else returns false.
//	If a wapon successfully stolen, do everything, removing it from player,
//	updating Stolen_items information, deselecting, etc.
int attempt_to_steal_item_3(object *objp, int player_num)
{
	int	i;

	if (Ai_local_info[objp-Objects].mode != AIM_THIEF_ATTACK)
		return 0;

	//	First, try to steal equipped items.

	if (maybe_steal_flag_item(player_num, PLAYER_FLAGS_INVULNERABLE))
		return 1;

	//	If primary weapon = laser, first try to rip away those nasty quad lasers!
	if (Players[Player_num].primary_weapon == 0)
		if (maybe_steal_flag_item(player_num, PLAYER_FLAGS_QUAD_LASERS))
			return 1;

	//	Makes it more likely to steal primary than secondary.
	for (i=0; i<2; i++)
		if (maybe_steal_primary_weapon(player_num, Players[Player_num].primary_weapon))
			return 1;

	if (maybe_steal_secondary_weapon(player_num, Players[Player_num].secondary_weapon))
		return 1;

	//	See what the player has and try to snag something.
	//	Try best things first.
	if (maybe_steal_flag_item(player_num, PLAYER_FLAGS_INVULNERABLE))
		return 1;
	if (maybe_steal_flag_item(player_num, PLAYER_FLAGS_CLOAKED))
		return 1;
	if (maybe_steal_flag_item(player_num, PLAYER_FLAGS_QUAD_LASERS))
		return 1;
	if (maybe_steal_flag_item(player_num, PLAYER_FLAGS_AFTERBURNER))
		return 1;
	if (maybe_steal_flag_item(player_num, PLAYER_FLAGS_CONVERTER))
		return 1;
// --	if (maybe_steal_flag_item(player_num, PLAYER_FLAGS_AMMO_RACK))	//	Can't steal because what if have too many items, say 15 homing missiles?
// --		return 1;
	if (maybe_steal_flag_item(player_num, PLAYER_FLAGS_HEADLIGHT))
		return 1;
	if (maybe_steal_flag_item(player_num, PLAYER_FLAGS_MAP_ALL))
		return 1;

	for (i=MAX_SECONDARY_WEAPONS-1; i>=0; i--) {
		if (maybe_steal_primary_weapon(player_num, i))
			return 1;
		if (maybe_steal_secondary_weapon(player_num, i))
			return 1;
	}

	return 0;
}

//	----------------------------------------------------------------------------
int attempt_to_steal_item_2(object *objp, int player_num)
{
	int	rval;

	rval = attempt_to_steal_item_3(objp, player_num);

	if (rval) {
		Stolen_item_index = (Stolen_item_index+1) % MAX_STOLEN_ITEMS;
		if (d_rand() > 20000)	//	Occasionally, boost the value again
			Stolen_item_index = (Stolen_item_index+1) % MAX_STOLEN_ITEMS;
	}

	return rval;
}

//	----------------------------------------------------------------------------
//	Called for a thief-type robot.
//	If a item successfully stolen, returns true, else returns false.
//	If a wapon successfully stolen, do everything, removing it from player,
//	updating Stolen_items information, deselecting, etc.
int attempt_to_steal_item(object *objp, int player_num)
{
	int	i;
	int	rval = 0;

	if (objp->ctype.ai_info.dying_start_time)
		return 0;

	rval += attempt_to_steal_item_2(objp, player_num);

	for (i=0; i<3; i++) {
		if (!rval || (d_rand() < 11000)) {	//	about 1/3 of time, steal another item
			rval += attempt_to_steal_item_2(objp, player_num);
		} else
			break;
	}
	create_n_segment_path(objp, 10, ConsoleObject->segnum);
	Ai_local_info[objp-Objects].next_action_time = Thief_wait_times[Difficulty_level]/2;
	Ai_local_info[objp-Objects].mode = AIM_THIEF_RETREAT;
	if (rval) {
		PALETTE_FLASH_ADD(30, 15, -20);
		update_laser_weapon_info();
//		digi_link_sound_to_pos( SOUND_NASTY_ROBOT_HIT_1, objp->segnum, 0, &objp->pos, 0 , DEFAULT_ROBOT_SOUND_VOLUME);
//	I removed this to make the "steal sound" more obvious -AP
#ifdef NETWORK
                if (Game_mode & GM_NETWORK)
                 multi_send_stolen_items();
#endif
	}
	return rval;
}

// --------------------------------------------------------------------------------------------------------------
//	Indicate no items have been stolen.
void init_thief_for_level(void)
{
	int	i;

	for (i=0; i<MAX_STOLEN_ITEMS; i++)
		Stolen_items[i] = 255;

	Assert (MAX_STOLEN_ITEMS >= 3*2);	//	Oops!  Loop below will overwrite memory!
  
   if (!(Game_mode & GM_MULTI) || (Game_mode & GM_MULTI_COOP))
		for (i=0; i<3; i++) {
			Stolen_items[2*i] = POW_SHIELD_BOOST;
			Stolen_items[2*i+1] = POW_ENERGY;
		}

	Stolen_item_index = 0;
}

// --------------------------------------------------------------------------------------------------------------
void drop_stolen_items(object *objp)
{
	int	i;

	for (i=0; i<MAX_STOLEN_ITEMS; i++) {
		if (Stolen_items[i] != 255)
			drop_powerup(OBJ_POWERUP, Stolen_items[i], 1, &objp->mtype.phys_info.velocity, &objp->pos, objp->segnum);
		Stolen_items[i] = 255;
	}

}

// --------------------------------------------------------------------------------------------------------------
typedef struct escort_menu
{
	char	msg[300];
} escort_menu;

int escort_menu_keycommand(window *wind, d_event *event, escort_menu *menu)
{
	int	key;
	char error[256] = "";
	
	key = event_key_get(event);
	
	switch (key) {
		case KEY_0:
		case KEY_1:
		case KEY_2:
		case KEY_3:
		case KEY_4:
		case KEY_5:
		case KEY_6:
		case KEY_7:
		case KEY_8:
		case KEY_9:
			if (input_demo_recorder_is_active() &&
			    !input_demo_recorder_stage_direct_command_guidebot_goal(key, 1, error, sizeof(error)) &&
			    error[0])
				con_printf(CON_NORMAL, "Input demo recorder guidebot goal event failed: %s\n", error);
			Looking_for_marker = -1;
			Last_buddy_key = -1;
			set_escort_special_goal(key);
			Last_buddy_key = -1;
			window_close(wind);
			return 1;
			
		case KEY_ESC:
		case KEY_ENTER:
			window_close(wind);
			return 1;
			
		case KEY_T: {
			char	msg[32];
			int	temp;
			
			temp = !Buddy_messages_suppressed;
			
			if (temp)
				strcpy(msg, "suppressed");
			else
				strcpy(msg, "enabled");
			
			Buddy_messages_suppressed = 1;
			buddy_message("Messages %s.", msg);
			
			Buddy_messages_suppressed = temp;
			
			window_close(wind);
			return 1;
		}
			
		default:
			break;
	}
	
	return 0;
}

int escort_menu_handler(window *wind, d_event *event, escort_menu *menu)
{
	switch (event->type)
	{
		case EVENT_WINDOW_ACTIVATED:
			game_flush_inputs();
			break;
			
		case EVENT_KEY_COMMAND:
			return escort_menu_keycommand(wind, event, menu);
			
		case EVENT_IDLE:
			timer_delay2(50);
			break;
			
		case EVENT_WINDOW_DRAW:
			show_escort_menu(menu->msg);		//TXT_PAUSE);
			break;
			
		case EVENT_WINDOW_CLOSE:
			return 0;	// continue closing
			break;
			
		default:
			return 0;
			break;
	}

	return 1;
}

void do_escort_menu(void)
{
	int	i;
	int	next_goal;
	char	goal_str[32], tstr[32];
	escort_menu *menu;
	window *wind;

	if (Game_mode & GM_MULTI) {
#ifdef NETWORK
		if (!(Game_mode & GM_MULTI_COOP)) {
			HUD_init_message_literal(HM_DEFAULT, "No Guide-Bot in Multiplayer!");
			return;
		}
		if (Escort_owner_player != Player_num) {
			HUD_init_message_literal(HM_DEFAULT, "Guide-Bot is controlled by another player");
			return;
		}
#else
		HUD_init_message_literal(HM_DEFAULT, "No Guide-Bot in Multiplayer!");
		return;
#endif
	}

	for (i=0; i<=Highest_object_index; i++) {
		if (Objects[i].type == OBJ_ROBOT)
			if (Robot_info[Objects[i].id].companion)
				break;
	}

	if (i > Highest_object_index) {

		HUD_init_message_literal(HM_DEFAULT, "No Guide-Bot present in mine!");

		#if 0	//ndef NDEBUG	// Just use HELPVISHNU!!
		//	If no buddy bot, create one!
		HUD_init_message(HM_DEFAULT, "Debug Version: Creating Guide-Bot!");
		create_buddy_bot();
		#else
		return;
		#endif
	}

	ok_for_buddy_to_talk();	//	Needed here or we might not know buddy can talk when he can.

	if (!Buddy_allowed_to_talk) {
		HUD_init_message(HM_DEFAULT, "%s has not been released",PlayerCfg.GuidebotName);
		return;
	}

	MALLOC(menu, escort_menu, 1);
	if (!menu)
		return;
	
	// Just make it the full screen size and let show_escort_menu figure it out
	wind = window_create(&grd_curscreen->sc_canvas, 0, 0, SWIDTH, SHEIGHT, (int (*)(window *, d_event *, void *))escort_menu_handler, menu);
	if (!wind)
	{
		d_free(menu);
		return;
	}
	
	//	This prevents the buddy from coming back if you've told him to scram.
	//	If we don't set next_goal, we get garbage there.
	if (Escort_special_goal == ESCORT_GOAL_SCRAM) {
		Escort_special_goal = -1;	//	Else setting next goal might fail.
		next_goal = escort_set_goal_object();
		Escort_special_goal = ESCORT_GOAL_SCRAM;
	} else {
		Escort_special_goal = -1;	//	Else setting next goal might fail.
		next_goal = escort_set_goal_object();
	}

	switch (next_goal) {
	#ifndef NDEBUG
		case ESCORT_GOAL_UNSPECIFIED:
			Int3();
			sprintf(goal_str, "ERROR");
			break;
	#endif
			
		case ESCORT_GOAL_BLUE_KEY:
			sprintf(goal_str, "blue key");
			break;
		case ESCORT_GOAL_GOLD_KEY:
			sprintf(goal_str, "yellow key");
			break;
		case ESCORT_GOAL_RED_KEY:
			sprintf(goal_str, "red key");
			break;
		case ESCORT_GOAL_CONTROLCEN:
			sprintf(goal_str, "reactor");
			break;
		case ESCORT_GOAL_BOSS:
			sprintf(goal_str, "boss");
			break;
		case ESCORT_GOAL_EXIT:
			sprintf(goal_str, "exit");
			break;
		case ESCORT_GOAL_MARKER1:
		case ESCORT_GOAL_MARKER2:
		case ESCORT_GOAL_MARKER3:
		case ESCORT_GOAL_MARKER4:
		case ESCORT_GOAL_MARKER5:
		case ESCORT_GOAL_MARKER6:
		case ESCORT_GOAL_MARKER7:
		case ESCORT_GOAL_MARKER8:
		case ESCORT_GOAL_MARKER9:
			sprintf(goal_str, "marker %i", next_goal-ESCORT_GOAL_MARKER1+1);
			break;

	}
			
	if (!Buddy_messages_suppressed)
		sprintf(tstr, "Suppress");
	else
		sprintf(tstr, "Enable");

	sprintf(menu->msg,	"Select Guide-Bot Command:\n\n\n"
						"0.  Next Goal: %s" CC_LSPACING_S "3\n\n"
						"\x84.  Find Energy Powerup" CC_LSPACING_S "3\n\n"
						"2.  Find Energy Center" CC_LSPACING_S "3\n\n"
						"3.  Find Shield Powerup" CC_LSPACING_S "3\n\n"
						"4.  Find Any Powerup" CC_LSPACING_S "3\n\n"
						"5.  Find a Robot" CC_LSPACING_S "3\n\n"
						"6.  Find a Hostage" CC_LSPACING_S "3\n\n"
						"7.  Stay Away From Me" CC_LSPACING_S "3\n\n"
						"8.  Find My Powerups" CC_LSPACING_S "3\n\n"
						"9.  Find the exit\n\n"
						"T.  %s Messages"
						// -- "9.	Find the exit" CC_LSPACING_S "3\n"
				, goal_str, tstr);
}

//	-------------------------------------------------------------------------------
//	Show the Buddy menu!
void show_escort_menu(char *msg)
{	
	int	w,h,aw;
	int	x,y;


	gr_set_current_canvas(NULL);

	gr_set_curfont( GAME_FONT );

	gr_get_string_size(msg,&w,&h,&aw);

	x = (SWIDTH-w)/2;
	y = (SHEIGHT-h)/2;

	gr_set_fontcolor( BM_XRGB(0, 28, 0), -1 );

	nm_draw_background(x-BORDERX,y-BORDERY,x+w+BORDERX,y+h+BORDERY);

	gr_ustring( x, y, msg );

	reset_cockpit();
}

#ifdef NETWORK
// android port: multiplayer coop guidebot support
void multi_send_escort_owner(int owner_pnum)
{
	multibuf[0] = MULTI_ESCORT_OWNER;
	multibuf[1] = (ubyte)Player_num;
	multibuf[2] = (ubyte)owner_pnum;
	multi_send_data(multibuf, 3, 2);
}

void multi_do_escort_owner(const ubyte *buf)
{
	int new_owner = (int)buf[2];
	if (new_owner < 0 || new_owner >= MAX_PLAYERS)
		return;
	ESCORT_DIAG("rx escort_owner: new_owner=%d (was %d)", new_owner, Escort_owner_player);
	Escort_owner_player = new_owner;
	if (Buddy_objnum >= 0 && Buddy_objnum <= Highest_object_index)
		Objects[Buddy_objnum].ctype.ai_info.REMOTE_OWNER = (sbyte)new_owner;
	if (new_owner == Player_num)
		HUD_init_message_literal(HM_DEFAULT, "Guide-Bot: you have control");
	else
		HUD_init_message(HM_DEFAULT, "Guide-Bot: %s has control", Players[new_owner].callsign);
}

void escort_transfer_ownership_on_disconnect(int gone_pnum)
{
	if (Escort_owner_player != gone_pnum)
		return;
	if (Buddy_objnum < 0)
		return;

	// Pick lowest-numbered connected player as new owner
	int new_owner = -1;
	for (int i = 0; i < N_players; i++) {
		if (i == gone_pnum)
			continue;
		if (Players[i].connected == CONNECT_PLAYING) {
			new_owner = i;
			break;
		}
	}

	ESCORT_DIAG("transfer_ownership: gone=%d new_owner=%d", gone_pnum, new_owner);
	Escort_owner_player = new_owner;
	if (Buddy_objnum >= 0 && Buddy_objnum <= Highest_object_index)
		Objects[Buddy_objnum].ctype.ai_info.REMOTE_OWNER = (new_owner >= 0) ? (sbyte)new_owner : -1;
	if (new_owner >= 0)
		multi_send_escort_owner(new_owner);
}

void escort_release_control(void)
{
	int candidates[MAX_PLAYERS];
	int n = 0;
	int i, new_owner;

	if (Escort_owner_player != Player_num)
		return;
	if (!(Game_mode & GM_MULTI_COOP))
		return;

	for (i = 0; i < N_players; i++) {
		if (i != Player_num && Players[i].connected == CONNECT_PLAYING)
			candidates[n++] = i;
	}
	if (n == 0)
		return;

	new_owner = candidates[d_rand() % n];
	Escort_owner_player = new_owner;
	if (Buddy_objnum >= 0 && Buddy_objnum <= Highest_object_index)
		Objects[Buddy_objnum].ctype.ai_info.REMOTE_OWNER = (sbyte)new_owner;
	multi_send_escort_owner(new_owner);
	HUD_init_message_literal(HM_DEFAULT, "Guide-Bot control released");
}
#endif
