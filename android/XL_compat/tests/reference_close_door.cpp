#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
using fix = int32_t;
constexpr int WALL_DOOR_CLOSED = 0, WALL_DOOR_OPENING = 1, WALL_DOOR_WAITING = 2, WALL_DOOR_CLOSING = 3, WALL_DOOR_OPEN = 4;
constexpr int ND_STATE_RECORDING = 1, ND_STATE_PLAYBACK = 2, NO_WALL = -1;
#define ENTER(a, b)
#define RETURN     return;
#define IS_WALL(i) ((i) >= 0 && (i) < 2)
struct CActiveDoor {
	int nPartCount = 0, nFrontWall[2]{ -1, -1 }, nBackWall[2]{ -1, -1 };
	fix time = 0;
};
struct DoorArray {
	CActiveDoor data[90]{};
	unsigned count = 0;
	bool full = false;
	bool Grow()
	{
		if (full) return false;
		++count;
		return true;
	}
	CActiveDoor *Top()
	{
		return &data[count - 1];
	}
	CActiveDoor *Buffer()
	{
		return data;
	}
	unsigned ToS() const
	{
		return count;
	}
};
struct {
	struct {
		DoorArray activeDoors;
		struct {
			fix xTotalTime = 0;
			int openSound = -1;
		} pAnim[1];
	} wallData;
	struct {
		int nState = 0;
	} demoData;
} gameData;
struct {
	struct {
		bool bD2XLevel = true;
	} app;
	struct {
		struct {
			float fSpeed = 1;
		} slowmo[1];
	} gameplay;
} gameStates;
struct {
	void Update() {}
} audio;
struct CWall {
	int state = 0, nClip = 0;
	bool IgnoreMarker() const
	{
		return false;
	}
	CActiveDoor *CloseDoor(bool);
} walls[2];
#define WALLS walls
CWall *WALL(int index)
{
	return &walls[index];
}
bool obstructed = false;
struct CSegment {
	int index = 0, m_children[6]{ 1, 1, 1, 1, 1, 1 };
	CWall *Wall(int)
	{
		return &walls[index];
	}
	bool DoorIsBlocked(int, bool)
	{
		return obstructed;
	}
	int ConnectedSide(CSegment *)
	{
		return 0;
	}
	int WallNum(int)
	{
		return index;
	}
	void CreateSound(int, int) {}
	void CloseDoor(int32_t, bool);
} segments[2];
CSegment *SEGMENT(int i)
{
	return i >= 0 && i < 2 ? &segments[i] : nullptr;
}
int SEG_IDX(CSegment *s)
{
	return s->index;
}
void NDRecordDoorOpening(int, int) {}
#if defined(__GNUC__)
#pragma GCC diagnostic push
// Preserve the pinned caller's misleading indentation without suppressing adapter warnings
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#endif
#include "xl_reference_close_door.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
int main(int argc, char **argv)
{
	try {
		if (argc != 2) throw std::runtime_error("Expected close-door samples");
		segments[1].index = 1;
		std::ifstream in(argv[1]);
		int state, mode, elapsed, play, blocked, full, front, back, count, time, samples = 0;
		while (in >> state >> mode >> elapsed >> play >> blocked >> full >> front >> back >> count >> time) {
			walls[0].state = walls[1].state = state;
			auto &array = gameData.wallData.activeDoors;
			array = {};
			array.full = full != 0;
			array.count = full ? 89 : mode ? 1
			                               : 0;
			if (mode) {
				auto &entry = array.data[array.count - 1];
				entry.time = elapsed;
				entry.nPartCount = mode == 3 ? 2 : 1;
				if (mode == 2) entry.nBackWall[0] = 0;
				else entry.nFrontWall[mode == 3 ? 1 : 0] = 0;
			}
			gameData.wallData.pAnim[0].xTotalTime = play;
			obstructed = blocked != 0;
			segments[0].CloseDoor(0, true);
			const int actual_time = array.count ? array.data[array.count - 1].time : -1;
			if (walls[0].state != front || walls[1].state != back || int(array.count) != count || actual_time != time)
				throw std::runtime_error("Close-door mismatch at " + std::to_string(samples));
			++samples;
		}
		if (samples != 3072) throw std::runtime_error("Incomplete close-door matrix");
		std::cout << samples << " extracted-reference close-door state/allocation samples passed\n";
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
