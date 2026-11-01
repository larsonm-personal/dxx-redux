#include "camera_glass.h"
#include <iostream>
#include <random>
namespace glass_reference
{
using fix = std::int32_t;
#define ENTER(a, b)
#define RETURN             return;
#define RETVAL(x)          return x;
#define DBG                0
#define DBG_SCAN           0
#define USE_FCD_CACHE      0
#define MULTITHREADED_SCAN 0
#define MAX_SEGMENTS_D2X   256
#define SEGMENT_SIDE_COUNT 6
#define I2X(x)             ((x) * 65536)
struct CFixVector {
	xl_compat::FixedLightPoint point{};
	static int Dist(const CFixVector &a, const CFixVector &b)
	{
		return xl_compat::glass_distance(a.point, b.point);
	}
};
struct CFCDCache {
	void Flush() {}
	void SetPathLength(int) {}
	void Add(int, int, int, int) {}
	int Dist(int, int)
	{
		return -1;
	}
};
struct CSegment {
	int16_t m_children[6]{ -1, -1, -1, -1, -1, -1 };
	int m_childDists[2][6]{};
	bool transparent[6]{};
	CFixVector center;
	int index = 0;
	const CFixVector &Center() const
	{
		return center;
	}
	int IsPassable(int side, void *) const
	{
		return transparent[side] ? 1 : 0;
	}
	int ConnectedSide(const CSegment *other) const
	{
		for (int side = 0; side < 6; ++side)
			if (other->m_children[side] == index) return side;
		return -1;
	}
};
std::array<CSegment, 256> segments;
CSegment *SEGMENT(int index)
{
	return &segments.at(index);
}
struct {
	struct {
		int nSegments = 0;
	} segData;
} gameData;
int FindSegByPos(const CFixVector &, int, int, int)
{
	throw std::runtime_error("Unexpected missing glass route segment");
}
#include "xl_reference_camera_glass.h"
} // namespace glass_reference
void camera_glass_source_controls()
{
	using namespace glass_reference;
	std::mt19937 random(477);
	unsigned cases = 0, blocked = 0, same = 0;
	for (int count : { 1, 2, 3, 5, 8, 16, 32 })
		for (int topology = 0; topology < 12; ++topology) {
			std::vector<xl_compat::GlassRouteRoom> rooms(count);
			segments = {};
			gameData.segData.nSegments = count;
			for (int i = 0; i < count; ++i) {
				auto &room = rooms[i];
				auto &source = segments[i];
				source.index = i;
				room.center = { int(random() % 100) * 65536, int(random() % 100) * 65536, int(random() % 100) * 65536 };
				source.center.point = room.center;
				for (int side = 0; side < 6; ++side) {
					const int candidate = side == 0 ? i + 1 : side == 1 ? i - 1
					                                                    : int(random() % count);
					const int child = candidate >= 0 && candidate < count && candidate != i ? candidate : -1;
					source.m_children[side] = room.children[side] = child;
					source.transparent[side] = room.transparent[side] = topology < 3 || random() % 3;
				}
			}
			for (int i = 0; i < count; ++i)
				for (int side = 0; side < 6; ++side) {
					const int child = rooms[i].children[side];
					segments[i].m_childDists[0][side] = child < 0 ? -1 : std::max(1, xl_compat::glass_distance(rooms[i].center, rooms[child].center));
				}
			CSimpleBiDirRouter source{};
			for (int start = 0; start < count; ++start)
				for (int destination = 0; destination < count; ++destination)
					for (int depth : { 0, 1, 2, 5 }) {
						CFixVector a{ rooms[start].center }, b{ rooms[destination].center };
						a.point[0] += 17 * 65536;
						b.point[1] -= 13 * 65536;
						const int expected = source.PathLength(a, start, b, destination, depth, 1, -1);
						const int actual = xl_compat::camera_glass_route(rooms, start, destination, a.point, b.point, depth);
						if (expected != actual) {
							std::cerr << "Glass route mismatch count=" << count << " topology=" << topology << " start=" << start << " end=" << destination << " depth=" << depth << " expected=" << expected << " actual=" << actual << '\n';
							throw std::runtime_error("Camera glass bidirectional route differs from source");
						}
						++cases;
						if (expected < 0) ++blocked;
						if (start == destination) ++same;
					}
		}
	std::cout << "Camera glass source: " << cases << " uncached routes, " << blocked << " blocked, " << same << " same-room cases passed\n";
}
