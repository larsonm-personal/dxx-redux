#ifdef ANDROID
#include "android_pause.h"
#include "android_graphics_safety.h"
#include "android_lifecycle_diagnostics.h"
#include "android_log.h"
#include "coop/coop_briefing.h"
#include "coop/coop_endgame.h"
#include "coop/coop_travel.h"
#include "coop/coop_save.h"
#include "event.h"
#include "game.h"
#include "inferno.h"
#include "key.h"
#include "kconfig.h"
#include "multi.h"
#include "newdemo.h"
#include "endlevel.h"
#include "screens.h"
#include "window.h"
#include <pthread.h>
#include <string.h>

extern int pause_handler(window *, d_event *, char *);
extern int (*window_get_callback(window *))(window *, d_event *, void *);
extern int HandleSystemKey(int key);

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static struct android_pause_snapshot published;
static uint64_t owner_serial, owner, session, ui_revision, request_serial, last_request, repairs;
static unsigned ui_modals, engine_ui_modals, debug_modals;
static uint64_t engine_ui_owner, engine_ui_revision;
static int dispatching;
static int last_result, was_blocked;
struct pause_request {
	uint64_t owner, session, id, ui_revision;
	int action, engine_origin;
};
static struct pause_request requests[32];
static unsigned queue_head, queue_count;

static int user_pause_front(void)
{
	window *front = window_get_front();
	return front && front != Game_wind &&
	       window_get_callback(front) == (int (*)(window *, d_event *, void *)) pause_handler;
}

uint64_t android_pause_attach_ui(void)
{
	pthread_mutex_lock(&lock);
	owner = ++owner_serial;
	ui_revision = 0;
	ui_modals = 0;
	uint64_t result = owner;
	pthread_mutex_unlock(&lock);
	return result;
}

void android_pause_detach_ui(uint64_t id)
{
	pthread_mutex_lock(&lock);
	if (id == owner) {
		owner = 0;
		ui_modals = 0;
		ui_revision = 0;
	}
	pthread_mutex_unlock(&lock);
}

int android_pause_publish_ui(uint64_t id, uint64_t epoch, uint64_t revision, unsigned modals)
{
	pthread_mutex_lock(&lock);
	int accepted = id && id == owner && epoch == session && (revision > ui_revision || (revision == ui_revision && (modals & 7u) == ui_modals));
	if (accepted) {
		ui_modals = modals & 7u;
		ui_revision = revision;
	}
	pthread_mutex_unlock(&lock);
	return accepted;
}

uint64_t android_pause_request(uint64_t id, uint64_t epoch, uint64_t revision, unsigned modals, int action)
{
	pthread_mutex_lock(&lock);
	uint64_t result = 0;
	if (id && id == owner && epoch == session && queue_count < 32 &&
	    action >= ANDROID_PAUSE_RESUME && action <= ANDROID_PAUSE_QUICK_LOAD && revision > ui_revision) {
		ui_revision = revision;
		ui_modals = modals & 7u;
		result = ++request_serial;
		requests[(queue_head + queue_count++) % 32] = (struct pause_request) { id, epoch, result, revision, action, 0 };
	}
	pthread_mutex_unlock(&lock);
	return result;
}

void android_pause_get_snapshot(struct android_pause_snapshot *out)
{
	pthread_mutex_lock(&lock);
	*out = published;
	pthread_mutex_unlock(&lock);
}

int android_pause_reasons(void)
{
	int reasons = android_pause_legacy_depth() ? ANDROID_PAUSE_OPERATION : 0;
	if (!Game_wind) return reasons;
	if (engine_ui_modals || debug_modals) reasons |= ANDROID_PAUSE_UI;
	if (window_get_front() != Game_wind)
		reasons |= user_pause_front() ? ANDROID_PAUSE_USER : ANDROID_PAUSE_MENU;
	if (android_graphics_safety_blocks_simulation()) reasons |= ANDROID_PAUSE_GRAPHICS;
	if (android_lifecycle_diagnostics_requested_visibility() == ANDROID_LIFECYCLE_VISIBILITY_BACKGROUND)
		reasons |= ANDROID_PAUSE_BACKGROUND;
	if (coop_briefing_running() || coop_endgame_active() || coop_travel_blocks_gameplay() || multi_save_transfer_paused())
		reasons |= ANDROID_PAUSE_COOP;
	return reasons;
}

int android_pause_simulation_paused(void)
{
	int reasons = android_pause_reasons();
	if ((Game_mode & GM_MULTI) && Newdemo_state != ND_STATE_PLAYBACK) {
		reasons &= ANDROID_PAUSE_OPERATION | ANDROID_PAUSE_COOP |
		           (Endlevel_sequence ? ANDROID_PAUSE_MENU : 0);
	}
	return reasons != 0;
}

int android_pause_input_allowed(void)
{
	return Game_wind && window_get_front() == Game_wind && Screen_mode == SCREEN_GAME &&
	       !engine_ui_modals && !debug_modals && !android_pause_reasons();
}

void android_pause_publish(void)
{
	struct android_pause_snapshot next = { 0 };
	next.reasons = android_pause_reasons();
	next.has_game = Game_wind != NULL;
	next.game_front = Game_wind && window_get_front() == Game_wind && Screen_mode == SCREEN_GAME;
	next.simulation_paused = next.has_game && android_pause_simulation_paused();
	next.input_allowed = android_pause_input_allowed();
	next.can_resume = next.has_game &&
	                  (next.reasons & (ANDROID_PAUSE_UI | ANDROID_PAUSE_USER)) &&
	                  !(next.reasons & ~(ANDROID_PAUSE_UI | ANDROID_PAUSE_USER));
	next.legacy_depth = android_pause_legacy_depth();
	if (!was_blocked && !next.input_allowed) {
		/* Co-op keeps simulating behind menus, while ReadControls ignores releases
		 * Clear the ship's controls without flushing events needed by the menu */
		debug_log_force(DLOG_GAME, "Android pause input blocked: reasons=%d fire=%d heading=%d",
		                next.reasons, Controls.fire_primary_state, Controls.heading_time);
		memset(&Controls, 0, sizeof(Controls));
	}
	if (was_blocked && next.input_allowed) {
		reset_time();
		game_flush_inputs();
	}
	was_blocked = !next.input_allowed;
	pthread_mutex_lock(&lock);
	next.session = session;
	next.ui_owner = engine_ui_owner;
	next.ui_revision = engine_ui_revision;
	next.request = last_request;
	next.result = last_result;
	next.repairs = repairs;
	next.revision = published.revision;
	if (memcmp(&next, &published, sizeof(next))) ++next.revision;
	published = next;
	pthread_mutex_unlock(&lock);
}

void android_pause_new_session(void)
{
	pthread_mutex_lock(&lock);
	++session;
	ui_modals = 0;
	ui_revision = 0;
	pthread_mutex_unlock(&lock);
	engine_ui_modals = debug_modals = 0;
	engine_ui_revision = 0;
	android_pause_publish();
}

/* Only inferno's outer loop calls this: synchronous save/load/menu stacks have
 * returned (or unwound through LeaveEvents), so no legacy pause can be live */
void android_pause_outer_tick(void)
{
	if (android_pause_legacy_depth()) {
		debug_log_force(DLOG_GAME, "Android pause recovery: orphan legacy depth=%d session=%llu",
		                android_pause_legacy_depth(), (unsigned long long) session);
		android_pause_clear_legacy();
		++repairs;
	}
	dispatching = 0; // Also repairs a native action unwound through LeaveEvents
	android_pause_tick();
}

void android_pause_tick(void)
{
	struct pause_request request = { 0 };
	pthread_mutex_lock(&lock);
	engine_ui_modals = ui_modals;
	engine_ui_owner = owner;
	engine_ui_revision = ui_revision;
	if (queue_count && !dispatching) {
		request = requests[queue_head];
		queue_head = (queue_head + 1) % 32;
		--queue_count;
		last_request = request.id;
		last_result = (request.engine_origin || (request.owner == owner && request.ui_revision == ui_revision)) && request.session == session ? ANDROID_PAUSE_EXECUTING : ANDROID_PAUSE_STALE;
	}
	int execute = request.id && last_result == ANDROID_PAUSE_EXECUTING;
	pthread_mutex_unlock(&lock);
	android_pause_publish();
	if (!execute) return;
	debug_log_force(DLOG_GAME, "Android pause action: id=%llu session=%llu action=%d reasons=%d front=%d",
	                (unsigned long long) request.id, (unsigned long long) request.session, request.action,
	                android_pause_reasons(), window_get_front() == Game_wind);
	dispatching = 1;
	int result = ANDROID_PAUSE_REJECTED;
	int blockers = android_pause_reasons() & (ANDROID_PAUSE_GRAPHICS | ANDROID_PAUSE_BACKGROUND | ANDROID_PAUSE_COOP | ANDROID_PAUSE_OPERATION);
	if (Game_wind && !blockers) {
		if (request.action == ANDROID_PAUSE_RESUME && (window_get_front() == Game_wind || user_pause_front())) {
			/* Kotlin dismisses its resumable UI before sending Resume */
			debug_modals = 0;
			if (user_pause_front()) window_close(window_get_front());
			result = ANDROID_PAUSE_APPLIED;
		} else if (request.action != ANDROID_PAUSE_RESUME && (window_get_front() == Game_wind || user_pause_front())) {
			if (user_pause_front()) window_close(window_get_front());
			debug_modals = 0;
			/* Acknowledge dispatch before entering a nested menu or a load that longjmps */
			pthread_mutex_lock(&lock);
			last_result = ANDROID_PAUSE_APPLIED;
			pthread_mutex_unlock(&lock);
			android_pause_publish();
			result = ANDROID_PAUSE_APPLIED;
			if (request.action == ANDROID_PAUSE_QUICK_LOAD) {
				extern volatile int android_quick_load_pending;
				extern int android_handle_ingame_saveload_request(void);
				android_quick_load_pending = 1;
				android_handle_ingame_saveload_request();
			} else {
				int key = request.action == ANDROID_PAUSE_OPEN_MENU ? KEY_ESC : KEY_ALTED | (request.action == ANDROID_PAUSE_OPEN_SAVE ? KEY_F2 : KEY_F3);
				result = ANDROID_PAUSE_APPLIED;
				/* Native menus may pump nested events or close the game via longjmp */
				HandleSystemKey(key);
			}
		}
	}
	debug_log_force(DLOG_GAME, "Android pause action returned: id=%llu result=%d reasons=%d front=%d",
	                (unsigned long long) request.id, result, android_pause_reasons(), window_get_front() == Game_wind);
	pthread_mutex_lock(&lock);
	if (last_request == request.id) last_result = result;
	dispatching = 0;
	pthread_mutex_unlock(&lock);
	android_pause_publish();
}

int android_pause_queue_action(int action)
{
	if (!Game_wind) return 0;
	pthread_mutex_lock(&lock);
	if (queue_count == 32) {
		pthread_mutex_unlock(&lock);
		return 0;
	}
	uint64_t id = ++request_serial;
	requests[(queue_head + queue_count++) % 32] = (struct pause_request) { 0, session, id, 0, action, 1 };
	pthread_mutex_unlock(&lock);
	return 1;
}

int android_pause_debug_overlay(void)
{
	if (!Game_wind || (Game_mode & GM_MULTI) || window_get_front() != Game_wind) return 0;
	debug_modals = ANDROID_PAUSE_TRAY;
	android_pause_publish();
	return 1;
}
#endif
