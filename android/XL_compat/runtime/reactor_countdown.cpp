#include "reactor_countdown.h"
#include "../countdown.h"
#include "preview.h"
#include "input_demo_hooks.h"
extern "C" {
#include "inferno.h"
#include "cntrlcen.h"
#include "game.h"
#include "gameseq.h"
#include "object.h"
#include "switch.h"
#include "digi.h"
#include "gr.h"
#include "rng_shared.h"
extern int Countdown_seconds_left, Total_countdown_time;
}
namespace
{
int pending = 0, phase = 0, fraction = 0;
#if defined(INTROSPECT_ON) || defined(DXX_XL_DEVELOPER_PROBE)
std::array<unsigned, 256> sounds{};
unsigned deaths = 0;
#endif
xl_compat::Countdown current()
{
	return { Control_center_destroyed, Countdown_timer, Countdown_seconds_left, Total_countdown_time, pending };
}
void publish(const xl_compat::Countdown &state)
{
	Control_center_destroyed = state.destroyed;
	Countdown_timer = state.timer;
	Countdown_seconds_left = state.seconds;
	Total_countdown_time = state.total;
	pending = state.pending;
}
} // namespace
extern "C" void xl_countdown_clear(void)
{
	pending = phase = fraction = 0;
}
extern "C" int xl_countdown_valid(const trigger *action)
{
	return action && action->type == 25 && xl_compat::countdown_total(action->time, Base_control_center_explosion_time, Difficulty_level) >= 0;
}
extern "C" int xl_countdown_action(const trigger *action)
{
	if (!xl_level_extensions_active() || !xl_countdown_valid(action)) return 0;
	auto state = current();
	if (!xl_compat::initialize_countdown(state, action->time, Base_control_center_explosion_time, Difficulty_level)) return 0;
	publish(state);
	reactor_countdown_reset_pause();
	return 1;
}
extern "C" void xl_countdown_frame(void)
{
	if (!xl_level_extensions_active() || Reactor_countdown_paused) return;
	// A saved simulation clock replaces XL's SDL wall clock. Preserve sub-ms
	// remainder as well as the source's discarded tick overshoot across restore
	const auto elapsed = std::int64_t(fraction) + std::int64_t((std::max) (FrameTime, 0)) * 1000;
	fraction = int(elapsed % F1_0);
	const bool tick = xl_compat::countdown_tick(phase, elapsed / F1_0);
	auto state = current();
	xl_compat::CountdownInput input{ FrameTime, Difficulty_level, tick, true, game_is_time_paused() != 0, Player_is_dead != 0, false };
	const auto output = xl_compat::advance_countdown(state, input, [] { return d_rand() - 16384; });
	publish(state);
	ConsoleObject->mtype.phys_info.rotvel.x += output.rotation_x;
	ConsoleObject->mtype.phys_info.rotvel.z += output.rotation_z;
	for (unsigned i = 0; i < output.sound_count; ++i) {
		const auto &sound = output.sounds[i];
#if defined(INTROSPECT_ON) || defined(DXX_XL_DEVELOPER_PROBE)
		++sounds[sound.id];
#endif
		digi_play_sample(sound.id, sound.volume);
	}
	if (output.flash >= 0) PALETTE_FLASH_SET(output.flash, output.flash, output.flash);
	if (!output.death) return;
	gr_set_current_canvas(nullptr);
	gr_clear_canvas(BM_XRGB(31, 31, 31));
	reset_palette_add();
	if (input_demo_finish_replay_from_mine_exit()) return;
#if defined(INTROSPECT_ON) || defined(DXX_XL_DEVELOPER_PROBE)
	++deaths;
#endif
	DoPlayerDead();
}
extern "C" void xl_countdown_get(xl_countdown_state *state)
{
	if (state) *state = { Control_center_destroyed, Countdown_timer, Countdown_seconds_left, Total_countdown_time, pending, phase, fraction };
}
extern "C" int xl_countdown_restore(const xl_countdown_state *state, int apply)
{
	if (!xl_level_extensions_active() || !state || state->destroyed < 0 || state->destroyed > 1 ||
	    state->timer < -128 * F1_0 || state->seconds < -128 || state->seconds > 32767 || state->total < 0 || state->total > 32767 ||
	    state->pending < 0 || state->pending > F1_0 * 25 / 1000 || state->phase < 0 || state->phase >= 25 || state->fraction < 0 || state->fraction >= F1_0 ||
	    state->destroyed != Control_center_destroyed || state->timer != Countdown_timer) return 0;
	if (apply) {
		publish({ state->destroyed, state->timer, state->seconds, state->total, state->pending });
		phase = state->phase;
		fraction = state->fraction;
	}
	return 1;
}
#if defined(INTROSPECT_ON) || defined(DXX_XL_DEVELOPER_PROBE)
extern "C" unsigned xl_countdown_sound_queries(int sound)
{
	return sound >= 0 && sound < int(sounds.size()) ? sounds[sound] : 0;
}
extern "C" unsigned xl_countdown_death_queries(void)
{
	return deaths;
}
#endif
