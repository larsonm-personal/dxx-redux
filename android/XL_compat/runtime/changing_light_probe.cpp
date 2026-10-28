#include "changing_light_probe.h"
#include "changing_light.h"
#include "preview.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
extern "C" {
#include "inferno.h"
#include "object.h"
#include "player.h"
#include "segment.h"
#include "gameseg.h"
#include "switch.h"
#include "game.h"
#include "state_android_shared.h"
#include "android_meta_actions.h"
#include "android_log.h"
#include "debug_log_categories.h"
}
namespace
{
std::string failure;
std::vector<xl_light_segment_state> earned;
void require(bool condition, const char *message)
{
	if (!condition) throw std::runtime_error(message);
}
std::vector<xl_light_segment_state> snapshot()
{
	std::vector<xl_light_segment_state> result(xl_light_segment_count());
	for (std::size_t i = 0; i < result.size(); ++i)
		require(xl_light_segment_get(static_cast<int>(i), &result[i]), "Cannot capture Android light consumers");
	return result;
}
bool same(const std::vector<xl_light_segment_state> &a, const std::vector<xl_light_segment_state> &b)
{
	return a.size() == b.size() && (a.empty() || !std::memcmp(a.data(), b.data(), a.size() * sizeof(a[0])));
}
void verify()
{
	require(xl_level_extensions_active() && xl_light_index_count() == 501 && xl_light_delta_count() == 73726 &&
	            !earned.empty() && same(earned, snapshot()),
	        "Android light restore lost full records or earned brightness");
}
void disturb()
{
	verify();
	xl_light_index index{};
	require(xl_light_index_get(500, &index) && subtract_light(index.segment, index.side) == 1,
	        "Cannot disturb high-index Android light source");
	require(!same(earned, snapshot()), "Android light disturbance did not change actual consumers");
}
} // namespace
const char *xl_changing_light_probe(const char *operation)
{
	try {
		require(Game_wind && ConsoleObject && !(Game_mode & GM_MULTI), "Light controls require running singleplayer");
		if (!std::strcmp(operation, "prepare")) {
			require(xl_level_extensions_active() && xl_light_index_count() == 501 && xl_light_delta_count() == 73726 && Num_triggers >= 2,
			        "Light controls require the authored full-width fixture");
			const auto before = snapshot();
			std::vector<std::array<int64_t, 24>> contribution(Num_segments);
			for (int i = 0; i < xl_light_index_count(); ++i) {
				xl_light_index index{};
				require(xl_light_index_get(i, &index) && (i != 500 || (index.first == 65535 && index.count == 8191)), "Android light range narrowed");
				for (int j = 0; j < index.count; ++j) {
					xl_light_delta delta{};
					require(xl_light_delta_get(index.first + j, &delta), "Android light contribution missing");
					for (int c = 0; c < 4; ++c) contribution[delta.segment][delta.side * 4 + c] += int64_t(delta.light[c]) * DL_SCALE;
				}
			}
			require(check_trigger_sub(Num_triggers - 2, Player_num, 1) == 0, "Android authored light-off failed");
			const auto dark = snapshot();
			for (int s = 0; s < Num_segments; ++s)
				for (int c = 0; c < 24; ++c)
					require(dark[s].light[c] == (std::max) (INT64_C(0), int64_t(before[s].light[c]) - contribution[s][c]), "Android light-off lost clamped contributions");
			for (int i : { 0, 500 }) {
				xl_light_index index{};
				require(xl_light_index_get(i, &index) && subtract_light(index.segment, index.side) == 0, "Android repeated subtraction changed source mask");
			}
			require(same(dark, snapshot()), "Android repeated light-off changed earned brightness");
			require(check_trigger_sub(Num_triggers - 1, Player_num, 1) == 0, "Android authored light-on failed");
			earned = snapshot();
			for (int s = 0; s < Num_segments; ++s)
				for (int c = 0; c < 24; ++c)
					require(earned[s].light[c] == dark[s].light[c] + contribution[s][c], "Android light-on lost high-index contributions");
			require(!same(before, earned), "Android light fixture lacks non-invertible clamping witness");
			verify();
		} else if (!std::strcmp(operation, "verify")) verify();
		else if (!std::strcmp(operation, "disturb")) disturb();
		else if (!std::strcmp(operation, "disk_save")) {
			verify();
			require(state_android_save_to_slot(ANDROID_SAVE_META_SLOT_QUICK, "XL earned light state", ANDROID_SAVE_META_KIND_MANUAL) == 1, "Cannot save Android light state");
		} else if (!std::strcmp(operation, "disk_load")) {
			require(!earned.empty() && state_android_restore_slot(ANDROID_SAVE_META_SLOT_QUICK) == 1, "Cannot restore Android light state");
			verify();
			debug_log_force(DLOG_GAME, "Full Android UVL and ambient light state restored exactly before simulation");
			require(android_open_overlay_pause_if_safe(), "Cannot pause restored light state");
		} else if (!std::strcmp(operation, "stock")) {
			require(!xl_level_extensions_active() && !xl_light_index_count() && !xl_light_segment_count() && !xl_changing_light(0, 0, -1), "XL changing lights leaked into stock");
		} else require(false, "Unknown changing-light probe operation");
		return nullptr;
	} catch (const std::exception &error) {
		failure = error.what();
		return failure.c_str();
	}
}
