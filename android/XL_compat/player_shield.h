#ifndef DXX_XL_PLAYER_SHIELD_H
#define DXX_XL_PLAYER_SHIELD_H
#include "shield_pulse.h"
#include <array>
#include <cmath>
#include <stdexcept>

namespace xl_compat
{
struct PlayerShieldState {
	int was_hit = 0;
	int32_t last_hit = 0;
	ShieldPulse pulse;
};
struct PlayerShieldInputs {
	// These booleans are resolved source render/profile gates, not new settings
	bool object_effects = true, shadows = false, fast_shadows = true;
	int shadow_pass = 0;
	bool shields = true, only_hits = true, appearing = false;
	bool cloaked = false, cloak_visible = true, invulnerable = false;
	int cloak_fade = 31;
	int32_t ticks = 0, shield = 6553600, maximum_shield = 6553600;
	int duration = 2000;
};
struct PlayerShieldPresentation {
	bool setup_surface = false, draw = false;
	std::array<float, 4> color{};
};
inline PlayerShieldPresentation player_shield_presentation(PlayerShieldState &state, const PlayerShieldInputs &input)
{
	PlayerShieldPresentation result;
	if (!input.object_effects || (input.shadows && input.shadow_pass != (input.fast_shadows ? 1 : 3))) return result;
	if (!input.appearing && !input.shields) return result;
	float scale = 1;
	if (input.cloaked) {
		if (!input.cloak_visible) return result;
		scale = float(input.cloak_fade) / 31.f;
		scale *= scale;
	}
	const int64_t elapsed = int64_t(input.ticks) - state.last_hit;
	if ((input.duration != 500 && input.duration != 2000) || input.maximum_shield <= 0 || elapsed < INT32_MIN || elapsed > INT32_MAX)
		throw std::runtime_error("Unsupported player shield timing or maximum shield");
	result.setup_surface = true;
	int32_t dt = 0;
	if (state.was_hit < 0) {
		state.was_hit = 1;
		state.last_hit = input.ticks;
		state.pulse.setup(.1f, .5f);
	} else if (state.was_hit) {
		dt = int32_t(elapsed);
		if (dt >= 300) {
			state.was_hit = 0;
			state.pulse.setup(.02f, .4f);
		}
	}
	if (!input.appearing && input.only_hits && !state.was_hit) return result;
	const unsigned color = input.appearing ? 1 : input.invulnerable ? 2
	                                                                : 0;
	float alpha;
	if (state.was_hit) {
		alpha = !input.appearing && input.only_hits ? float(::cos(::sqrt(double(dt) / float(input.duration)) * 3.141592653589793240 / 2)) : 1.f;
		scale *= alpha;
	} else if (input.invulnerable) alpha = 1;
	else {
		alpha = (float(input.shield) / 65536.f) / (float(input.maximum_shield) / 65536.f);
		scale *= alpha;
		if (!state.pulse.valid) state.pulse.setup(.02f, .5f);
	}
	const std::array<std::array<float, 3>, 3> colors{ { { 0, .5f, 1 }, { 1, .5f, 0 }, { 1, .8f, .6f } } };
	result.draw = true;
	result.color = { colors[color][0] * scale, colors[color][1] * scale, colors[color][2] * scale, alpha };
	return result;
}
} // namespace xl_compat
#endif
