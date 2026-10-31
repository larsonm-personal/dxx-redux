#include "mission_progress.h"
#include "preview.h"
#include "../mission_flow.h"
#include <string>
#include <vector>
#include <stdexcept>
extern "C" {
#include "inferno.h"
#include "mission.h"
#include "gameseq.h"
#include "state.h"
#include "game.h"
#include "object.h"
#include "switch.h"
#include "cntrlcen.h"
#include "weapon.h"
#include "physfsx.h"
#include "newdemo.h"
#include "digi.h"
#include "args.h"
void InitPlayerPosition(int random_flag);
}

namespace
{
std::string mission;
std::vector<int> states;
bool cache_restore = false;
std::string failure;
std::string cache_path(int level)
{
	const auto encode = [](const char *text) {
		std::string result;
		for (; *text; ++text) {
			const auto byte = static_cast<unsigned char>(*text);
			result += "0123456789abcdef"[byte >> 4];
			result += "0123456789abcdef"[byte & 15];
		}
		return result;
	};
	return std::string(GameArg.SysUsePlayersDir ? "Players/" : "") + "xl-level-" + encode(Players[Player_num].callsign) + "-" + encode(Current_mission_filename) + "-" + std::to_string(level) + ".sav";
}
bool bound()
{
	return Current_mission && mission == Current_mission_filename && Last_level > 0 && states.size() == std::size_t(Last_level) + 1;
}
} // namespace
extern "C" void xl_mission_progress_new_game(void)
{
	mission.clear();
	states.clear();
	cache_restore = false;
}
void xl_mission_progress_bind(int ordinal)
{
	if (!Current_mission || Last_level < 1 || Last_level > MAX_LEVELS_PER_MISSION)
		throw std::runtime_error("XL progression requires a bounded selected mission");
	if (!bound()) {
		mission = Current_mission_filename;
		states.assign(std::size_t(Last_level) + 1, 0);
	}
	// LoadLevel publishes Current_level_num only after the XL loader returns
	const int loaded = ordinal ? ordinal : Current_level_num;
	if (loaded >= 1 && loaded <= Last_level) states[loaded] = 1;
}
extern "C" int xl_mission_progress_state(int level)
{
	return bound() && level >= 1 && level <= Last_level ? states[level] : 0;
}
void xl_mission_progress_mark(int level, int state)
{
	if (state < -1 || state > 1) throw std::runtime_error("Invalid XL mission level state");
	if (bound() && level >= 1 && level <= Last_level) states[level] = state;
}
extern "C" void xl_mission_progress_cache_restore(int enabled)
{
	cache_restore = enabled != 0;
}
extern "C" int xl_mission_progress_active(void)
{
	return xl_level_extensions_active() && xl_source_level_version() > 20 && !(Game_mode & GM_MULTI) && Current_mission && !EMULATING_D1 && Current_level_num > 0;
}
extern "C" const char *xl_mission_progress_error(void)
{
	return failure.c_str();
}
extern "C" void xl_mission_progress_find_exit(void)
{
	if (!xl_mission_progress_active()) return;
	struct Range {
		const trigger *begin() const
		{
			return Triggers;
		}
		const trigger *end() const
		{
			return Triggers + Num_triggers;
		}
	};
	Next_level_num = xl_compat::mission_death_destination(xl_source_level_version(), Current_level_num, Range{}, xl_mission_progress_state);
}
extern "C" int xl_mission_progress_prepare_advance(void)
{
	if (!xl_mission_progress_active()) return 0;
	failure.clear();
	if (Control_center_destroyed) xl_mission_progress_mark(Current_level_num, -1);
	if (Next_level_num < 1) Next_level_num = Current_level_num + 1;
	if (Next_level_num > Last_level || xl_mission_progress_state(Next_level_num) < 0) return 1;
	if (Newdemo_state == ND_STATE_RECORDING || Newdemo_state == ND_STATE_PAUSED) newdemo_stop_recording(0);
	auto path = cache_path(Current_level_num);
	if (Control_center_destroyed) {
		if (PHYSFSX_exists(path.c_str(), 0)) PHYSFS_delete(path.c_str());
		return 1;
	}
	char description[64] = "XL level revisit";
	stop_time();
	if (!state_save_all_sub(path.data(), description)) {
		failure = "Cannot save the departing XL level for a revisit";
		return -1;
	}
	return 1;
}
extern "C" int xl_mission_progress_reenter(void)
{
	const int destination = Next_level_num;
	if (xl_mission_progress_state(destination) <= 0) return 0;
	auto path = cache_path(destination);
	if (!PHYSFSX_exists(path.c_str(), 0)) return 0;
	const auto primary = Players[Player_num].primary_weapon, secondary = Players[Player_num].secondary_weapon;
	struct CacheRestore {
		CacheRestore()
		{
			cache_restore = true;
		}
		~CacheRestore()
		{
			cache_restore = false;
		}
	} scope;
	if (!state_restore_all_sub(path.data(), 0)) {
		failure = "Cannot restore the revisited XL level";
		return -1;
	}
	InitPlayerPosition(1);
	fly_init(ConsoleObject);
	Players[Player_num].primary_weapon = primary;
	Players[Player_num].secondary_weapon = secondary;
	Current_level_num = destination;
	Next_level_num = destination;
	return 1;
}
nlohmann::json xl_compat::mission_progress_snapshot()
{
	if (!bound()) throw std::runtime_error("XL progression owner does not match the loaded mission");
	return { { "mission", mission }, { "states", states }, { "next", Next_level_num } };
}
bool xl_compat::restore_mission_progress(const nlohmann::json &saved, bool apply)
{
	try {
		if (!bound() || !saved.is_object() || saved.size() != 3 || saved.at("mission") != mission || !saved.at("states").is_array() || saved.at("states").size() != states.size() || !saved.at("next").is_number_integer()) return false;
		const auto next = saved.at("next").get<std::int64_t>();
		if (next < -32768 || next > 32767) return false;
		std::vector<int> restored;
		for (const auto &value : saved.at("states")) {
			if (!value.is_number_integer()) return false;
			const auto state = value.get<std::int64_t>();
			if (state < -1 || state > 1) return false;
			restored.push_back(int(state));
		}
		if (restored[0]) return false;
		// Revisit caches restore their world, never an older campaign timeline
		if (apply && !cache_restore) {
			states = std::move(restored);
			Next_level_num = int(next);
		}
		return true;
	} catch (...) {
		return false;
	}
}

void xl_compat::mission_progress_native_controls()
{
	const auto earned = mission_progress_snapshot();
	struct Restore {
		nlohmann::json saved;
		~Restore()
		{
			cache_restore = false;
			restore_mission_progress(saved, true);
		}
	} restore{ earned };
	auto changed = earned;
	changed["states"][1] = -1;
	changed["next"] = 29;
	if (!restore_mission_progress(changed, true) || mission_progress_snapshot() != changed)
		throw std::runtime_error("Mission progress round trip differs");
	cache_restore = true;
	if (!restore_mission_progress(earned, true) || mission_progress_snapshot() != changed)
		throw std::runtime_error("Level revisit rolled back mission progress");
	cache_restore = false;
	for (int malformed = 0; malformed < 3; ++malformed) {
		auto bad = earned;
		if (malformed == 0) bad["states"][1] = 2;
		if (malformed == 1) bad["mission"] = "unrelated";
		if (malformed == 2) bad["next"] = 32768;
		if (restore_mission_progress(bad, true) || mission_progress_snapshot() != changed)
			throw std::runtime_error("Invalid progression state changed the live campaign");
	}
}
