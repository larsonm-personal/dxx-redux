#include "../mission_flow.h"
#include <iostream>
#include <vector>
#include <array>
namespace mission_flow_reference
{
struct {
	struct {
		bool bD1Mission;
		bool bPlayerIsDead;
	} app;
} gameStates;
struct {
	struct {
		int nLevelVersion;
	} segData;
	struct {
		int m_nTriggers[1]{};
	} trigData;
} gameData;
std::array<int, 32> level_states{};
bool use_level_states = false;
struct CMissionManager {
	int nCurrentLevel = 0, nEntryLevel = 0, next = 0;
	int state = 0;
	void SetNextLevel(int value)
	{
		next = value;
	}
	void AdvanceLevel(int32_t nNextLevel);
	int NextLevel() const
	{
		return next;
	}
	int GetLevelState(int level) const
	{
		if (use_level_states) return level > 0 && level < int(level_states.size()) ? level_states[level] : 0;
		return state;
	}
} missionManager;
constexpr int N_LOCALPLAYER = 1, TF_DISABLED = 4, TF_PERMANENT = 8, TT_EXIT = 3;
struct CTrigger {
	int flags = 0;
	std::int32_t value = 0;
	int type = TT_EXIT;
	int Type() const
	{
		return type;
	}
	bool Flagged(int mask) const
	{
		return (flags & mask) != 0;
	}
	void SetFlags(int bits)
	{
		flags |= bits;
	}
	std::int32_t GetValue() const
	{
		return value;
	}
	bool DoExit(int32_t player);
} action;
struct GeometryTriggers {
	std::vector<CTrigger> actions;
	CTrigger *Buffer()
	{
		return actions.data();
	}
} GEOTRIGGERS;
std::vector<int> events;
struct Player {
	int nObject = 99, shield = 0;
	int Shield() const
	{
		return shield;
	}
} local;
#define PLAYER(i)   local
#define LOCALPLAYER local
int X2I(std::int32_t value)
{
	return value >= 0 ? value / 65536 : int((std::int64_t(value) - 65535) / 65536);
}
struct Audio {
	void StopAll()
	{
		events.push_back(1);
	}
} audio;
void StopSpeedBoost(int object)
{
	events.push_back(object);
}
void StartEndLevelSequence(int)
{
	events.push_back(2);
	events.push_back(action.flags);
}
void ExitSecretLevel()
{
	events.push_back(3);
}
#include "xl_reference_mission_flow.h"
#undef PLAYER
#undef LOCALPLAYER

struct World {
	bool is_d1, is_dead;
	int level, level_version, health, state, next, flags;
	std::vector<int> calls;
	int local_player() const
	{
		return N_LOCALPLAYER;
	}
	bool d1() const
	{
		return is_d1;
	}
	bool dead() const
	{
		return is_dead;
	}
	int current() const
	{
		return level;
	}
	int version() const
	{
		return level_version;
	}
	int shield() const
	{
		return health;
	}
	int destination_state() const
	{
		return state;
	}
	void stop_audio()
	{
		calls.push_back(1);
	}
	void stop_boost(int)
	{
		calls.push_back(99);
	}
	void advance(std::int32_t value)
	{
		next = xl_compat::mission_exit_destination(is_d1, level_version, level, 5, value, next);
	}
	void end_sequence()
	{
		calls.push_back(2);
		calls.push_back(flags);
	}
	void exit_secret()
	{
		calls.push_back(3);
	}
};
} // namespace mission_flow_reference

void mission_flow_source_controls()
{
	using namespace mission_flow_reference;
	unsigned cases = 0;
	for (bool d1 : { false, true })
		for (int version : { 15, 17, 20, 21, 23, 25, 27 })
			for (int current : { -3, -1, 0, 1, 11, 14 })
				for (int entry : { 1, 5, 14 })
					for (std::int32_t value : { INT32_MIN, -65537, -1, 0, 65535, 65536, 131072, 720896, 1900544, INT32_MAX })
						for (int previous : { -1, 0, 2, 11 }) {
							gameStates.app.bD1Mission = d1;
							gameData.segData.nLevelVersion = version;
							missionManager = { current, entry, previous };
							// Source X2I is an arithmetic shift, including negative fractions
							const int integer = value >= 0 ? value / 65536 : int((std::int64_t(value) - 65535) / 65536);
							missionManager.AdvanceLevel(integer);
							if (xl_compat::mission_exit_destination(d1, version, current, entry, value, previous) != missionManager.next)
								throw std::runtime_error("Authored exit destination differs from pinned source");
							++cases;
						}
	std::cout << cases << " pinned mission exit destination cases passed; transition persistence not covered\n";
	cases = 0;
	for (int flags = 0; flags < 256; ++flags)
		for (bool d1 : { false, true })
			for (int version : { 20, 21 })
				for (int current : { -1, 0, 1 })
					for (int state : { -1, 0, 1 })
						for (int health : { -1, 0, 1 })
							for (bool dead : { false, true })
								for (int player : { 0, N_LOCALPLAYER })
									for (std::int32_t value : { 0, 131072, 1900544 }) {
										gameStates.app = { d1, dead };
										gameData.segData.nLevelVersion = version;
										missionManager = { current, 5, 7, state };
										local.shield = health;
										action = { flags, value };
										events.clear();
										const bool expected = action.DoExit(player);
										World world{ d1, dead, current, version, health, state, 7, flags, {} };
										const bool actual = xl_compat::operate_mission_exit(world, player, world.flags, value);
										if (actual != expected || world.next != missionManager.next || world.flags != action.flags || world.calls != events)
											throw std::runtime_error("Exit activation or side-effect order differs from pinned source");
										++cases;
									}
	std::cout << cases << " pinned exit activation, flags and side-effect sequences passed\n";
	cases = 0;
	use_level_states = true;
	for (int version : { 20, 21, 27 })
		for (int current : { -1, 0, 1, 14 })
			for (int first : { -1, 0, 1 })
				for (int second : { -1, 0, 1 })
					for (int third : { -1, 0, 1 })
						for (int mask = 0; mask < 32; ++mask) {
							level_states.fill(0);
							level_states[2] = first;
							level_states[11] = second;
							level_states[29] = third;
							GEOTRIGGERS.actions.clear();
							const std::array<int, 5> values{ 1900544, 131072, 720896, 0, -1 };
							for (int i = 0; i < 5; ++i)
								GEOTRIGGERS.actions.push_back({ 4, values[i], mask & (1 << i) ? TT_EXIT : 0 });
							gameData.trigData.m_nTriggers[0] = int(GEOTRIGGERS.actions.size());
							gameData.segData.nLevelVersion = version;
							missionManager.nCurrentLevel = current;
							const auto expected = FindNextLevel();
							const auto actual = xl_compat::mission_death_destination(version, current, GEOTRIGGERS.actions, [](int level) { return missionManager.GetLevelState(level); });
							if (actual != expected) throw std::runtime_error("Reactor-death destination differs from pinned source");
							++cases;
						}
	use_level_states = false;
	std::cout << cases << " pinned reactor-death exit selection cases passed\n";
}
