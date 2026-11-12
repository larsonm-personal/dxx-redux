#include <nlohmann/json.hpp>
#include "slow_motion_save.h"
#include "save_epoch_format.h"
#include "slow_motion_runtime.h"
#include "preview.h"
#include "named_audio.h"
#include "resource_validation.h"
#include "input_demo_recorder.h"
#include "input_demo_replay.h"
#include "../time_equipment.h"
#include <SDL.h>
#include <algorithm>
#include <cmath>
extern "C" {
#include "inferno.h"
#include "object.h"
#include "game.h"
#include "endlevel.h"
#include "vecmat.h"
#include "aistruct.h"
#include "weapon.h"
#include "player.h"
#include "timer.h"
#include "hudmsg.h"
#include "cntrlcen.h"
#include "newdemo.h"
#include "dxxerror.h"
#include "console.h"
#include "vclip.h"
#include "args.h"
#include "input_demo_hooks_shared.h"
#include "input_demo_hooks.h"
}
namespace
{
static_assert(XL_PLAYER_FLAGS_SLOW_MOTION == xl_compat::slow_motion_equipment && XL_PLAYER_FLAGS_BULLET_TIME == xl_compat::bullet_time_equipment);
xl_compat::SlowMotionState state;
bool slow_requested = false, bullet_requested = false;
uint32_t restore_equipment_hint = 0;
int64_t clock_ms()
{
	// The fixed offset permits restoring a 26 ms threshold age at process startup
	return timer_query() * 1000 / 65536 + 26;
}
int64_t last_missile;
uint32_t render_clock, last_omega_frame;
input_demo_environment_clock saved_environment{};
bool saved_environment_valid = false;
// Keep the cached gameplay clock in its saved domain across cold continuation
// Level changes preserve this process clock, just as they preserve SDL uptime
int64_t cached_clock_offset = 0;
bool cached_clock_held = false;
uint32_t held_cached_clock = 0;
uint32_t cached_clock_now()
{
	if (cached_clock_held) return held_cached_clock;
	const auto now = int64_t(timer_query_milliseconds()) + cached_clock_offset;
	if (now < 0 || now > INT32_MAX) Error("XL cached gameplay clock exceeds qualified signed range");
	return uint32_t(now);
}
input_demo_environment_clock environment_now()
{
	input_demo_environment_clock clock{};
	if (input_demo_replay_peek_environment(&clock)) return clock;
	return { clock_ms(), SDL_GetTicks(), render_clock, cached_clock_now() };
}
input_demo_environment_clock observe(const char *consumer, int subject = -1)
{
	input_demo_environment_clock clock{};
	xl_input_demo_observe(consumer, subject, &clock);
	return clock;
}
bool enabled()
{
	return xl_level_extensions_active() && !(Game_mode & GM_MULTI);
}
} // namespace
extern "C" void xl_input_demo_observe(const char *consumer, int subject, input_demo_environment_clock *clock)
{
	*clock = environment_now();
	if (!input_demo_observe_environment(consumer, subject, clock)) Error("XL input-demo environment: %s", consumer);
	// This counter is an environmental input; actual replay draws do not advance it
	if (input_demo_replay_format_version() == 6) {
		render_clock = clock->render_frame;
		// Ending playback can return to ordinary play in the last consumed domain
		cached_clock_offset = int64_t(clock->cached_ms) - timer_query_milliseconds();
	}
}
extern "C" uint32_t xl_input_demo_cached_clock(void)
{
	input_demo_environment_clock clock{};
	return input_demo_replay_peek_environment(&clock) ? clock.cached_ms : cached_clock_now();
}
extern "C" void xl_input_demo_hold_cached_clock(void)
{
	// Source paused CalcFrameTime returns before GetSlowTicks
	// Keep that tick through the interrupted frame, including chained messages
	held_cached_clock = xl_input_demo_cached_clock();
	cached_clock_held = true;
}
extern "C" void xl_input_demo_begin_input_clock(void)
{
	cached_clock_held = false;
}
extern "C" int xl_input_demo_restore_cached_clock(uint32_t ticks)
{
	if (ticks > INT32_MAX) return 0;
	cached_clock_held = false;
	cached_clock_offset = int64_t(ticks) - timer_query_milliseconds();
	return 1;
}
extern "C" int xl_input_demo_saved_environment(input_demo_environment_clock *clock)
{
	if (!clock || !saved_environment_valid) return 0;
	*clock = saved_environment;
	return 1;
}
extern "C" void xl_input_demo_current_environment(input_demo_environment_clock *clock)
{
	*clock = environment_now();
}
extern "C" uint32_t xl_slow_motion_last_omega_frame(void)
{
	return last_omega_frame;
}
extern "C" void xl_slow_motion_initial_omega_frame(uint32_t frame)
{
	last_omega_frame = frame;
}
namespace xl_runtime
{
bool time_equipment_resources_required(const xl_compat::Inventory &level)
{
	return xl_compat::level_has_time_equipment(level) ||
	       ((Players[Player_num].flags | restore_equipment_hint) & (XL_PLAYER_FLAGS_SLOW_MOTION | XL_PLAYER_FLAGS_BULLET_TIME));
}
xl_compat::SlowMotionOutput slow_motion_step(xl_compat::SlowMotionInput input, const xl_compat::SlowMotionSink &sink)
{
	input.xl = xl_level_extensions_active();
	input.multiplayer = (Game_mode & GM_MULTI) != 0;
	return xl_compat::slow_motion_frame(state, input, sink);
}
const xl_compat::SlowMotionState &slow_motion_state()
{
	return state;
}
} // namespace xl_runtime
extern "C" uint32_t xl_time_equipment_restore_hint(uint32_t equipment)
{
	const auto previous = restore_equipment_hint;
	restore_equipment_hint = equipment & (XL_PLAYER_FLAGS_SLOW_MOTION | XL_PLAYER_FLAGS_BULLET_TIME);
	return previous;
}
extern "C" int xl_time_equipment_initialize(object *actor, int spawned)
{
	if ((!xl_level_extensions_active() && !xl_loading_level_extensions()) || (Game_mode & GM_MULTI) ||
	    actor->type != OBJ_POWERUP || !xl_compat::time_equipment_powerup(actor->id)) return 0;
	if (spawned && !actor->size) actor->size = xl_compat::time_equipment_spawn_radius;
	actor->control_type = CT_POWERUP;
	actor->rtype.vclip_info.vclip_num = 47 - actor->id;
	actor->rtype.vclip_info.frametime = 0;
	actor->rtype.vclip_info.framenum = 0;
	return 1;
}
extern "C" int xl_time_equipment_animate(object *actor)
{
	if (!enabled() || actor->type != OBJ_POWERUP || !xl_compat::time_equipment_powerup(actor->id)) return 0;
	auto &clip = actor->rtype.vclip_info;
	const auto now = std::int64_t(observe("pickup", int(actor - Objects)).pickup_ms);
	auto elapsed = now - clip.frametime;
	if (elapsed < 0) {
		clip.frametime = fix(now);
		elapsed = 0;
	}
	const auto steps = elapsed / 80;
	if (steps) {
		clip.frametime = fix(std::int64_t(clip.frametime) + steps * 80);
		const auto frame = std::int64_t(clip.framenum) + ((actor - Objects) & 1 ? steps : -steps);
		// Retain the source's reverse-animation wrap at exact multiples of sixteen
		clip.framenum = sbyte(frame < 0 ? 16 - ((-frame % 16) ? -frame % 16 : 1) : frame % 16);
	}
	return 1;
}
extern "C" void xl_slow_motion_reset(void)
{
	cached_clock_held = false;
	state = {};
	slow_requested = bullet_requested = false;
	xl_named_audio_slow_motion_reset();
	last_missile = 0;
	// The reference loader resets the render counter, not the Omega marker
	render_clock = 0;
	saved_environment_valid = false;
}
extern "C" int xl_slow_motion_request(int bullet_time)
{
	if (!enabled()) return 0;
	if ((input_demo_recorder_is_active() && !input_demo_recorder_uses_environment()) ||
	    (input_demo_replay_is_loaded() && !input_demo_replay_environment_running())) {
		HUD_init_message(HM_DEFAULT, "XL time controls require a recorded real-time clock timeline");
		return 1;
	}
	if (!Player_is_dead) {
		if (input_demo_recorder_uses_environment()) {
			char error[256] = "";
			input_demo_record_input_context();
			if (!input_demo_recorder_stage_direct_command_xl_time_control(bullet_time, error, sizeof(error)))
				con_printf(CON_NORMAL, "XL time control recording failed: %s\n", error);
		}
		if (bullet_time) bullet_requested = true;
		else slow_requested = true;
	}
	return 1;
}
namespace
{
void runtime_step(bool slow, bool bullet, bool stop)
{
	if (!enabled() || Newdemo_state == ND_STATE_PLAYBACK) return;
	try {
		xl_compat::SlowMotionInput input;
		input.equipment = Players[Player_num].flags;
		input.energy = Players[Player_num].energy;
		input.frame = stop ? 0 : FrameTime;
		input.difficulty = Difficulty_level;
		input.now_ms = observe(stop ? "slow_off" : "slow_frame").clock_ms;
		input.reactor_destroyed = stop || Control_center_destroyed != 0;
		input.force_off = stop;
		input.mixer = !GameArg.SndNoSound && xl_named_audio_backend_ready() != 0;
		input.slow_pressed = slow;
		input.bullet_pressed = bullet;
		const auto result = xl_runtime::slow_motion_step(input, [](xl_compat::SlowMotionEvent event) {
			if (event == xl_compat::SlowMotionEvent::slowing_message) HUD_init_message(HM_DEFAULT, "Slowing down");
			else if (event == xl_compat::SlowMotionEvent::speeding_message) HUD_init_message(HM_DEFAULT, "Speeding up");
			else return xl_compat::slow_motion_audio_event(event);
			return true;
		});
		if (result.energy_delta)
			Players[Player_num].energy = static_cast<fix>(std::clamp<std::int64_t>(std::int64_t(input.energy) + result.energy_delta, 0, 200 * 65536));
	} catch (const std::exception &error) {
		Error("XL slow-motion frame: %s (frame=%d energy=%d difficulty=%d)", error.what(), FrameTime, Players[Player_num].energy, Difficulty_level);
	}
}
} // namespace
extern "C" void xl_slow_motion_frame(void)
{
	const bool slow = slow_requested, bullet = bullet_requested;
	slow_requested = bullet_requested = false;
	runtime_step(slow, bullet, false);
}
extern "C" void xl_slow_motion_off(void)
{
	runtime_step(false, false, true);
}

nlohmann::json xl_runtime::slow_motion_diagnostics()
{
	return { { "has_slow_motion", bool(Players[Player_num].flags & XL_PLAYER_FLAGS_SLOW_MOTION) },
		     { "has_bullet_time", bool(Players[Player_num].flags & XL_PLAYER_FLAGS_BULLET_TIME) },
		     { "world_speed", state.clocks[0].speed },
		     { "player_speed", state.clocks[1].speed },
		     { "world_direction", state.clocks[0].direction },
		     { "player_direction", state.clocks[1].direction },
		     { "world_active", state.clocks[0].active },
		     { "player_active", state.clocks[1].active } };
}
static nlohmann::json slow_motion_state_snapshot(const input_demo_environment_clock &environment)
{
	const auto now = environment.clock_ms;
	nlohmann::json saved = { { "clocks", nlohmann::json::array() }, { "transition_sound", state.transition_sound }, { "pickups", nlohmann::json::array() }, { "slow_requested", slow_requested }, { "bullet_requested", bullet_requested } };
	for (const auto &clock : state.clocks)
		// Only the strict 25 ms threshold matters; the reference never catches up
		saved["clocks"].push_back({ clock.speed, clock.direction, std::clamp<std::int64_t>(now - clock.updated_ms, 0, 26), clock.active });
	for (int i = 0; i <= Highest_object_index; ++i) {
		const auto &actor = Objects[i];
		if (actor.type == OBJ_POWERUP && xl_compat::time_equipment_powerup(actor.id))
			saved["pickups"].push_back({ i, actor.id, std::int64_t(environment.pickup_ms) - actor.rtype.vclip_info.frametime });
	}
	return saved;
}
nlohmann::json xl_runtime::slow_motion_snapshot()
{
	saved_environment = environment_now();
	saved_environment_valid = true;
	return slow_motion_state_snapshot(saved_environment);
}
nlohmann::json xl_runtime::slow_motion_trace_snapshot()
{
	input_demo_environment_clock anchor{};
	// Observe the last consumed sample, never a fresh timer query or save snapshot
	if (!input_demo_recorder_peek_environment(&anchor) && !input_demo_replay_peek_environment(&anchor))
		return { { "bound", bool{ false } } };
	return { { "bound", bool{ true } }, { "version", 1 }, { "anchor", { anchor.clock_ms, anchor.pickup_ms, anchor.render_frame, anchor.cached_ms } }, { "slow_motion", slow_motion_state_snapshot(anchor) }, { "weapon_clock", { last_missile - GameTime64, last_omega_frame } } };
}
bool xl_runtime::restore_slow_motion(const nlohmann::json &saved, bool apply)
{
	try {
		if (!saved.is_object() || saved.size() != 5 || !saved.at("transition_sound").is_boolean() ||
		    !saved.at("slow_requested").is_boolean() || !saved.at("bullet_requested").is_boolean() ||
		    !saved.at("clocks").is_array() || saved.at("clocks").size() != 2 || !saved.at("pickups").is_array()) return false;
		xl_compat::SlowMotionState restored;
		restored.transition_sound = saved.at("transition_sound").get<bool>();
		auto environment = environment_now();
		// Admission installs the captured origin before ordinary checkpoint restore
		if (!input_demo_replay_environment_running()) input_demo_replay_get_environment_origin(&environment);
		const auto now = environment.clock_ms;
		for (unsigned i = 0; i < 2; ++i) {
			const auto &row = saved.at("clocks")[i];
			if (!row.is_array() || row.size() != 4 || !row[0].is_number() || !row[1].is_number_integer() ||
			    !row[2].is_number_integer() || !row[3].is_boolean()) return false;
			const auto speed = row[0].get<float>();
			if ((row[1].is_number_unsigned() && row[1].get<std::uint64_t>() > 1) ||
			    (row[2].is_number_unsigned() && row[2].get<std::uint64_t>() > 26)) return false;
			const auto direction = row[1].get<std::int64_t>(), age = row[2].get<std::int64_t>();
			if (!std::isfinite(speed) || speed < 1 || speed > 3 || direction < -1 || direction > 1 || age < 0 || age > 26) return false;
			restored.clocks[i] = { speed, int(direction), now - age, row[3].get<bool>() };
		}
		std::size_t pickup = 0;
		const auto pickup_now = std::int64_t(environment.pickup_ms);
		for (int i = 0; i <= Highest_object_index; ++i) {
			const auto &actor = Objects[i];
			if (actor.type != OBJ_POWERUP || !xl_compat::time_equipment_powerup(actor.id)) continue;
			if (pickup >= saved.at("pickups").size()) return false;
			const auto &row = saved.at("pickups")[pickup++];
			if (!row.is_array() || row.size() != 3 || !row[0].is_number_integer() || !row[1].is_number_integer() || row[0] != i || row[1] != actor.id || !row[2].is_number_integer() ||
			    (row[2].is_number_unsigned() && row[2].get<std::uint64_t>() > INT32_MAX)) return false;
			const auto age = row[2].get<std::int64_t>();
			if (age < 0 || age > INT32_MAX || pickup_now - age > INT32_MAX || actor.render_type != RT_POWERUP ||
			    actor.rtype.vclip_info.vclip_num != 47 - actor.id || actor.rtype.vclip_info.framenum < 0 || actor.rtype.vclip_info.framenum >= 16) return false;
		}
		if (pickup != saved.at("pickups").size()) return false;
		if (apply) {
			state = restored;
			slow_requested = saved.at("slow_requested").get<bool>();
			bullet_requested = saved.at("bullet_requested").get<bool>();
			for (const auto &row : saved.at("pickups"))
				Objects[row[0].get<int>()].rtype.vclip_info.frametime = fix(pickup_now - row[2].get<std::int64_t>());
		}
		return true;
	} catch (const std::exception &) {
		return false;
	}
}
extern "C" void xl_slow_motion_offset(const object *actor, vms_vector *offset)
{
	if (!enabled()) return;
	const float speed = state.clocks[actor == ConsoleObject ? 1 : 0].speed;
	if (speed == 1) return;
	// Reference UpdateOffset scales the computed vector, after fluid displacement
	const auto scale = static_cast<fix>(65536 / speed);
	offset->x = static_cast<fix>(double(offset->x) * scale / 65536.0);
	offset->y = static_cast<fix>(double(offset->y) * scale / 65536.0);
	offset->z = static_cast<fix>(double(offset->z) * scale / 65536.0);
}
extern "C" int32_t xl_slow_motion_lifetime(const object *actor, int32_t time)
{
	// Redux markers use lifeleft as their flashing clock, unlike the XL renderer
	if (!enabled() || actor->type == OBJ_MARKER) return time;
	const auto remaining = actor->lifeleft;
	if (remaining == 0x3ffffffe || remaining == IMMORTAL_TIME || time <= 0) return 0;
	if (state.clocks[0].speed == 1) return time;
	return static_cast<int32_t>(time / state.clocks[0].speed);
}

extern "C" void xl_slow_motion_rotation(const object *actor, vms_angvec *angles)
{
	if (!enabled()) return;
	const float speed = state.clocks[actor == ConsoleObject ? 1 : 0].speed;
	if (speed == 1) return;
	// DoPhysicsSimRot divides already wrapped angles, not thrust or frame time
	angles->p = static_cast<fixang>(angles->p / speed);
	angles->h = static_cast<fixang>(angles->h / speed);
	angles->b = static_cast<fixang>(angles->b / speed);
}

extern "C" void xl_slow_motion_turn_delta(const object *actor, int32_t *pitch, int32_t *heading)
{
	if (!enabled()) return;
	const float speed = state.clocks[actor == ConsoleObject ? 1 : 0].speed;
	if (speed == 1) return;
	// Reference TurnTowardsVector scales before sign-change saturation
	*pitch = static_cast<fix>(*pitch / speed);
	*heading = static_cast<fix>(*heading / speed);
}

extern "C" int32_t xl_slow_motion_world_time(int32_t time)
{
	if (!enabled() || state.clocks[0].speed == 1) return time;
	return static_cast<int32_t>(time / state.clocks[0].speed);
}
extern "C" int xl_slow_motion_ai_timers(ai_local *local, int secondary, int32_t time)
{
	if (!enabled()) return 0;
	const float speed = state.clocks[0].speed;
	if (local->next_fire > -8 * F1_0 * speed) local->next_fire -= xl_slow_motion_world_time(time);
	if (secondary != -1) {
		if (local->next_fire2 > -8 * F1_0 * speed) local->next_fire2 -= xl_slow_motion_world_time(time);
	} else local->next_fire2 = 8 * F1_0;
	// Scheduling is intentionally not on the world clock
	if (local->time_since_processed < 256 * F1_0) local->time_since_processed += time;
	return 1;
}

static int firing_blocked(int64_t last, int64_t next, int64_t now, int seconds)
{
	if (!enabled()) return next > now;
	const float speed = state.clocks[0].speed;
	int64_t deadline = next;
	if (speed > 1) {
		// Relative fixed-point intervals retain precision on Redux's 64-bit clock
		const bool positive = next >= last;
		const uint64_t magnitude = positive ? uint64_t(next) - uint64_t(last) : uint64_t(last) - uint64_t(next);
		if (magnitude > uint64_t(INT32_MAX)) return 1;
		const int64_t interval = positive ? int64_t(magnitude) : -int64_t(magnitude);
		const float scaled = float(interval) * speed;
		// The reference casts to fix; reject values outside that defined domain
		if (double(scaled) > INT32_MAX || double(scaled) < INT32_MIN) return 1;
		const auto delay = static_cast<int32_t>(scaled);
		if ((delay > 0 && last > INT64_MAX - delay) || (delay < 0 && last < INT64_MIN - delay)) return 1;
		deadline = last + delay;
	}
	// Compare remaining duration, not float absolute timestamps
	return deadline > now && uint64_t(deadline) - uint64_t(now) < double(seconds * F1_0 * speed);
}
extern "C" int xl_slow_motion_flare_blocked(int64_t next, int64_t now)
{
	if (!enabled()) return next > now;
	return next > now && uint64_t(next) - uint64_t(now) < 2 * F1_0;
}
extern "C" int32_t xl_slow_motion_flare_delay(int enough_energy)
{
	const float speed = enabled() ? state.clocks[0].speed : 1;
	// F2X then integer division for sufficient energy; multiply for empty energy
	return enough_energy ? int32_t(speed * F1_0) / 4 : int32_t(speed * (2 * F1_0));
}

extern "C" int xl_slow_motion_primary_blocked(int64_t last, int64_t next, int64_t now)
{
	return firing_blocked(last, next, now, 2);
}
extern "C" int xl_slow_motion_missile_blocked(int64_t next, int64_t now)
{
	return firing_blocked(last_missile, next, now, 5);
}
extern "C" void xl_slow_motion_missile_fired(void)
{
	if (enabled()) last_missile = GameTime64;
}
nlohmann::json xl_runtime::weapon_clock_snapshot()
{
	const auto rendered = environment_now().render_frame;
	saved_environment.render_frame = rendered;
	return nlohmann::json{ last_missile - GameTime64, rendered, last_omega_frame };
}
bool xl_runtime::restore_weapon_clock(const nlohmann::json &saved, bool apply)
{
	try {
		if (!save_epoch_valid(GameTime64) || !saved.is_array() || saved.size() != 3) return false;
		const auto integer = [](const nlohmann::json &value, int64_t low, int64_t high) {
			if (!value.is_number_integer() || (value.is_number_unsigned() && value.get<uint64_t>() > uint64_t(high))) throw std::runtime_error("Invalid weapon clock integer");
			const auto result = value.get<int64_t>();
			if (result < low || result > high) throw std::runtime_error("Weapon clock outside bounds");
			return result;
		};
		const auto missile = GameTime64 + integer(saved[0], -SAVE_EPOCH_MAX, 0);
		const auto rendered = uint32_t(integer(saved[1], 0, UINT32_MAX));
		const auto omega = uint32_t(integer(saved[2], 0, UINT32_MAX));
		if (!save_timestamp_valid(missile)) return false;
		if (apply) {
			last_missile = missile;
			render_clock = rendered;
			last_omega_frame = omega;
		}
		return true;
	} catch (const std::exception &) {
		return false;
	}
}

extern "C" void xl_slow_motion_render_advance(void)
{
	if (enabled() && input_demo_replay_format_version() != 6) ++render_clock;
}
extern "C" uint32_t xl_slow_motion_render_clock(void)
{
	return environment_now().render_frame;
}

extern "C" int xl_slow_motion_omega_blocked(void)
{
	if (!enabled()) return -1;
	const auto rendered = observe("omega_blocked").render_frame;
	return last_omega_frame == rendered || last_omega_frame == rendered - 1;
}
extern "C" int xl_slow_motion_omega_fired(void)
{
	if (!enabled()) return 0;
	last_omega_frame = observe("omega_fired").render_frame;
	return 1;
}

extern "C" const char *xl_input_demo_profile_error(void)
{
	if (!enabled()) return nullptr;
	// Low-memory bitmap substitution can change transparency collision pixels
	if (GameArg.SysLowMem) return "XL input demos require low-memory bitmap substitution to be disabled";
	// Only the enabled/all-weapons flags have an ordered startup/input contract
	if (cheats.lamer || cheats.allkeys || cheats.accessory || cheats.invul || cheats.shields ||
	    cheats.killreactor || cheats.exitpath || cheats.levelwarp || cheats.fullautomap ||
	    cheats.ghostphysics || cheats.rapidfire || cheats.bouncyfire || cheats.turbo ||
	    cheats.robotfiringsuspended || cheats.killallrobots || cheats.robotskillrobots ||
	    cheats.monsterdamage || cheats.buddyclone || cheats.buddyangry || cheats.acid)
		return "XL input demos do not support an active cheat outside the ordered all-weapons contract";
	// The ordinary escape sequence suspends robots as part of its native lifecycle
	if (Game_suspended & ~(Endlevel_sequence ? SUSP_ROBOTS : 0))
		return "XL input demos do not support suspended simulation subsystems";
	return nullptr;
}
extern "C" const char *xl_omega_input_demo_error(void)
{
	if (const auto *error = xl_input_demo_profile_error()) return error;
#ifdef DXX_XL_DEVELOPER_PROBE
	if (input_demo_recorder_uses_environment() || input_demo_replay_format_version() == 6) return nullptr;
#endif
	if (enabled() && (xl_prepared_time_equipment_image(0) || (Players[Player_num].flags & (xl_compat::slow_motion_equipment | xl_compat::bullet_time_equipment))))
		return "XL time-equipment input demos require an unimplemented real-time clock timeline";
	return enabled() && (Players[Player_num].primary_weapon_flags & (1 << OMEGA_INDEX))
	           ? "XL Omega input demos require an unimplemented render-clock timeline"
	           : nullptr;
}
