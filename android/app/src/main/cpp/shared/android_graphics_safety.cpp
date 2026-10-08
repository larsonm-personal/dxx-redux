#ifdef ANDROID
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <mutex>
#include <string>
#include <time.h>
#include <unistd.h>
#include <nlohmann/json.hpp>
#include <EGL/egl.h>

extern "C" {
#include "android_graphics_safety.h"
#include "graphics_safety_store.h"
#include "graphics_config_transaction.h"
#include "android_gpu_capabilities.h"
#include "android_graphics_options.h"
#include "android_jni_overlay.h"
#include "android_log.h"
#include "android_surface_lifecycle.h"
#include "config.h"
#include "game.h"
#include "gr.h"
#include "inferno.h"
#include "window.h"
#include "event.h"
#include "screens.h"
#include "segment.h"
#include "automap.h"
#include "newmenu.h"
#include "ogl_init.h"
#include "newdemo.h"
#include "args.h"
#include "gameseq.h"
#include "input_demo_replay.h"
}

namespace
{
using json = nlohmann::json;
// Allow a busy Android compositor to draw before starting the separate confirmation deadline
constexpr uint64_t overlay_prepare_timeout_ms = 5000;
// Wait for all queued graphics options to stay unchanged before showing confirmation
constexpr uint64_t option_debounce_ms = 2500;
std::mutex mutex;
std::string files_root;
bool initialized = false;
std::string storage_failure;
json renderer_failure;
bool failure_notified = false;
bool ui_foreground = true;
bool ui_blocked = false;
bool applying = false; // Game-thread only; suppress recursive trial creation during restore
bool restore_pending = false;
bool accepted_pending = false;
bool preparing = false;
bool armed = false;
bool candidate_ready = false;
enum preview_phase { preview_none,
	                 preview_offering,
	                 preview_editing,
	                 preview_live,
	                 preview_settling };
preview_phase preview = preview_none;
std::string first_run_marker;
bool first_run_pending = false;
bool first_run_trial = false;
json preview_capabilities;
uint64_t candidate_revision = 0, applied_revision = 0;
uint64_t preview_apply_deadline = 0;
uint64_t preview_presented_frames = 0;
// Game-thread frame tag; a menu/failed/recreated-surface swap cannot consume a gameplay frame
bool main_view_rendered = false;
bool main_view_skipped = false;
uint64_t incomplete_presentations = 0;
uint64_t withheld_presentations = 0;
uint64_t rendered_generation = 0, rendered_revision = 0;
EGLContext rendered_context = EGL_NO_CONTEXT;
#ifdef INTROSPECT_ON
int debug_stall_ms;
bool debug_black_enabled, debug_black_started;
unsigned debug_black_frames, debug_black_valid_frames;
#endif
bool queued[5] = {};
int queued_values[5] = {};
bool queued_persist = false;
uint64_t quiet_until = 0;
uint64_t prepared_at = 0;
uint64_t active_id = 0;
uint64_t deadline = 0;
uint64_t serial = 0;
uint64_t staged_poll_at = 0;
uint64_t staged_generation = 0;
bool current_known = false;
graphics_safety_snapshot observed_current;
graphics_safety_snapshot pending_candidate;
thread_local graphics_safety_snapshot config_write_snapshot;
thread_local bool config_write_has_snapshot = false;

uint64_t now_ms()
{
	struct timespec time;
	clock_gettime(CLOCK_BOOTTIME, &time);
	return static_cast<uint64_t>(time.tv_sec) * 1000 + static_cast<uint64_t>(time.tv_nsec) / 1000000;
}

const char *variant()
{
#ifdef DXX_BUILD_DESCENT_II
	return "d2";
#else
	return "d1";
#endif
}

int field(const char *name)
{
	static const char *const names[] = { "tex_filt", "aniso_level", "msaa_level", "menu_tex_filt", "hud_tex_filt" };
	if (name)
		for (int i = 0; i < 5; ++i)
			if (!std::strcmp(name, names[i])) return i;
	return -1;
}

graphics_safety_snapshot live(int width = 0, int height = 0)
{
	graphics_safety_snapshot snapshot = { { GameCfg.TexFilt, GameCfg.AnisoLevel, GameCfg.MsaaLevel, GameCfg.MenuTexFilt, GameCfg.HudTexFilt,
		                                    width > 0 ? width : static_cast<int>(SM_W(Game_screen_mode)),
		                                    height > 0 ? height : static_cast<int>(SM_H(Game_screen_mode)),
		                                    GameCfg.AspectX, GameCfg.AspectY, GameCfg.ColorDepth } };
	return snapshot;
}

bool eligible()
{
	if (!Game_wind || Screen_mode != SCREEN_GAME || window_get_front() != Game_wind || Automap_active)
		return false;
	android_surface_snapshot surface;
	android_surface_acquire_snapshot(&surface);
	const bool available = surface.window && !surface.paused;
	android_surface_release_snapshot(&surface);
	return available;
}

json snapshot_json(const graphics_safety_snapshot &snapshot)
{
	json value = json::object();
	for (int i = 0; i < GRAPHICS_SAFE_FIELD_COUNT; ++i) value[graphics_safety_keys[i]] = snapshot.values[i];
	return value;
}

std::string state_json_locked()
{
	if (!storage_failure.empty()) return json({ { "phase", "failed" }, { "reason", storage_failure } }).dump();
	graphics_safety_record record;
	if (!initialized)
		return "{\"phase\":\"disabled\"}";
	if (!graphics_safety_read_record(files_root.c_str(), &record)) {
		storage_failure = "record_read_failed";
		return "{\"phase\":\"failed\",\"reason\":\"record_read_failed\"}";
	}
	graphics_safety_snapshot requested;
	if (!graphics_safety_read_staged(files_root.c_str(), variant(), &requested)) {
		storage_failure = "requested_read_failed";
		return "{\"phase\":\"failed\",\"reason\":\"requested_read_failed\"}";
	}
	json queued_options = json::object();
	for (int i = 0; i < 5; ++i)
		if (queued[i]) queued_options[graphics_safety_keys[i]] = queued_values[i];
	const char *phase = restore_pending ? "restoring" : preparing                 ? "preparing"
	                                                : armed                       ? "challenge"
	                                                : preview == preview_offering ? "offering"
	                                                : preview == preview_editing  ? "editing"
	                                                : preview == preview_live     ? "live"
	                                                : preview == preview_settling ? "settling"
	                                                                              : "idle";
	json state = { { "phase", phase }, { "trial_id", active_id }, { "deadline_ms", deadline }, { "accepted", snapshot_json(record.accepted) }, { "candidate", snapshot_json(pending_candidate) }, { "requested", snapshot_json(requested) }, { "current", current_known ? snapshot_json(observed_current) : json(nullptr) }, { "queued", queued_options }, { "staged_generation", staged_generation }, { "reason", record.reason }, { "renderer_failure", renderer_failure } };
	state["first_run_pending"] = first_run_pending;
	state["first_run_trial"] = first_run_trial;
	state["candidate_ready"] = candidate_ready;
	state["preview_presented_frames"] = preview_presented_frames;
	state["incomplete_presentations"] = incomplete_presentations;
	state["withheld_presentations"] = withheld_presentations;
	if (first_run_trial) state["capabilities"] = preview_capabilities;
	if (first_run_trial || preview == preview_live) {
		state["candidate_revision"] = candidate_revision;
		state["applied_revision"] = applied_revision;
		state["apply_deadline_ms"] = preview_apply_deadline;
		state["quiet_until_ms"] = quiet_until;
	}
#ifdef INTROSPECT_ON
	state["debug_black_output"] = { { "active", debug_black_enabled && armed && candidate_ready && !restore_pending }, { "frames", debug_black_frames }, { "valid_frames", debug_black_valid_frames } };
#endif
	return state.dump();
}

void notify()
{
	std::string message;
	{
		std::lock_guard<std::mutex> lock(mutex);
		message = state_json_locked();
	}
	android_send_graphics_safety_state(message.c_str());
}

bool begin(const graphics_safety_snapshot &candidate, bool prepare)
{
	const uint64_t id = now_ms() * 1000 + (++serial % 1000);
	if (!graphics_safety_begin_attempt(files_root.c_str(), &candidate, id, getpid(), prepare)) {
		storage_failure = "attempt_persist_failed";
		return false;
	}
	active_id = id;
	pending_candidate = candidate;
	preparing = prepare;
	armed = false;
	candidate_ready = false;
	prepared_at = now_ms();
	deadline = 0;
	return true;
}

bool apply_snapshot(const graphics_safety_snapshot &snapshot, bool persist)
{
	applying = true;
	auto before = live();
	graphics_safety_normalize(&before);
	const char *const names[] = { "tex_filt", "aniso_level", "msaa_level", "menu_tex_filt", "hud_tex_filt" };
	for (int i = 0; i < 5; ++i)
		if (before.values[i] != snapshot.values[i])
			android_graphics_set_option(names[i], snapshot.values[i], 0);
	const bool mode_changed = before.values[GRAPHICS_SAFE_WIDTH] != snapshot.values[GRAPHICS_SAFE_WIDTH] ||
	                          before.values[GRAPHICS_SAFE_HEIGHT] != snapshot.values[GRAPHICS_SAFE_HEIGHT] ||
	                          before.values[GRAPHICS_SAFE_ASPECT_X] != snapshot.values[GRAPHICS_SAFE_ASPECT_X] ||
	                          before.values[GRAPHICS_SAFE_ASPECT_Y] != snapshot.values[GRAPHICS_SAFE_ASPECT_Y] ||
	                          before.values[GRAPHICS_SAFE_COLOR_DEPTH] != snapshot.values[GRAPHICS_SAFE_COLOR_DEPTH];
	bool good = true;
	// A failed mode change can mutate the canvas and destroy EGL before Game_screen_mode changes
	const bool renderer_incomplete = eglGetCurrentContext() == EGL_NO_CONTEXT ||
	                                 eglGetCurrentSurface(EGL_DRAW) == EGL_NO_SURFACE ||
	                                 (grd_curscreen && (grd_curscreen->sc_w != snapshot.values[GRAPHICS_SAFE_WIDTH] ||
	                                                    grd_curscreen->sc_h != snapshot.values[GRAPHICS_SAFE_HEIGHT]));
	if ((mode_changed || renderer_incomplete) && grd_curscreen) {
		const int width = snapshot.values[GRAPHICS_SAFE_WIDTH], height = snapshot.values[GRAPHICS_SAFE_HEIGHT];
		GameCfg.ColorDepth = snapshot.values[GRAPHICS_SAFE_COLOR_DEPTH];
		GameCfg.AspectX = snapshot.values[GRAPHICS_SAFE_ASPECT_X];
		GameCfg.AspectY = snapshot.values[GRAPHICS_SAFE_ASPECT_Y];
		GameCfg.ResolutionX = width;
		GameCfg.ResolutionY = height;
		newmenu_free_background();
		good = gr_set_mode(SM(width, height)) == 0;
		if (good) {
			Game_screen_mode = SM(width, height);
			game_init_render_buffers(width, height);
			if (Game_wind) {
				d_event event;
				WINDOW_SEND_EVENT(Game_wind, EVENT_WINDOW_ACTIVATED);
				WINDOW_SEND_EVENT(Game_wind, EVENT_WINDOW_DEACTIVATED);
			}
		}
	}
	applying = false;
	if (persist) {
		graphics_safety_snapshot requested;
		good = graphics_safety_read_requested(files_root.c_str(), variant(), &requested) && good;
		if (good) {
			// Preserve mode settings staged in the launcher for the next engine start
			for (int i = 0; i < 5; ++i) requested.values[i] = snapshot.values[i];
			good = graphics_safety_patch_snapshot(files_root.c_str(), &requested) != 0;
		}
	}
	return good;
}

} // namespace

extern "C" int android_graphics_safety_blocks_simulation(void)
{
	std::lock_guard<std::mutex> guard(mutex);
	return initialized && (preparing || armed || restore_pending || accepted_pending ||
	                       (preview != preview_none && preview != preview_live));
}

extern "C" int android_graphics_safety_initialize(const char *root)
{
	std::lock_guard<std::mutex> lock(mutex);
	if (!root || !*root || !graphics_safety_recover(root, getpid())) return 0;
	files_root = root;
	storage_failure.clear();
	failure_notified = false;
	graphics_safety_defaults(&observed_current);
	initialized = true;
	return 1;
}

extern "C" void android_graphics_safety_shutdown(void)
{
	std::lock_guard<std::mutex> lock(mutex);
	if (initialized) graphics_safety_clean_exit(files_root.c_str(), getpid());
	initialized = false;
}

extern "C" void android_graphics_safety_first_run_marker(const char *path)
{
	std::lock_guard<std::mutex> lock(mutex);
	first_run_marker = path ? path : "";
	first_run_pending = !first_run_marker.empty() && access(first_run_marker.c_str(), F_OK) != 0 && errno == ENOENT;
}

extern "C" void android_graphics_safety_main_view_rendered(void)
{
	android_surface_snapshot surface;
	android_surface_acquire_snapshot(&surface);
	main_view_rendered = surface.window && !surface.paused;
	rendered_generation = surface.generation;
	android_surface_release_snapshot(&surface);
	rendered_context = eglGetCurrentContext();
	std::lock_guard<std::mutex> lock(mutex);
	rendered_revision = applied_revision;
}

extern "C" int android_graphics_safety_allow_present(void)
{
	if (!main_view_skipped) return 1;
	main_view_skipped = false;
	main_view_rendered = false;
	std::lock_guard<std::mutex> lock(mutex);
	++withheld_presentations;
	debug_log(DLOG_GRAPHICS, "Graphics safety retained completed frame: trial=%llu count=%llu",
	          (unsigned long long) active_id, (unsigned long long) withheld_presentations);
	return 0;
}

extern "C" void android_graphics_safety_presented(int success, uint64_t generation)
{
	if (main_view_skipped && success) {
		std::lock_guard<std::mutex> lock(mutex);
		++incomplete_presentations;
		debug_log_force(DLOG_GRAPHICS, "Graphics safety presented a skipped main view: trial=%llu preparing=%d armed=%d count=%llu",
		                (unsigned long long) active_id, preparing, armed, (unsigned long long) incomplete_presentations);
	}
	main_view_skipped = false;
	const bool gameplay = main_view_rendered && success && generation == rendered_generation &&
	                      rendered_context != EGL_NO_CONTEXT && rendered_context == eglGetCurrentContext();
	main_view_rendered = false;
	if (!gameplay || !eligible()) return;
	bool offer = false;
	{
		std::lock_guard<std::mutex> lock(mutex);
		if (!initialized || !storage_failure.empty() || restore_pending) return;
		if (first_run_trial) ++preview_presented_frames;
		if ((first_run_trial || preview == preview_live) && preview != preview_offering && rendered_revision == candidate_revision) {
			candidate_ready = true;
			preview_apply_deadline = 0;
		}
		if (!first_run_pending || first_run_trial || preparing || armed || preview != preview_none ||
		    accepted_pending || !ui_foreground || ui_blocked || Game_mode & GM_MULTI ||
		    Newdemo_state == ND_STATE_PLAYBACK || input_demo_replay_is_loaded() ||
		    GameArg.SysInputDemoNoRender || Current_level_num == 0 || Player_is_dead) return;
		for (bool option : queued)
			if (option) return;
		graphics_safety_record record;
		if (!graphics_safety_read_record(files_root.c_str(), &record) || record.phase != GRAPHICS_SAFE_IDLE) return;
		char capabilities[4096];
		android_gpu_capabilities_json(capabilities, sizeof(capabilities));
		auto report = json::parse(capabilities, nullptr, false);
		if (!report.is_object() || report.value("schema", 0) != 1 ||
		    report.value("color_depth", -1) != GameCfg.ColorDepth) return;
		const auto candidate = live();
		const uint64_t id = now_ms() * 1000 + (++serial % 1000);
		if (!graphics_safety_preview(files_root.c_str(), &candidate, id, getpid(), 0)) {
			storage_failure = "preview_persist_failed";
			return;
		}
		preview_capabilities = std::move(report);
		pending_candidate = observed_current = candidate;
		current_known = true;
		active_id = id;
		first_run_trial = true;
		preview = preview_offering;
		deadline = 0;
		candidate_ready = false;
		candidate_revision = applied_revision = 0;
		preview_presented_frames = 0;
		preview_apply_deadline = now_ms() + overlay_prepare_timeout_ms;
		offer = true;
	}
	if (offer) {
		notify();
	}
}

extern "C" int android_graphics_safety_preview_ready(uint64_t id)
{
	std::lock_guard<std::mutex> lock(mutex);
	if (!initialized || id != active_id || preview != preview_offering || !ui_foreground || restore_pending) return 0;
	// Called only after the Android chooser has drawn, before any maximum-setting GL change
	if (graphics_config_atomic_replace(first_run_marker.c_str(), "offered\n", 8) != GRAPHICS_CONFIG_TRANSACTION_OK) return 0;
	first_run_pending = false;
	auto candidate = pending_candidate;
	candidate.values[GRAPHICS_SAFE_TEXFILT] = 2;
	candidate.values[GRAPHICS_SAFE_ANISO] = 0;
	// These product choices match GraphicsOptionChoices.kt and the existing config limits
	for (int value : { 2, 4, 8, 16 })
		if (value <= preview_capabilities.value("aniso_max", 1.0)) candidate.values[GRAPHICS_SAFE_ANISO] = value;
	candidate.values[GRAPHICS_SAFE_MSAA] = preview_capabilities.value("msaa_4", 0) >= 4 ? 4 : preview_capabilities.value("msaa_2", 0) >= 2 ? 2
	                                                                                                                                       : 0;
	if (!graphics_safety_preview(files_root.c_str(), &candidate, id, getpid(), 0)) return 0;
	pending_candidate = candidate;
	++candidate_revision;
	preview = preview_editing;
	preview_apply_deadline = now_ms() + overlay_prepare_timeout_ms;
	return 1;
}

extern "C" int android_graphics_safety_preview_option(uint64_t id, const char *name, int value)
{
	const int index = field(name);
	std::lock_guard<std::mutex> lock(mutex);
	if (!initialized || id != active_id || preview != preview_editing || restore_pending || index < 0 || index > 2) return 0;
	bool supported = index == GRAPHICS_SAFE_TEXFILT && value >= 0 && value <= 2;
	if (index == GRAPHICS_SAFE_ANISO)
		supported = (value == 0 || value == 2 || value == 4 || value == 8 || value == 16) &&
		            (value == 0 || value <= preview_capabilities.value("aniso_max", 1.0));
	if (index == GRAPHICS_SAFE_MSAA)
		supported = value == 0 || (value == 2 && preview_capabilities.value("msaa_2", 0) >= 2) ||
		            (value == 4 && preview_capabilities.value("msaa_4", 0) >= 4);
	if (!supported) return 0;
	auto candidate = pending_candidate;
	candidate.values[index] = value;
	if (!graphics_safety_preview(files_root.c_str(), &candidate, id, getpid(), 0)) return 0;
	pending_candidate = candidate;
	++candidate_revision;
	candidate_ready = false;
	if (!preview_apply_deadline) preview_apply_deadline = now_ms() + overlay_prepare_timeout_ms;
	quiet_until = now_ms() + option_debounce_ms;
	return 1;
}

extern "C" int android_graphics_safety_preview_done(uint64_t id)
{
	std::lock_guard<std::mutex> lock(mutex);
	if (!initialized || id != active_id || preview != preview_editing || restore_pending) return 0;
	preview = preview_settling;
	quiet_until = now_ms() + option_debounce_ms;
	return 1;
}

extern "C" int android_graphics_safety_queue_option(const char *name, int value, int persist, int debounce)
{
	const int index = field(name);
	if (index < 0) return ANDROID_GRAPHICS_OPTION_UNKNOWN;
	std::lock_guard<std::mutex> lock(mutex);
	if (!initialized || !storage_failure.empty() || preparing || armed || restore_pending ||
	    (preview != preview_none && preview != preview_live)) return ANDROID_GRAPHICS_OPTION_PERSIST_FAILED;
	graphics_safety_snapshot normalized;
	graphics_safety_defaults(&normalized);
	normalized.values[index] = value;
	if (!graphics_safety_normalize(&normalized)) return ANDROID_GRAPHICS_OPTION_PERSIST_FAILED;
	value = normalized.values[index];
	if (queued[index] && queued_values[index] == value) {
		queued_persist |= persist != 0;
		return ANDROID_GRAPHICS_OPTION_OK;
	}
	const bool unchanged = current_known && !queued[index] && observed_current.values[index] == value;
	if (unchanged && !persist) return ANDROID_GRAPHICS_OPTION_OK;
	queued[index] = true;
	queued_values[index] = value;
	queued_persist |= persist != 0;
	if (!unchanged) quiet_until = now_ms() + (debounce ? option_debounce_ms : 0);
	debug_log_force(DLOG_GRAPHICS, "Graphics live option %s=%d debounce=%d quiet_until_ms=%llu now_ms=%llu", name, value, debounce,
	                (unsigned long long) quiet_until, (unsigned long long) now_ms());
	return ANDROID_GRAPHICS_OPTION_OK;
}

extern "C" int android_graphics_safety_note_option(const char *name, int value)
{
	if (applying) return 1;
	const int index = field(name);
	if (index < 0) return 1;
	std::lock_guard<std::mutex> lock(mutex);
	if (!initialized) return 1; // Previews/metadata processes cannot validate graphics
	if (!storage_failure.empty() || preparing || armed || restore_pending || preview != preview_none) return 0;
	auto candidate = live();
	candidate.values[index] = value;
	if (!graphics_safety_normalize(&candidate)) return 0;
	if (!begin(candidate, false)) return 0;
	observed_current = candidate;
	current_known = true;
	return 1;
}

extern "C" int android_graphics_safety_before_mode(int width, int height)
{
	if (applying) return 1;
	std::lock_guard<std::mutex> lock(mutex);
	if (!initialized) return 1;
	if (!storage_failure.empty() || preparing || armed || restore_pending || preview != preview_none) return 0;
	const auto candidate = live(width, height);
	if (!begin(candidate, false)) return 0;
	const auto current = live();
	if (Game_wind && !graphics_safety_equal(&candidate, &current) &&
	    !graphics_safety_patch_snapshot(files_root.c_str(), &candidate)) {
		storage_failure = "mode_persist_failed";
		return 0;
	}
	return 1;
}

extern "C" void android_graphics_safety_ui_state(int foreground, int blocked)
{
	uint64_t cancel_id = 0;
	{
		std::lock_guard<std::mutex> lock(mutex);
		// Launcher preferences can re-arm or suppress the offer while this game is backgrounded
		if (foreground && !ui_foreground)
			first_run_pending = !first_run_marker.empty() && access(first_run_marker.c_str(), F_OK) != 0 && errno == ENOENT;
		ui_foreground = foreground != 0;
		ui_blocked = blocked != 0;
		if (!ui_foreground && (preparing || armed || preview != preview_none)) cancel_id = active_id;
	}
	if (cancel_id) android_graphics_safety_decide(cancel_id, 0, "background");
}

extern "C" int android_graphics_safety_arm(uint64_t id)
{
	std::lock_guard<std::mutex> lock(mutex);
	const auto now = now_ms();
	if (!initialized || !preparing || active_id != id || !ui_foreground || ui_blocked ||
	    !graphics_safety_arm(files_root.c_str(), id, now)) return 0;
	preparing = false;
	armed = true;
	deadline = now + 5000;
	return 1;
}

extern "C" int android_graphics_safety_decide(uint64_t id, int accept, const char *reason)
{
	int result;
	uint64_t decision_time;
	uint64_t trial_deadline;
	{
		std::lock_guard<std::mutex> lock(mutex);
		if (!initialized || id != active_id || (accept && !candidate_ready)) return 0;
		trial_deadline = deadline;
		decision_time = now_ms();
		result = graphics_safety_decide(files_root.c_str(), id, accept, decision_time, reason);
		if (result == 1) {
			accepted_pending = true;
			armed = false;
			preparing = false;
		} else if (result == 2 || result == -1) {
			restore_pending = true;
			preparing = false;
			armed = false;
			std::memset(queued, 0, sizeof(queued));
		}
		if (result != 0) {
			preview = preview_none;
			preview_apply_deadline = 0;
		}
	}
	debug_log_force(DLOG_GRAPHICS, "Graphics trial decision id=%llu accept=%d reason=%s result=%d now_ms=%llu deadline_ms=%llu",
	                (unsigned long long) id, accept, reason ? reason : "", result,
	                (unsigned long long) decision_time, (unsigned long long) trial_deadline);
	notify();
	return result;
}

extern "C" void android_graphics_safety_renderer_failed(const char *reason)
{
	uint64_t id = 0;
	std::string evidence;
	{
		std::lock_guard<std::mutex> lock(mutex);
		graphics_safety_record attempt = {};
		if (initialized && graphics_safety_read_record(files_root.c_str(), &attempt) &&
		    attempt.phase != GRAPHICS_SAFE_IDLE && attempt.trial_id == active_id) id = active_id;
		if (id && !restore_pending &&
		    (renderer_failure.is_null() || renderer_failure.value("trial_id", uint64_t(0)) != id)) {
			json state = json::parse(state_json_locked());
			state.erase("renderer_failure");
			state["failure_reason"] = reason ? reason : "renderer_failed";
			state["reported_at_ms"] = now_ms();
			state["attempt_phase"] = attempt.phase;
			renderer_failure = std::move(state);
			evidence = "Graphics renderer failure before rollback\n" + renderer_failure.dump(2);
		}
	}
	if (!evidence.empty()) debug_log_batch_force(DLOG_GRAPHICS, evidence.c_str());
	if (id) android_graphics_safety_decide(id, 0, reason);
}

extern "C" int android_graphics_safety_restoring_mode(void)
{
	if (!applying) return 0;
	std::lock_guard<std::mutex> lock(mutex);
	return restore_pending;
}

extern "C" void android_graphics_safety_event_tick(void)
{
	bool failed = false, report_failure = false;
	{
		std::lock_guard<std::mutex> lock(mutex);
		failed = initialized && !storage_failure.empty();
		if (failed && !failure_notified) {
			failure_notified = true;
			report_failure = true;
		}
	}
	if (failed) {
		if (report_failure) notify();
		return;
	}
	bool restore = false, accepted = false;
	bool poll_staged = false;
	uint64_t cancel_id = 0, restore_id = 0;
	graphics_safety_snapshot target;
	{
		std::lock_guard<std::mutex> lock(mutex);
		if (!initialized) return;
		if (now_ms() >= staged_poll_at && !preparing && !armed && !restore_pending && preview == preview_none) {
			staged_poll_at = now_ms() + 250;
			poll_staged = true;
		}
		if ((armed && (now_ms() >= deadline || !ui_foreground || !eligible())) ||
		    (preparing && now_ms() - prepared_at >= overlay_prepare_timeout_ms) ||
		    (preview != preview_none && (!ui_foreground || !eligible() ||
		                                 (preview_apply_deadline && now_ms() >= preview_apply_deadline)))) cancel_id = active_id;
		restore = restore_pending;
		accepted = accepted_pending;
		if (restore) {
			graphics_safety_record record;
			if (!graphics_safety_read_record(files_root.c_str(), &record)) {
				storage_failure = "restore_record_read_failed";
				return;
			}
			target = record.accepted;
			restore_id = active_id;
		}
		accepted_pending = false;
		if (accepted) first_run_trial = false;
	}
	const int staged_result = poll_staged ? graphics_safety_flush_staged(files_root.c_str()) : 1;
	if (!staged_result) {
		std::lock_guard<std::mutex> lock(mutex);
		storage_failure = "staged_persist_failed";
		return;
	}
	if (staged_result == 2) {
		{
			std::lock_guard<std::mutex> lock(mutex);
			++staged_generation;
		}
		notify();
	}
	if (cancel_id) {
		android_graphics_safety_decide(cancel_id, 0, "timeout_or_interruption");
		return;
	}
	if (accepted) {
		notify();
	}
	if (restore) {
		// Neither the state mutex nor the file lock may be held during GL recovery
		const bool durable = graphics_safety_decide(files_root.c_str(), restore_id, 0, now_ms(), "restore_retry") == 2;
		const bool good = durable && apply_snapshot(target, false) && graphics_safety_complete_restore(files_root.c_str(), restore_id);
		if (good) {
			{
				std::lock_guard<std::mutex> lock(mutex);
				restore_pending = false;
				first_run_trial = false;
				observed_current = target;
				current_known = true;
			}
			notify();
		}
	}
}

static int prepare_main_view(void)
{
	graphics_safety_snapshot candidate;
	bool apply = false, persist = false, show = false, trial_active = false, live_started = false;
	uint64_t revision = 0, unchanged_preview = 0;
	{
		std::lock_guard<std::mutex> lock(mutex);
		if (!initialized) return 1;
		if (!storage_failure.empty() || restore_pending || preparing) return 0;
		if (!eligible()) return 1;
		auto current = live();
		if (!graphics_safety_normalize(&current)) return 0;
		observed_current = current;
		current_known = true;
		// Record each live candidate before touching GL; debounce only the confirmation
		if ((preview == preview_none || preview == preview_live) && !armed && ui_foreground && !ui_blocked) {
			candidate = current;
			bool has_queued = false;
			for (int i = 0; i < 5; ++i)
				if (queued[i]) {
					candidate.values[i] = queued_values[i];
					has_queued = true;
				}
			if (has_queued && (preview == preview_live || !graphics_safety_equal(&candidate, &current))) {
				const uint64_t id = preview == preview_live ? active_id : now_ms() * 1000 + (++serial % 1000);
				if (!graphics_safety_preview(files_root.c_str(), &candidate, id, getpid(), 0)) {
					storage_failure = "live_preview_persist_failed";
					return 0;
				}
				live_started = preview != preview_live;
				active_id = id;
				pending_candidate = candidate;
				preview = preview_live;
				deadline = 0;
				++candidate_revision;
				candidate_ready = false;
				if (!preview_apply_deadline) preview_apply_deadline = now_ms() + overlay_prepare_timeout_ms;
				std::memset(queued, 0, sizeof(queued));
			}
		}
		trial_active = armed || preview != preview_none;
		if (preview == preview_offering) return 1;
		if (preview == preview_editing || preview == preview_settling || preview == preview_live) {
			candidate = pending_candidate;
			revision = candidate_revision;
			apply = !graphics_safety_equal(&candidate, &current);
			if ((preview == preview_settling || preview == preview_live) && candidate_ready && now_ms() >= quiet_until && !ui_blocked) {
				graphics_safety_record record;
				if (!graphics_safety_read_record(files_root.c_str(), &record)) return 0;
				if (preview == preview_live && (graphics_safety_equal(&candidate, &record.accepted) || graphics_safety_all_off(&candidate))) {
					if (!graphics_safety_finish_safe_preview(files_root.c_str(), active_id, getpid())) {
						storage_failure = "live_preview_finish_failed";
						return 0;
					}
					preview = preview_none;
					persist = queued_persist;
					queued_persist = false;
					trial_active = false;
				} else if (graphics_safety_equal(&candidate, &record.accepted)) unchanged_preview = active_id;
				else if (graphics_safety_preview(files_root.c_str(), &candidate, active_id, getpid(), 1)) {
					debug_log_force(DLOG_GRAPHICS, "Graphics preview confirmation quiet_until_ms=%llu now_ms=%llu",
					                (unsigned long long) quiet_until, (unsigned long long) now_ms());
					preview = preview_none;
					preparing = true;
					prepared_at = now_ms();
					candidate_ready = false;
					queued_persist = true;
					show = true;
				} else storage_failure = "preview_confirm_persist_failed";
			}
		} else if (armed) {
			candidate = pending_candidate;
			revision = candidate_revision;
			apply = !graphics_safety_equal(&candidate, &current);
			persist = queued_persist;
			if (!apply && !persist && !first_run_trial) candidate_ready = true;
			queued_persist = false;
		} else {
			if (!ui_foreground || ui_blocked || now_ms() < quiet_until) return 1;
			candidate = current;
			for (int i = 0; i < 5; ++i)
				if (queued[i]) candidate.values[i] = queued_values[i];
			if (!graphics_safety_normalize(&candidate)) return 0;
			graphics_safety_record record;
			if (!graphics_safety_read_record(files_root.c_str(), &record)) {
				storage_failure = "record_read_failed";
				return 0;
			}
			apply = !graphics_safety_equal(&candidate, &current);
			persist = queued_persist;
			if (!graphics_safety_equal(&candidate, &record.accepted) && !graphics_safety_all_off(&candidate)) {
				if (!begin(candidate, true)) return 0;
				show = true;
			} else {
				// Editing back or choosing all-off clears any pre-level attempt marker
				if (record.phase != GRAPHICS_SAFE_IDLE && !begin(candidate, false)) return 0;
				queued_persist = false;
			}
			std::memset(queued, 0, sizeof(queued));
		}
	}
	// Start the independent UI watchdog before a renderer call can stall
	if (live_started) notify();
	if (unchanged_preview) {
		android_graphics_safety_decide(unchanged_preview, 0, "unchanged_preview");
		return 0;
	}
	if (show) {
		notify();
		return 0;
	}
	if ((apply || persist) && !apply_snapshot(candidate, persist)) {
		android_graphics_safety_renderer_failed("apply_or_persist_failed");
		if (!trial_active) {
			std::lock_guard<std::mutex> lock(mutex);
			storage_failure = "apply_or_persist_failed";
		}
		return 0;
	}
	extern GLfloat ogl_maxanisotropy;
	if (trial_active && GameCfg.AnisoLevel > 1 && ogl_maxanisotropy <= 1.0f) {
		android_graphics_safety_renderer_failed("anisotropy_unsupported");
		return 0;
	}
	if (apply || persist || trial_active) {
		std::lock_guard<std::mutex> lock(mutex);
		observed_current = candidate;
		current_known = true;
		applied_revision = revision;
		if (trial_active && armed && !restore_pending && !first_run_trial) candidate_ready = true;
	}
#ifdef INTROSPECT_ON
	if (trial_active && debug_stall_ms) {
		const int milliseconds = debug_stall_ms;
		debug_stall_ms = 0;
		debug_log_force(DLOG_GRAPHICS, "Graphics safety fault: trial render-thread stall for %d ms", milliseconds);
		struct timespec remaining = { milliseconds / 1000, (milliseconds % 1000) * 1000000L };
		// Deliberately outside both graphics locks so the Android deadline can win
		while (nanosleep(&remaining, &remaining) && errno == EINTR) {}
	}
#endif
	return 1;
}

extern "C" int android_graphics_safety_before_main_view(void)
{
	const int render = prepare_main_view();
	main_view_skipped = !render;
	return render;
}

#ifdef INTROSPECT_ON
extern "C" void android_graphics_safety_debug_stall_once(int milliseconds)
{
	debug_stall_ms = milliseconds > 0 && milliseconds <= 15000 ? milliseconds : 0;
}

extern "C" void android_graphics_safety_debug_black_once(void)
{
	std::lock_guard<std::mutex> lock(mutex);
	debug_black_enabled = true;
	debug_black_started = false;
	debug_black_frames = debug_black_valid_frames = 0;
}

extern "C" int android_graphics_safety_debug_black_active(void)
{
	std::lock_guard<std::mutex> lock(mutex);
	if (debug_black_started && !armed) debug_black_enabled = false;
	const bool active = debug_black_enabled && armed && candidate_ready && !restore_pending;
	if (active) debug_black_started = true;
	return active;
}

extern "C" void android_graphics_safety_debug_black_result(int valid)
{
	std::lock_guard<std::mutex> lock(mutex);
	++debug_black_frames;
	if (valid) ++debug_black_valid_frames;
}
#endif

extern "C" void android_graphics_safety_state_json(char *buffer, size_t size)
{
	std::lock_guard<std::mutex> lock(mutex);
	const auto value = state_json_locked();
	std::snprintf(buffer, size, "%s", value.c_str());
}

extern "C" void *android_graphics_safety_config_write_begin(int *ready)
{
	*ready = 1;
	if (!initialized) return nullptr;
	void *lock = graphics_safety_lock(files_root.c_str());
	config_write_has_snapshot = lock && graphics_safety_read_requested(files_root.c_str(), variant(), &config_write_snapshot);
	if (!config_write_has_snapshot) {
		graphics_safety_unlock(lock);
		*ready = 0;
		std::lock_guard<std::mutex> state_lock(mutex);
		storage_failure = "config_write_snapshot_failed";
		return nullptr;
	}
	return lock;
}

extern "C" int android_graphics_safety_config_write_value(const char *key, int fallback)
{
	const int index = graphics_safety_field_for_key(key);
	return config_write_has_snapshot && index >= 0 ? config_write_snapshot.values[index] : fallback;
}

extern "C" void android_graphics_safety_config_write_end(void *lock)
{
	config_write_has_snapshot = false;
	graphics_safety_unlock(lock);
}
#endif
