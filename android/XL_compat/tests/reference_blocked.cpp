#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
constexpr int WID_VISIBLE_FLAG = 2, WID_EXTERNAL_FLAG = 8, WID_PASSABLE_FLAG = 1, WID_TRANSPARENT_FLAG = 4;
constexpr int OBJ_PLAYER = 4, OBJ_ROBOT = 2, OBJ_POWERUP = 7, SEGMENT_FUNC_SPEEDBOOST = 9, TT_SPEEDBOOST = 15;
struct CObject {
	struct {
		int nType = 0;
	} info;
	int index = 0;
	int Index() const
	{
		return index;
	}
} object;
struct CTrigger {
	struct {
		int nType = 0;
	} m_info;
} action;
struct CWall {
	int flags = 0;
	bool has_trigger = false;
	int IsPassable(CObject *, bool)
	{
		return flags;
	}
	CTrigger *Trigger()
	{
		return has_trigger ? &action : nullptr;
	}
} wall;
struct Side {
	CWall *wall = nullptr;
	CWall *Wall()
	{
		return wall;
	}
	int FaceCount()
	{
		return 1;
	}
};
struct CSegment {
	int m_children[6]{ 1, 1, 1, 1, 1, 1 };
	Side m_sides[6];
	std::uint8_t m_function = 0;
	bool blocked = false;
	bool HasBlockedProp()
	{
		return blocked;
	}
	int32_t IsPassable(int32_t, CObject *, bool);
} segments[2];
CSegment *SEGMENT(int index)
{
	return &segments[index];
}
struct {
	struct {
		CObject *pConsole = &object;
		struct {
			int bBoosted = 0;
		} speedBoost[2];
	} objData;
} gameData;
#include "xl_reference_blocked.h"
int main(int argc, char **argv)
{
	try {
		if (argc != 2) throw std::runtime_error("Expected blocked passability samples");
		std::ifstream input(argv[1]);
		int bits, type, wall_flags, has_wall, expected, count = 0;
		while (input >> bits >> type >> wall_flags >> has_wall >> expected) {
			object.info.nType = type;
			segments[1].blocked = bits & 4;
			wall.flags = wall_flags;
			segments[0].m_sides[0].wall = has_wall ? &wall : nullptr;
			const int actual = segments[0].IsPassable(0, type < 0 ? nullptr : &object, false);
			if (actual != expected) throw std::runtime_error("Blocked passability differs at sample " + std::to_string(count) + " source " + std::to_string(actual) + " runtime " + std::to_string(expected));
			++count;
		}
		if (count < 500) throw std::runtime_error("Insufficient blocked samples");
		std::cout << count << " extracted-reference blocked passability samples passed\n";
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
