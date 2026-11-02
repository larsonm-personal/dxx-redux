// Execute the unmodified pinned final-objective function against engine samples
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace reactor_reference
{
constexpr int OBJ_REACTOR = 9, OBJ_ROBOT = 2, TT_COUNTDOWN = 25, GM_MULTI_ROBOTS = 4;
struct CObject {
	struct {
		int nType = OBJ_REACTOR;
	} info;
	int Index() const
	{
		return 0;
	}
};
struct CTrigger {
};
struct {
	struct {
		bool bD2XLevel = true;
	} app;
	struct {
		bool bKillBossCheat = false, bMultiBosses = true;
		int nReactorCount[2] = { 1, 1 };
	} gameplay;
} gameStates;
struct {
	int nBossCount[2]{};
} extraGameInfo[1];
struct {
	struct {
		bool GameMode(int) const
		{
			return false;
		}
	} appData;
	struct {
		bool bDestroyed = false;
		struct {
			int m_nLinks = 1, m_segments[1]{}, m_sides[1]{};
		} triggers;
		struct {
			int nDeadObj = -1;
		} states[1];
	} reactorData;
} gameData;
struct {
	int nCurrentLevel = 1;
} missionManager;
struct {
	struct {
		const char *szSavegames = "";
	} user;
} gameFolders;
struct CFile {
	static void Delete(const char *, const char *) {}
};
int wall_toggles = 0, events = 0;
struct Segment {
	void ToggleWall(int)
	{
		++wall_toggles;
	}
} segment;
Segment *SEGMENT(int)
{
	return &segment;
}
void KillAllBossRobots(int) {}
CTrigger *FindObjTrigger(int, int, int)
{
	return nullptr;
}
void InitCountdown(CTrigger *, int, int)
{
	gameData.reactorData.bDestroyed = true;
}
void ExecObjTriggers(int, int)
{
	++events;
}
int FindReactor(CObject *)
{
	return 0;
}
#include "xl_reference_reactor_objectives.h"
} // namespace reactor_reference

void check_reactor_objectives(const std::filesystem::path &path)
{
	using namespace reactor_reference;
	std::ifstream file(path);
	int reactor, remaining, destroyed, cases = 0;
	while (file >> reactor >> remaining >> destroyed) {
		extraGameInfo[0].nBossCount[0] = remaining;
		gameData.reactorData.bDestroyed = false;
		wall_toggles = events = 0;
		CObject object;
		DoReactorDestroyedStuff(reactor ? &object : nullptr);
		if (gameData.reactorData.bDestroyed != bool(destroyed) || wall_toggles != int(!remaining) || events != reactor)
			throw std::runtime_error("Pinned reactor objective completion differs from native combat");
		++cases;
	}
	if (!file.eof() || cases != 6) throw std::runtime_error("Missing mixed objective samples");
	std::cout << "Pinned XL reactor objectives: both combat orders and intermediate disk restore matched\n";
}
