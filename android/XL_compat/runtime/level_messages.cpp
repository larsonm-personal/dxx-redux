#include "level_messages.h"
#include "preview.h"
#include "../level_text.h"
#include "../message_time.h"
#include "../inventory.h"
#include "picosha2.h"
#include <algorithm>
#include <chrono>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>

namespace xl_compat
{
struct MessageResources {
	LevelTextTable table;
	std::string identity;
	std::set<std::pair<std::int32_t, std::int32_t>> authored;
};
} // namespace xl_compat

extern "C" {
#include "inferno.h"
#include "physfsx.h"
#include "mission.h"
#include "switch.h"
#include "game.h"
#include "gr.h"
#include "gamefont.h"
#include "newmenu.h"
#include "text.h"
}

namespace
{
std::shared_ptr<const xl_compat::MessageResources> bank;
int message_id = 0, duration = 0;
std::int64_t started = 0, frame_clock = 0;
std::uint64_t draws = 0, modals = 0;
int drawn_id = 0;
float drawn_alpha = 0;
std::int64_t milliseconds()
{
	// Like the source SDL clock, this advances while game simulation is paused
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
void clear_message()
{
	message_id = duration = 0;
	started = 0;
}
std::int64_t age()
{
	if (!message_id) return 0;
	const auto elapsed = (std::max) (std::int64_t(0), frame_clock - started);
	if (elapsed >= std::int64_t(duration) * 1000) {
		clear_message();
		return 0;
	}
	return elapsed;
}
} // namespace

namespace xl_compat
{
std::shared_ptr<const MessageResources> prepare_messages(const char *level, const Inventory &inventory)
{
	auto resources = std::make_shared<MessageResources>();
	const auto action = [&](const Trigger &entry) {
		if (entry.type != 29) return;
		const int id = entry.value / 65536;
		if (id <= 0) throw std::runtime_error("XL message action requires a positive text ID");
		resources->authored.emplace(id, entry.time);
	};
	for (const auto &entry : inventory.triggers) action(entry);
	for (const auto &entry : inventory.object_trigger_records) action(entry.action);
	if (resources->authored.empty()) return {};
	std::string name = level ? level : "";
	if (name.empty() || name.find_first_of("/\\:") != std::string::npos || name.find("..") != std::string::npos)
		throw std::runtime_error("Invalid XL message level identity");
	const auto dot = name.find_last_of('.');
	if (dot == std::string::npos) throw std::runtime_error("XL message level has no filename extension");
	name.resize(dot);
	name += ".msg";
	if (PHYSFSEXT_locateCorrectCase(name.data()) != 0) throw std::runtime_error("Missing XL message table: " + name);
	if (!mission_owns_resource_archive(PHYSFS_getRealDir(name.c_str())))
		throw std::runtime_error("XL message table is shadowed outside the selected mission HOG: " + name);
	std::unique_ptr<PHYSFS_file, decltype(&PHYSFS_close)> file(PHYSFS_openRead(name.c_str()), PHYSFS_close);
	const auto length = file ? PHYSFS_fileLength(file.get()) : -1;
	if (length < 0 || length > 1024 * 1024) throw std::runtime_error("Cannot read bounded XL message table: " + name);
	std::string bytes(static_cast<std::size_t>(length), '\0');
	if (PHYSFS_readBytes(file.get(), bytes.data(), bytes.size()) != length) throw std::runtime_error("Incomplete XL message table: " + name);
	resources->table = decode_level_text(bytes);
	resources->identity = picosha2::hash256_hex_string(bytes);
	for (const auto &entry : resources->authored)
		if (!find_level_text(resources->table, entry.first))
			throw std::runtime_error("XL message ID " + std::to_string(entry.first) + " is missing from " + name);
	return resources;
}

void install_messages(std::shared_ptr<const MessageResources> resources)
{
	clear_message();
	frame_clock = milliseconds();
	bank = std::move(resources);
}
} // namespace xl_compat

extern "C" void xl_message_clear(void)
{
	clear_message();
	bank.reset();
	drawn_id = 0;
	drawn_alpha = 0;
}

extern "C" int xl_message_action(const trigger *action)
{
	if (!xl_level_extensions_active() || !action || action->type != 29) return 0;
	xl_message_frame();
	if (!bank) return 1;
	const int id = action->value / F1_0;
	const auto *row = xl_compat::find_level_text(bank->table, id);
	if (!row) return 1;
	clear_message();
	if (action->time < 0) {
		auto text = row->text;
		++modals;
		struct Paused {
			Paused()
			{
				stop_time();
			}
			~Paused()
			{
				start_time();
			}
		} paused;
		newmenu_item close{};
		close.type = NM_TYPE_MENU;
		close.text = TXT_OK;
		// MSG text is owned and bounded; avoid the fixed messagebox printf buffer
		newmenu_do(nullptr, text.data(), 1, &close, nullptr, nullptr);
	} else {
		message_id = id;
		duration = action->time;
		started = frame_clock;
	}
	return 1;
}

extern "C" const char *xl_message_identity(void)
{
	return bank ? bank->identity.c_str() : "";
}

extern "C" const char *xl_message_text(void)
{
	age();
	const auto *row = bank && message_id ? xl_compat::find_level_text(bank->table, message_id) : nullptr;
	return row ? row->text.c_str() : "";
}

extern "C" float xl_message_alpha(void)
{
	const auto elapsed = age();
	return message_id ? xl_compat::message_opacity(elapsed, std::int64_t(duration) * 1000 - elapsed) : 0;
}

extern "C" void xl_message_state_get(xl_message_state *state)
{
	if (!state) return;
	const auto elapsed = age();
	*state = { elapsed, message_id, duration };
}

extern "C" int xl_message_restore(const xl_message_state *state, int apply)
{
	if (!state || state->elapsed_ms < 0 || state->duration < 0 || state->id < 0) return 0;
	if (!state->id) {
		if (state->elapsed_ms || state->duration) return 0;
	} else if (!bank || !bank->authored.count({ state->id, state->duration }) || state->elapsed_ms >= std::int64_t(state->duration) * 1000) return 0;
	if (apply) {
		frame_clock = milliseconds();
		message_id = state->id;
		duration = state->duration;
		started = message_id ? frame_clock - state->elapsed_ms : 0;
	}
	return 1;
}

extern "C" void xl_message_frame(void)
{
	// XL samples SDL ticks before its frame. Serializers read that frame's state;
	// resampling inside a write would give detached and synchronous saves different clocks
	frame_clock = milliseconds();
	age();
}

extern "C" void xl_message_draw(void)
{
	drawn_id = 0;
	drawn_alpha = 0;
	if (!xl_level_extensions_active() || !grd_curcanv) return;
	xl_message_frame();
	const char *text = xl_message_text();
	const float alpha = xl_message_alpha();
	if (!*text || alpha <= 0) return;
	const auto saved = *grd_curcanv;
	gr_set_curfont(GAME_FONT);
	int width, height, average;
	gr_get_string_size(text, &width, &height, &average);
	const int x = (grd_curcanv->cv_bitmap.bm_w - width) / 2;
	const int y = (grd_curcanv->cv_bitmap.bm_h - height) * 2 / 5;
	gr_settransblend(static_cast<int>((1 - 0.75f * alpha) * (GR_FADE_LEVELS - 1)), GR_BLEND_NORMAL);
	gr_setcolor(BM_XRGB(0, 0, 0));
	gr_rect(x - 8, y - 8, x + width + 8, y + height + 8);
	gr_settransblend(static_cast<int>((1 - alpha) * (GR_FADE_LEVELS - 1)), GR_BLEND_NORMAL);
	gr_set_fontcolor(BM_XRGB(31, 24, 0), -1);
	gr_string(0x8000, y, text);
	gr_set_curfont(saved.cv_font);
	gr_set_fontcolor(saved.cv_font_fg_color, saved.cv_font_bg_color);
	gr_setcolor(saved.cv_color);
	gr_settransblend(saved.cv_fade_level, saved.cv_blend_func);
	++draws;
	drawn_id = message_id;
	drawn_alpha = alpha;
}

extern "C" std::uint64_t xl_message_draws(void)
{
	return draws;
}
extern "C" std::uint64_t xl_message_modal_shows(void)
{
	return modals;
}
extern "C" int xl_message_drawn_id(void)
{
	return drawn_id;
}
extern "C" float xl_message_drawn_alpha(void)
{
	return drawn_alpha;
}
