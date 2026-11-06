#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <vector>

namespace starts_reference
{
constexpr int MAX_PLAYERS = 16, MAX_COOP_PLAYERS = 3;
constexpr int OBJ_PLAYER = 4, OBJ_GHOST = 12, OBJ_COOP = 14, OBJ_ROBOT = 2, SEGMENT_FUNC_NONE = 0;
constexpr bool IsCoopGame = false, IsMultiGame = false;
constexpr int N_PLAYERS = 1;
struct CObject {
	struct {
		int nType = 0, nSegment = 0, nId = 0, position = 0;
	} info;
	int index = 0;
	bool alive = true;
	int Index() const
	{
		return index;
	}
	bool IsGuideBot() const
	{
		return false;
	}
	void SetType(int type)
	{
		info.nType = type;
	}
};
std::vector<CObject> objects;
std::vector<int> deleted;
struct Store {
	CObject *Buffer()
	{
		return objects.data();
	}
} OBJECTS;
CObject *OBJECT(int index)
{
	return &objects.at(index);
}
struct Player {
	int nObject = -1, connected = 0;
	void SetObject(int index)
	{
		nObject = index;
	}
} players[16];
#define PLAYER(i) players[i]
struct {
	struct {
		bool bPlayerIsTyping[16];
	} multi;
} gameStates;
struct {
	struct {
		bool bAdjustPowerupCap[16];
		struct {
			int position = 0, nSegment = 0, nSegType = 0;
		} playerInit[16];
		int nPlayerPositions = 0;
	} multiplayer;
	struct {
		CObject *pConsole = nullptr;
	} objData;
	void SetViewer(CObject *) {}
} gameData;
struct Segment {
	int m_function = 0;
} segment;
Segment *SEGMENT(int)
{
	return &segment;
}
void ReleaseObject(int index)
{
	if (!objects.at(index).alive || index == 0) throw std::runtime_error("Source deleted invalid/player-zero object");
	objects[index].alive = false;
	deleted.push_back(index);
}
void MultiTurnPlayerToGhost(int)
{
	throw std::runtime_error("Singleplayer source entered network cleanup");
}
struct CObjectIterator {
	int cursor = 0;
	explicit CObjectIterator(CObject *&out)
	{
		out = Start();
	}
	CObject *Start()
	{
		cursor = 0;
		return seek();
	}
	CObject *seek()
	{
		while (cursor < int(objects.size()) && !objects[cursor].alive) ++cursor;
		return cursor < int(objects.size()) ? &objects[cursor] : nullptr;
	}
	CObject *Step()
	{
		++cursor;
		return seek();
	}
	CObject *Back()
	{
		do {
			--cursor;
		} while (cursor >= 0 && !objects[cursor].alive);
		return cursor >= 0 ? &objects[cursor] : nullptr;
	}
};
#include "xl_reference_player_starts.h"
#undef PLAYER
} // namespace starts_reference
void compare_player_starts(const char *path)
{
	using namespace starts_reference;
	std::ifstream file(path);
	const auto read = [&] { int value; if (!(file >> value)) throw std::runtime_error("Truncated player-start samples"); return value; };
	if (read() != 8) throw std::runtime_error("Incomplete player-start cohort");
	std::set<int> cases;
	for (int test = 0; test < 8; ++test) {
		const int count = read(), ghosts = read(), size = read();
		if ((count != 1 && count != 8 && count != 9 && count != 16) || (ghosts != 0 && ghosts != 1) || size < 16 || size > 10000 || !cases.insert(count * 2 + ghosts).second)
			throw std::runtime_error("Invalid player-start sample profile");
		objects.assign(size, {});
		deleted.clear();
		for (int i = 0; i < size; ++i) {
			objects[i].index = i;
			objects[i].info.nType = read();
			objects[i].info.position = i;
		}
		GameStartInitNetworkPlayers();
		if (gameData.multiplayer.nPlayerPositions != count || players[0].nObject != 0 || gameData.multiplayer.playerInit[0].position != 0)
			throw std::runtime_error("Source selected a different player start");
		GameStartRemoveUnusedPlayers();
		const int removed = read();
		if (removed != int(deleted.size())) throw std::runtime_error("Native/source removed count differs");
		for (int id : deleted)
			if (id != read()) throw std::runtime_error("Native/source deletion and allocator order differs");
		for (const auto &object : objects)
			if (object.alive && (object.info.nType == OBJ_PLAYER || object.info.nType == OBJ_GHOST || object.info.nType == OBJ_COOP) && object.index != 0)
				throw std::runtime_error("Source retained an unused start");
	}
	std::string extra;
	if (file >> extra) throw std::runtime_error("Unexpected player-start evidence");
	std::cout << "Pinned XL selection/cleanup matched eight native count/ghost/coop/allocator cases\n";
}
