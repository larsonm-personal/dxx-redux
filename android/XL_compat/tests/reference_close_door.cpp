#include <cstdint>
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include "xl_reference_door_capacity.h"
#include "xl_reference_door_object_types.h"
#define Assert assert
using fix = int32_t;
constexpr int WALL_DOOR_CLOSED = 0, WALL_DOOR_OPENING = 1, WALL_DOOR_WAITING = 2, WALL_DOOR_CLOSING = 3, WALL_DOOR_OPEN = 4;
constexpr int ND_STATE_RECORDING = 1, ND_STATE_PLAYBACK = 2, NO_WALL = -1;
constexpr int WALL_DOOR_OPENED = 2, WALL_DOOR_AUTO = 16;
#define ENTER(a, b)
#define RETURN     return;
#define IS_WALL(i) ((i) >= 0 && (i) < 2)
struct CActiveDoor {
	int nPartCount = 0, nFrontWall[2]{ -1, -1 }, nBackWall[2]{ -1, -1 };
	fix time = 0;
};
struct DoorArray {
	using _T = CActiveDoor;
	CActiveDoor data[MAX_DOORS]{};
	unsigned m_tos = 0, m_growth = 0;
	struct {
		unsigned length = MAX_DOORS;
		CActiveDoor *buffer;
	} m_data{ MAX_DOORS, data };
	CActiveDoor *Resize(unsigned)
	{
		throw std::runtime_error("Unexpected active-door stack growth");
	}
#include "xl_reference_door_stack.h"
	CActiveDoor *Top()
	{
		return &data[m_tos - 1];
	}
	CActiveDoor *Buffer()
	{
		return data;
	}
	unsigned ToS() const
	{
		return m_tos;
	}
};
struct {
	struct {
		DoorArray activeDoors;
		struct {
			fix xTotalTime = 0;
			int openSound = -1;
			int nFrameCount = 0;
		} pAnim[1];
	} wallData;
	struct {
		int nState = 0;
	} demoData;
} gameData;
template <class T>
int WallEffectFrameCount(T *clip)
{
	return clip->nFrameCount;
}
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
	int state = 0, nClip = 0, nSegment = 0, nSide = 0, flags = 0;
	int32_t AnimateOpeningDoor(fix);
	int32_t AnimateClosingDoor(fix);
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
struct CObject {
	struct {
		int16_t nType = 2, nNextInSeg = -1;
	} info;
} obstruction_object;
CObject *OBJECT(int)
{
	return &obstruction_object;
}
struct CSegment {
	int index = 0, m_children[6]{ 1, 1, 1, 1, 1, 1 };
	int16_t m_objects = 0;
	int frame = -1;
	void SetTexture(int, CSegment *, int, int, int value)
	{
		frame = value;
	}
	CWall *Wall(int)
	{
		return &walls[index];
	}
	bool PokesThrough(int, int)
	{
		return obstructed;
	}
	int32_t DoorIsBlocked(int32_t, bool);
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
		if (argc != 3) throw std::runtime_error("Expected close-door transition and animation samples");
		segments[1].index = 1;
		std::ifstream in(argv[1]);
		int state, mode, elapsed, play, blocked, full, front, back, count, time, samples = 0;
		while (in >> state >> mode >> elapsed >> play >> blocked >> full >> front >> back >> count >> time) {
			walls[0].state = walls[1].state = state;
			auto &array = gameData.wallData.activeDoors;
			for (auto &entry : array.data) entry = {};
			array.m_tos = full ? MAX_DOORS : mode ? 1
			                                      : 0;
			if (mode) {
				auto &entry = array.data[array.m_tos - 1];
				entry.time = elapsed;
				entry.nPartCount = mode == 3 ? 2 : 1;
				if (mode == 2) entry.nBackWall[0] = 0;
				else entry.nFrontWall[mode == 3 ? 1 : 0] = 0;
			}
			gameData.wallData.pAnim[0].xTotalTime = play;
			obstructed = blocked != 0;
			segments[0].CloseDoor(0, true);
			const int actual_time = array.m_tos ? array.data[array.m_tos - 1].time : -1;
			if (walls[0].state != front || walls[1].state != back || int(array.m_tos) != count || actual_time != time)
				throw std::runtime_error("Close-door mismatch at " + std::to_string(samples));
			++samples;
		}
		if (samples != 3072) throw std::runtime_error("Incomplete close-door matrix");
		std::cout << samples << " extracted-reference close-door state/allocation samples passed\n";
		int obstruction_samples = 0;
		for (int type = 0; type <= 20; ++type)
			for (bool ignore_marker : { false, true })
				for (bool overlap : { false, true }) {
					obstruction_object.info.nType = int16_t(type);
					obstructed = overlap;
					const bool ignored = type == OBJ_WEAPON || type == OBJ_FIREBALL || type == OBJ_EXPLOSION || type == OBJ_EFFECT || (ignore_marker && type == OBJ_MARKER);
					if (bool(segments[0].DoorIsBlocked(0, ignore_marker)) != (overlap && !ignored))
						throw std::runtime_error("Source door obstruction filter differs");
					++obstruction_samples;
				}
		std::cout << obstruction_samples << " extracted-reference object-filter cases passed; ignore-marker remains outside runtime admission\n";
		std::ifstream animations(argv[2]);
		int automatic, opening, frames, duration, elapsed_ticks, expected_state, expected_flags, expected_frame, expected_count, expected_time;
		int animation_count = 0;
		while (animations >> automatic >> opening >> frames >> duration >> elapsed_ticks >> expected_state >> expected_flags >> expected_frame >> expected_count >> expected_time) {
			walls[0] = {};
			walls[0].state = opening ? WALL_DOOR_OPENING : WALL_DOOR_CLOSING;
			walls[0].flags = (automatic ? WALL_DOOR_AUTO : 0) | (opening ? 0 : WALL_DOOR_OPENED);
			segments[0].frame = -1;
			gameData.wallData.pAnim[0].nFrameCount = frames;
			gameData.wallData.pAnim[0].xTotalTime = duration;
			const int result = opening ? walls[0].AnimateOpeningDoor(elapsed_ticks) : walls[0].AnimateClosingDoor(elapsed_ticks);
			if (!opening && (result & 1)) {
				walls[0].state = WALL_DOOR_CLOSED;
				segments[0].frame = 0;
			}
			const int retained = !(result & 1);
			const int timer = retained && !(opening && (result & 2)) ? elapsed_ticks : 0;
			if (walls[0].state != expected_state || walls[0].flags != expected_flags || segments[0].frame != expected_frame || retained != expected_count || timer != expected_time)
				throw std::runtime_error("Door animation mismatch at " + std::to_string(animation_count));
			++animation_count;
		}
		if (!animations.eof() || animation_count != 660) throw std::runtime_error("Incomplete door animation samples");
		std::cout << animation_count << " extracted-reference door frame/flag/state/timer samples passed\n";
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
