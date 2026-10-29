#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
constexpr int WALL_OPEN = 4, WALL_ILLUSION = 3, WALL_BLASTABLE = 1, WALL_CLOAKED = 7, WALL_COLORED = 8, WALL_DOOR = 2, WALL_CLOSED = 5;
constexpr int WALL_ILLUSION_OFF = 32, WALL_BLASTED = 1, WALL_DOOR_OPENED = 2, WALL_DOOR_OPENING = 1, WALL_DOOR_CLOSING = 3;
constexpr int WID_NO_WALL = 5, WID_TRANSILLUSORY_WALL = 7, WID_ILLUSORY_WALL = 3, WID_TRANSPARENT_WALL = 6, WID_SOLID_WALL = 2, WID_CLOAKED_FLAG = 16, WID_TRANSPCOLOR_FLAG = 32;
constexpr int FADE_LEVELS = 31, OBJ_PLAYER = 4, KEY_BLUE = 2, KEY_RED = 4, PLAYER_FLAGS_BLUE_KEY = 2, PLAYER_FLAGS_RED_KEY = 4;
constexpr int OBJ_ROBOT = 2, OBJ_WEAPON = 5, KEY_GOLD = 8, PLAYER_FLAGS_GOLD_KEY = 8, WALL_NORMAL = 0, WALL_DOOR_LOCKED = 8, WALL_DOOR_CLOSED = 0;
constexpr int WHP_NOT_SPECIAL = 0, WHP_NO_KEY = 1, WHP_DOOR = 3, N_LOCALPLAYER = 0;
constexpr bool IsMultiGame = false;
constexpr const char *TXT_BLUE = "blue", *TXT_RED = "red", *TXT_YELLOW = "yellow", *TXT_ACCESS_DENIED = "denied", *TXT_CANT_OPEN_DOOR = "locked";
struct CFixVector {
	static int Dot(const CFixVector &, const CFixVector &)
	{
		return 0;
	}
};
void HUDInitMessage(const char *, ...) {}
void MultiSendDoorOpen(int, int, int)
{
	throw std::runtime_error("Unexpected multiplayer wall action");
}
bool AllowToOpenSpecialBossDoor(int, int)
{
	return false;
}
constexpr bool IsTeamGame = false;
constexpr int KEY_NONE = 1, WALL_BUDDY_PROOF = 128, ROBOT_BRAIN = 7, AIB_RUN_FROM = 0x83, AIB_SNIPE = 0x84, AIM_GOTO_PLAYER = 8, WCF_HIDDEN = 8;
#define ENTER(a, b)
#define RETVAL(value)   return value;
#define IS_CHILD(value) ((value) >= 0)
struct CObject {
	struct {
		int nType = 0, nId = 0;
		struct {
			struct {
				struct {
					struct {
						CFixVector f;
					} dir;
				} m;
			} mOrient;
		} position;
	} info;
	struct {
		struct {
			CFixVector velocity;
		} physInfo;
	} mType;
	struct {
		struct {
			struct {
				int nType = OBJ_PLAYER;
			} parent;
		} laserInfo;
		struct {
			int behavior = 0;
		} aiInfo;
	} cType;
	bool IsGuideBot() const
	{
		return false;
	}
	int Index() const
	{
		return 0;
	}
} actor;
struct {
	int flags = 0;
} players[1];
#define PLAYER(i)   players[i]
#define LOCALPLAYER players[0]
struct {
	struct {
		CObject *pConsole = nullptr;
	} objData;
	struct {
		struct {
			int mode = 0;
		} localInfo[1];
	} aiData;
	struct {
		int nObjNum = -1;
	} escortData;
	struct {
		struct {
			int flags = 0;
		} pAnim[1];
	} wallData;
} gameData;
struct CWall;
int GetTeam(int)
{
	return 0;
}
struct Segment {
	bool transparent = false;
	int m_children[6]{ 1, 1, 1, 1, 1, 1 };
	CWall *Wall(int);
	bool CheckTransparency(int)
	{
		return transparent;
	}
	void OpenDoor(int)
	{
		throw std::runtime_error("Source decision matrix unexpectedly animates a door");
	}
} segment;
using CSegment = Segment;
Segment *SEGMENT(int)
{
	return &segment;
}
struct CWall {
	int nType = WALL_CLOSED, nSegment = 0, nSide = 0, flags = 0, hps = 0, state = 0, keys = 0;
	int controllingTrigger = -1, nClip = 0;
	std::int8_t cloakValue = 0;
	int32_t IsPassable(CObject *, bool);
	int32_t ProcessHit(int32_t, CObject *);
} wall;
CWall *Segment::Wall(int)
{
	return &wall;
}
#include "xl_reference_keyed_wall.h"
int main(int argc, char **argv)
{
	try {
		if (argc == 3 && std::string(argv[2]) == "keys-robot") {
			std::ifstream in(argv[1]);
			int keys, owned, locked, expected, count = 0;
			actor.info.nType = OBJ_ROBOT;
			actor.info.nId = ROBOT_BRAIN;
			wall.nType = WALL_DOOR;
			while (in >> keys >> owned >> locked >> expected) {
				wall.keys = keys;
				players[0].flags = owned;
				wall.flags = locked ? WALL_DOOR_LOCKED : 0;
				if (bool(AIDoorIsOpenable(&actor, &segment, 0)) != bool(expected)) throw std::runtime_error("Raw-key robot mismatch at " + std::to_string(count));
				++count;
			}
			if (count != 6144) throw std::runtime_error("Incomplete raw-key robot matrix");
			std::cout << count << " extracted-reference raw-key robot door decisions passed\n";
			return 0;
		}
		if (argc == 3 && std::string(argv[2]) == "keys-hit") {
			std::ifstream in(argv[1]);
			int keys, owned, profile, expected, count = 0;
			actor.info.nType = OBJ_PLAYER;
			while (in >> keys >> owned >> profile >> expected) {
				wall.keys = keys;
				players[0].flags = owned;
				wall.nType = profile ? WALL_DOOR : WALL_NORMAL;
				wall.flags = profile == 1 ? WALL_DOOR_LOCKED : 0;
				wall.state = profile == 2 ? WALL_DOOR_OPENING : WALL_DOOR_CLOSED;
				if (wall.ProcessHit(0, &actor) != expected) throw std::runtime_error("Raw-key hit mismatch at " + std::to_string(count));
				++count;
			}
			if (count != 6144) throw std::runtime_error("Incomplete raw-key hit matrix");
			std::cout << count << " extracted-reference wall-key hit decisions passed\n";
			return 0;
		}
		if (argc == 3 && std::string(argv[2]) == "doorway") {
			std::ifstream in(argv[1]);
			int type, flags, state, cloak, transparent, expected, count = 0;
			while (in >> type >> flags >> state >> cloak >> transparent >> expected) {
				wall.nType = type;
				wall.flags = flags;
				wall.state = state;
				wall.cloakValue = cloak;
				segment.transparent = transparent != 0;
				if (wall.IsPassable(nullptr, false) != expected)
					throw std::runtime_error("Doorway mismatch at " + std::to_string(count));
				++count;
			}
			if (count <= 8192) throw std::runtime_error("Incomplete doorway matrix/animation");
			std::cout << count << " extracted-reference doorway matrix/animation samples passed\n";
			return 0;
		}
		if (argc != 2) throw std::runtime_error("Expected keyed-wall samples");
		std::ifstream in(argv[1]);
		int type, keys, owned, cloak, transparent, flags, expected, count = 0;
		while (in >> type >> keys >> owned >> cloak >> transparent >> flags >> expected) {
			actor.info.nType = type;
			players[0].flags = owned;
			wall.keys = keys;
			wall.cloakValue = cloak;
			wall.flags = flags;
			segment.transparent = transparent != 0;
			const int actual = wall.IsPassable(type < 0 ? nullptr : &actor, false);
			if (actual != expected) throw std::runtime_error("Keyed-wall mismatch at " + std::to_string(count));
			++count;
		}
		if (count != 72576) throw std::runtime_error("Incomplete keyed-wall matrix");
		std::cout << count << " extracted-reference keyed-wall samples passed\n";
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
