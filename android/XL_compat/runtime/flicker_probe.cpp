#include "flicker_probe.h"
#include "flicker.h"
#include "changing_light.h"
#include "preview.h"
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
extern "C" {
#include "inferno.h"
#include "object.h"
#include "player.h"
#include "gameseg.h"
#include "switch.h"
#include "game.h"
#include "state_android_shared.h"
#include "android_meta_actions.h"
#include "android_log.h"
#include "debug_log_categories.h"
void flicker_lights(void);
}
namespace
{
std::string failure;
std::vector<xl_flicker_state> earned;
std::vector<xl_light_segment_state> earned_light;
void require(bool condition, const char *message)
{
	if (!condition) throw std::runtime_error(message);
}
std::vector<xl_flicker_state> snapshot()
{
	std::vector<xl_flicker_state> rows(xl_flicker_count());
	for (std::size_t i = 0; i < rows.size(); ++i) require(xl_flicker_get(static_cast<int>(i), &rows[i]), "Cannot capture Android flicker phase");
	return rows;
}
std::vector<xl_light_segment_state> lights()
{
	std::vector<xl_light_segment_state> rows(xl_light_segment_count());
	for (std::size_t i = 0; i < rows.size(); ++i) require(xl_light_segment_get(static_cast<int>(i), &rows[i]), "Cannot capture Android flicker light consumers");
	return rows;
}
template <class T>
bool same(const std::vector<T> &a, const std::vector<T> &b)
{
	return a.size() == b.size() && (a.empty() || !std::memcmp(a.data(), b.data(), a.size() * sizeof(T)));
}
void frame(fix elapsed)
{
	const auto before = FrameTime;
	FrameTime = elapsed;
	flicker_lights();
	FrameTime = before;
}
void verify()
{
	require(xl_level_extensions_active() && earned.size() == 130 && same(earned, snapshot()) && same(earned_light, lights()), "Android restore lost flicker pattern, clock or earned light consumers");
}
} // namespace
const char *xl_flicker_probe(const char *operation)
{
	try {
		require(Game_wind && ConsoleObject && !(Game_mode & GM_MULTI), "Flicker controls require running singleplayer");
		if (!std::strcmp(operation, "prepare")) {
			require(xl_level_extensions_active() && xl_flicker_count() == 130 && Num_flickering_lights == 0, "Flicker controls require the owned 130-row fixture");
			auto state = snapshot();
			for (std::size_t i = 0; i < state.size(); ++i) {
				require(state[i].delay == (i == 129 ? 0 : F1_0 / 4), "Wrong authored flicker delay");
				state[i].timer = i == 129 ? 0 : F1_0 / 8;
				state[i].mask = 2;
			}
			require(xl_flicker_restore(state.data(), static_cast<int>(state.size()), 1), "Cannot seed the controlled authored flicker phase");
			add_light(state[0].segment, state[0].side);
			add_light(state[128].segment, state[128].side);
			const auto initial = lights();
			frame(F1_0 / 8);
			state = snapshot();
			require(state[0].timer == 0 && state[128].timer == 0 && state[0].mask == 2 && state[129].timer == INT32_MIN, "Android exact-boundary or zero-delay behavior differs");
			frame(1);
			state = snapshot();
			require(state[0].mask == 1 && state[128].mask == 1 && state[0].timer == F1_0 / 4 - 1, "Android flicker rotated in the Redux direction");
			frame(F1_0 / 4);
			state = snapshot();
			require(state[0].mask == 0x80000000u && state[128].mask == 0x80000000u && !same(initial, lights()), "Android flicker did not reach actual light consumers");
			require(check_trigger_sub(Num_triggers - 2, Player_num, 1) == 0, "Android authored flicker off failed");
			state = snapshot();
			require(state[0].timer == INT32_MIN && state[128].timer == INT32_MIN && state[1].timer >= 0, "Android authored disable lost first-duplicate ownership");
			require(check_trigger_sub(Num_triggers - 1, Player_num, 1) == 0, "Android authored flicker on failed");
			state = snapshot();
			require(state[0].timer == 0 && state[128].timer == 0, "Android authored enable did not restart clocks");
			frame(1);
			state = snapshot();
			require(state[0].mask == 0x40000000u && state[128].mask == 0x40000000u, "Android restart reset earned patterns");
			disable_flicker(state[0].segment, state[0].side);
			earned = snapshot();
			earned_light = lights();
			verify();
		} else if (!std::strcmp(operation, "verify")) verify();
		else if (!std::strcmp(operation, "disturb")) {
			verify();
			frame(F1_0);
			require(!same(earned, snapshot()), "Android flicker disturbance did not advance phase");
		} else if (!std::strcmp(operation, "disk_save")) {
			verify();
			require(state_android_save_to_slot(ANDROID_SAVE_META_SLOT_QUICK, "XL earned flicker phase", ANDROID_SAVE_META_KIND_MANUAL) == 1, "Cannot save Android flicker state");
		} else if (!std::strcmp(operation, "disk_load")) {
			require(!earned.empty() && state_android_restore_slot(ANDROID_SAVE_META_SLOT_QUICK) == 1, "Cannot restore Android flicker state");
			verify();
			debug_log_force(DLOG_GAME, "Android full flicker phases and light consumers restored exactly before simulation");
			require(android_open_overlay_pause_if_safe(), "Cannot pause restored flicker state");
		} else if (!std::strcmp(operation, "stock")) {
			require(!xl_level_extensions_active() && !xl_flicker_count() && !xl_flicker_frame() && !xl_flicker_enable(0, 0, 1), "XL flicker state leaked into stock");
		} else require(false, "Unknown flicker probe operation");
		return nullptr;
	} catch (const std::exception &error) {
		failure = error.what();
		return failure.c_str();
	}
}
