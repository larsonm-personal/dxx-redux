#include "camera_flash.h"
#include "../camera_flash.h"
#include "preview.h"
#include "save_epoch_format.h"
#include <nlohmann/json.hpp>
extern "C" {
#include "inferno.h"
#include "game.h"
#include "digi.h"
#include "sounds.h"
#include "newdemo.h"
#include "rng_shared.h"
extern fix Flash_effect;
extern fix64 Time_flash_last_played;
}
namespace
{
xl_compat::CameraFlashPalette current;
bool active = false;
void publish()
{
	const auto color = current.saved_color();
	PaletteRedAdd = color[0];
	PaletteGreenAdd = color[1];
	PaletteBlueAdd = color[2];
	Flash_effect = current.delay;
	Time_flash_last_played = current.last_sound;
}
void import_legacy_color()
{
	current.start({ float(PaletteRedAdd) / 64.0f, float(PaletteGreenAdd) / 64.0f, float(PaletteBlueAdd) / 64.0f });
	current.delay = Flash_effect;
	current.last_sound = Time_flash_last_played;
}
} // namespace
void xl_camera_flash_blast(int force, int flash, bool self)
{
	const auto pulse = xl_compat::camera_flash_pulse(force, flash, self);
	if (!pulse.intensity) return;
	if (!active) import_legacy_color();
	current.bump({ pulse.intensity, pulse.intensity, pulse.intensity });
	current.delay = pulse.delay;
	active = true;
	publish();
}
extern "C" int xl_camera_flash_add(int red, int green, int blue)
{
	if (!active || !xl_level_extensions_active()) return 0;
	current.delay = Flash_effect;
	current.bump({ red, green, blue });
	publish();
	return 1;
}
extern "C" void xl_camera_flash_palette_set(void)
{
	if (active) {
		import_legacy_color();
		publish();
	}
}
extern "C" int xl_camera_flash_frame(void)
{
	if (!active || !xl_level_extensions_active()) return 0;
	if (game_is_time_paused()) return 1;
	current.delay = Flash_effect;
	current.last_sound = Time_flash_last_played;
	const auto result = current.frame((std::max) (FrameTime, 0), GameTime64, [] { return d_rand_fx(); });
	if (result.sound) digi_play_sample(SOUND_CLOAK_OFF, *result.sound);
	publish();
	if (Newdemo_state == ND_STATE_RECORDING && result.demo)
		newdemo_record_palette_effect((*result.demo)[0], (*result.demo)[1], (*result.demo)[2]);
	gr_palette_step_up(PaletteRedAdd, PaletteGreenAdd, PaletteBlueAdd);
	if (!current.delay && !current.remaining) active = false;
	return 1;
}
extern "C" void xl_camera_flash_clear(void)
{
	current = {};
	if (active) publish();
	active = false;
}
nlohmann::json xl_camera_flash_snapshot()
{
	using nlohmann::json;
	if (!active) return nullptr;
	return json{ current.delay, current.last_sound ? json(current.last_sound - GameTime64) : json(nullptr), current.saved_color() };
}
bool xl_camera_flash_restore(const nlohmann::json &state, bool apply)
{
	try {
		xl_compat::CameraFlashPalette prepared;
		if (!state.is_null()) {
			if (!xl_level_extensions_active() || !save_epoch_valid(GameTime64) || !state.is_array() || state.size() != 3) return false;
			const auto integer = [](const nlohmann::json &value, std::int64_t low, std::int64_t high) {
				if (!value.is_number_integer() || (value.is_number_unsigned() && value.get<std::uint64_t>() > std::uint64_t(high))) throw std::runtime_error("Invalid camera flash integer");
				const auto number = value.get<std::int64_t>();
				if (number < low || number > high) throw std::runtime_error("Camera flash value outside bounds");
				return number;
			};
			prepared.delay = int(integer(state[0], 0, 4 * F1_0));
			if (!state[1].is_null()) {
				prepared.last_sound = GameTime64 + integer(state[1], -SAVE_EPOCH_MAX, SAVE_EPOCH_MAX);
				if (!save_timestamp_valid(prepared.last_sound)) return false;
			}
			if (!state[2].is_array() || state[2].size() != 3) return false;
			std::array<float, 3> color{};
			for (unsigned i = 0; i < 3; ++i) color[i] = float(integer(state[2][i], 0, 64)) / 64.0f;
			prepared.start(color);
		}
		if (apply) {
			current = prepared;
			active = !state.is_null();
			// A null custom state leaves the outer stock save's palette fields authoritative
			if (active) publish();
		}
		return true;
	} catch (const std::exception &) {
		return false;
	}
}
