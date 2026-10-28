#include "flicker.h"
#include "preview.h"
#include "resource_validation.h"
#include "texture_brightness.h"
#include "../inventory.h"
#include <algorithm>
#include <array>
#include <numeric>
#include <stdexcept>
#include <vector>
extern "C" {
#include "inferno.h"
#include "segment.h"
#include "gameseg.h"
#include "wall.h"
#include "game.h"
}
namespace
{
const xl_compat::Inventory *authored;
std::vector<xl_flicker_state> live;
std::vector<std::size_t> ordered;
std::array<int, 910> brightness;
int find(int segment, int side)
{
	const auto at = std::lower_bound(ordered.begin(), ordered.end(), std::pair<int, int>{ segment, side }, [](std::size_t index, const std::pair<int, int> &key) {
		return std::pair<int, int>{ live[index].segment, live[index].side } < key;
	});
	return at != ordered.end() && live[*at].segment == segment && live[*at].side == side ? static_cast<int>(*at) : -1;
}
} // namespace
void xl_flicker_bind(const xl_compat::Inventory *source)
{
	if (!source || source->segments.size() != static_cast<std::size_t>(Num_segments)) throw std::runtime_error("XL flicker geometry does not match its source");
	std::vector<xl_flicker_state> next;
	next.reserve(source->flicker_lights.size());
	for (const auto &row : source->flicker_lights) {
		if (row[0] < 0 || row[0] >= Num_segments || row[1] < 0 || row[1] >= 6 || row[4] < 0)
			throw std::runtime_error("XL flicker target or delay is invalid");
		next.push_back({ row[0], row[1], row[4], row[3], static_cast<uint32_t>(row[2]) });
	}
	std::vector<std::size_t> indices(next.size());
	std::iota(indices.begin(), indices.end(), 0);
	std::stable_sort(indices.begin(), indices.end(), [&](std::size_t a, std::size_t b) {
		return std::pair<int, int>{ next[a].segment, next[a].side } < std::pair<int, int>{ next[b].segment, next[b].side };
	});
	std::array<int, 910> next_brightness{};
	for (const auto &entry : xl_compat::texture_brightness) next_brightness[entry.texture] = entry.brightness;
	for (unsigned i = 0; i < next_brightness.size(); ++i) {
		int value;
		if (xl_prepared_texture_brightness(i, &value)) next_brightness[i] = value;
	}
	live.swap(next);
	ordered.swap(indices);
	brightness = next_brightness;
	authored = source;
}
extern "C" void xl_flicker_clear(void)
{
	authored = nullptr;
	live.clear();
	ordered.clear();
	brightness.fill(0);
}
extern "C" int xl_flicker_count(void)
{
	return static_cast<int>(live.size());
}
extern "C" int xl_flicker_get(int index, xl_flicker_state *out)
{
	if (!out || index < 0 || index >= xl_flicker_count()) return 0;
	*out = live[index];
	return 1;
}
extern "C" int xl_flicker_has_source(int segment, int side)
{
	return find(segment, side) >= 0;
}
extern "C" int xl_flicker_step(xl_flicker_state *state, int elapsed, int visible, int emitting)
{
	if (!state || state->timer == INT32_MIN || elapsed < 0) return -1;
	if (!visible || !emitting || state->delay <= 0) {
		state->timer = INT32_MIN;
		return -1;
	}
	const int64_t timer = int64_t(state->timer) - elapsed;
	if (timer >= 0) {
		state->timer = static_cast<int>(timer);
		return -1;
	}
	// Match the source's catch-up clock and single rotation without an unbounded loop
	const int64_t intervals = (-timer + state->delay - 1) / state->delay;
	state->timer = static_cast<int>(timer + intervals * state->delay);
	state->mask = (state->mask >> 1) | (state->mask << 31);
	return static_cast<int>(state->mask & 1);
}
extern "C" int xl_flicker_frame(void)
{
	if (!authored || !xl_level_extensions_active()) return 0;
	for (auto &state : live) {
		if (state.timer == INT32_MIN) continue;
		const auto &face = Segments[state.segment].sides[state.side];
		const int base = face.tmap_num, overlay = face.tmap_num2 & 0x3fff;
		const bool emitting = (base >= 0 && base < static_cast<int>(brightness.size()) && brightness[base]) ||
		                      (overlay >= 0 && overlay < static_cast<int>(brightness.size()) && brightness[overlay]);
		const int action = xl_flicker_step(&state, FrameTime, (WALL_IS_DOORWAY(&Segments[state.segment], state.side) & WID_RENDER_FLAG) != 0, emitting);
		if (action == 1) add_light(state.segment, state.side);
		else if (action == 0) subtract_light(state.segment, state.side);
	}
	return 1;
}
extern "C" int xl_flicker_enable(int segment, int side, int enabled)
{
	if (!authored || !xl_level_extensions_active()) return 0;
	const int index = find(segment, side);
	if (index >= 0) live[index].timer = enabled ? 0 : INT32_MIN;
	return 1;
}
extern "C" int xl_flicker_restore(const xl_flicker_state *states, int count, int apply)
{
	if (!authored || count != xl_flicker_count() || (count && !states)) return 0;
	for (int i = 0; i < count; ++i) {
		const auto &state = states[i];
		const auto &original = authored->flicker_lights[i];
		if (state.segment != original[0] || state.side != original[1] || state.delay != original[4]) return 0;
		const int maximum_timer = (std::max) (0, (std::max) (original[3], original[4] - 1));
		if (state.timer > maximum_timer) return 0;
		uint32_t rotated = static_cast<uint32_t>(original[2]);
		bool reachable = false;
		for (int bit = 0; bit < 32; ++bit) {
			reachable |= state.mask == rotated;
			rotated = (rotated >> 1) | (rotated << 31);
		}
		if (!reachable || (state.timer < 0 && state.timer != INT32_MIN &&
		                   (state.timer != original[3] || state.mask != static_cast<uint32_t>(original[2])))) return 0;
	}
	if (apply && count) std::copy(states, states + count, live.begin());
	return 1;
}
