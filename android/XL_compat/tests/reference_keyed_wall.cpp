#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
constexpr int WALL_OPEN = 4, WALL_ILLUSION = 3, WALL_BLASTABLE = 1, WALL_CLOAKED = 7, WALL_COLORED = 8, WALL_DOOR = 2, WALL_CLOSED = 5;
constexpr int WALL_ILLUSION_OFF = 32, WALL_BLASTED = 1, WALL_DOOR_OPENED = 2, WALL_DOOR_OPENING = 1, WALL_DOOR_CLOSING = 3;
constexpr int WID_NO_WALL = 5, WID_TRANSILLUSORY_WALL = 7, WID_ILLUSORY_WALL = 3, WID_TRANSPARENT_WALL = 6, WID_SOLID_WALL = 2, WID_CLOAKED_FLAG = 16, WID_TRANSPCOLOR_FLAG = 32;
constexpr int FADE_LEVELS = 31, OBJ_PLAYER = 4, KEY_BLUE = 2, KEY_RED = 4, PLAYER_FLAGS_BLUE_KEY = 2, PLAYER_FLAGS_RED_KEY = 4;
constexpr bool IsTeamGame = false;
struct CObject {
	struct {
		int nType = 0, nId = 0;
	} info;
} actor;
struct {
	int flags = 0;
} players[1];
#define PLAYER(i) players[i]
int GetTeam(int)
{
	return 0;
}
struct Segment {
	bool transparent = false;
	bool CheckTransparency(int)
	{
		return transparent;
	}
} segment;
Segment *SEGMENT(int)
{
	return &segment;
}
struct CWall {
	int nType = WALL_CLOSED, nSegment = 0, nSide = 0, flags = 0, hps = 0, state = 0, keys = 0;
	std::int8_t cloakValue = 0;
	int32_t IsPassable(CObject *, bool);
} wall;
#include "xl_reference_keyed_wall.h"
int main(int argc, char **argv)
{
	try {
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
		if (count != 55296) throw std::runtime_error("Incomplete keyed-wall matrix");
		std::cout << count << " extracted-reference keyed-wall samples passed\n";
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
