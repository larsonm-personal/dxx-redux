// Engine queries are recorded inputs; compile the unchanged XL list/action bodies
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include "../runtime/boss_destinations.h"
#define DBG                    0
#define MAX_BOSS_TELEPORT_SEGS 1000
#define SEGMENT_SIDE_COUNT     6
#define WID_PASSABLE_FLAG      1
#define I2X(value)             ((value) * 65536)
using fix = std::int32_t;
using CFixVector = std::array<fix, 3>;
template <class T>
struct Array : std::vector<T> {
	bool Create(int n)
	{
		this->resize(n);
		return true;
	}
	void Destroy()
	{
		this->clear();
	}
	T *Buffer()
	{
		return this->data();
	}
	int Length() const
	{
		return static_cast<int>(this->size());
	}
};
using CShortArray = Array<std::int16_t>;
using CIntArray = Array<int>;
struct CObject {
	struct {
		int nSegment = 0;
		fix xSize = 65536;
		struct {
			CFixVector vPos{};
		} position;
	} info;
	int index = 0;
	bool boss = true;
	bool IsBoss() const
	{
		return boss;
	}
	int Index() const
	{
		return index;
	}
	void RelinkToSeg(int segment)
	{
		info.nSegment = segment;
	}
	void SetSize(fix size)
	{
		info.xSize = size;
	}
};
std::array<CObject, 2> objects;
#define OBJECT(index) (&objects.at(index))
struct CSegment {
	int m_group = -1;
	std::array<int, 6> m_children{}, passable{};
	CFixVector center{};
	bool fits = true;
	int IsPassable(int side, void *) const
	{
		return passable[side];
	}
	CFixVector Center() const
	{
		return center;
	}
};
std::vector<CSegment> oracle_segments;
#define SEGMENT(index)  (&oracle_segments.at(index))
#define IS_CHILD(index) ((index) >= 0 && static_cast<std::size_t>(index) < oracle_segments.size())
#define Min(a, b)       (std::min)((a), (b))
struct CBossInfo {
	int m_nObject = -1;
	CShortArray m_gateSegs, m_teleportSegs;
	int m_nGateSegs = 0, m_nTeleportSegs = 0;
	fix m_nGateInterval = 0, m_nTeleportInterval = 0, m_nCloakInterval = 0;
	bool SetupSegments(CShortArray &, int, int);
	bool Setup(std::int16_t);
	void Destroy()
	{
		throw std::runtime_error("Unqualified reference allocation failure");
	}
};
std::vector<bool> visited;
struct Mine {
	struct Visibility {
		void BumpVisitedFlag()
		{
			visited.assign(oracle_segments.size(), false);
		}
	} visibility[1];
	bool Visited(int i) const
	{
		return visited.at(i);
	}
	void Visit(int i)
	{
		visited.at(i) = true;
	}
};
struct {
	struct {
		int nSegments = 0;
	} segData;
	struct {
		Mine mine;
	} renderData;
	struct {
		std::array<CBossInfo, 2> bosses;
		int Find(int index) const
		{
			return index >= 0 && index < 2 && bosses[index].m_nObject == index ? index : -1;
		}
		CBossInfo &operator[](int index)
		{
			return bosses.at(index);
		}
	} bossData;
} gameData;
struct {
	struct {
		bool bD1Mission = false;
		int nDifficultyLevel = 2;
	} app;
} gameStates;
struct {
	int nCurrentLevel = 1, nLastLevel = 1;
} missionManager;
#define LEVEL_SEGMENTS gameData.segData.nSegments
int BossFitsInSeg(CObject *, int segment)
{
	return oracle_segments.at(segment).fits;
}
std::vector<int> random_tokens;
std::size_t random_cursor = 0;
int Rand(int count)
{
	if (random_cursor >= random_tokens.size()) throw std::runtime_error("Reference RNG exhausted");
	const int result = random_tokens[random_cursor++] % count;
	return result;
}
struct CTrigger {
	int m_nLinks = 0;
	std::array<std::int16_t, 10> m_segments{};
	struct {
		int nTeleportDest = 0;
	} m_info;
	void DoTeleportBot(CObject *);
};
#include "xl_reference_boss_routes.h"

static void require(bool value, const char *message)
{
	if (!value) throw std::runtime_error(message);
}
static void matrix_controls()
{
	// Independent graph controls, including the source's home revisit and cap
	for (int count : { 1, 2, 17, 1000, 1001 })
		for (int scenario = 0; scenario < 4; ++scenario) {
			oracle_segments.assign(count, {});
			gameData.segData.nSegments = count;
			for (int i = 0; i < count; ++i) {
				auto &s = oracle_segments[i];
				s.m_children.fill(-1);
				s.passable.fill(1);
				s.fits = true;
				if (i + 1 < count) s.m_children[0] = i + 1;
				if (i > 0) s.m_children[1] = i - 1;
				if (scenario == 1 && i >= count / 2) s.m_group = 1;
				if (scenario == 2 && i == count / 2) s.fits = false;
				if (scenario == 3 && i == 0) s.passable[0] = 0;
			}
			objects[0].info.nSegment = 0;
			objects[0].info.position.vPos = { 7, 8, 9 };
			auto &boss = gameData.bossData[0];
			boss = {};
			boss.m_nObject = 0;
			require(boss.Setup(0), "Reference setup failed");
			const auto native = xl_compat::boss_destinations(0, count, [](int segment, int side) { return oracle_segments.at(segment).m_children[side]; }, [](int segment, int side) { return oracle_segments.at(segment).passable[side] != 0; }, [](int segment) { return oracle_segments.at(segment).m_group; }, [](int segment) { return oracle_segments.at(segment).fits; });
			require(native.size() == boss.m_teleportSegs.size() && std::equal(native.begin(), native.end(), boss.m_teleportSegs.begin()), "Production destination algorithm differs on reference graph/group/capacity boundary");
			require(!boss.m_teleportSegs.empty() && boss.m_teleportSegs.front() == 0 && boss.m_teleportSegs.size() <= static_cast<std::size_t>((std::min) (count, 1000)), "Reference list boundary failed");
			require(objects[0].info.nSegment == 0 && objects[0].info.position.vPos == CFixVector{ 7, 8, 9 }, "Reference setup moved its owner");
			if (scenario == 3) require(boss.m_teleportSegs.size() == 1, "XL setup unexpectedly crosses a closed first wall");
		}
	// Every serialized label, including the default 255, remains a distinct group
	for (int label = 0; label < 256; ++label)
		for (int split = 0; split < 2; ++split) {
			oracle_segments.assign(4, {});
			gameData.segData.nSegments = 4;
			for (int i = 0; i < 4; ++i) {
				auto &segment = oracle_segments[i];
				segment.m_children.fill(-1);
				segment.passable.fill(1);
				segment.fits = true;
				segment.m_children[0] = (i + 1) % 4;
				const int raw = split && i == 2 ? (label ^ 128) : label;
				segment.m_group = raw < 128 ? raw : raw - 256;
			}
			objects[0].info.nSegment = 0;
			auto &boss = gameData.bossData[0];
			boss = {};
			boss.m_nObject = 0;
			require(boss.Setup(0), "Group label source setup failed");
			const auto actual = xl_compat::boss_destinations(0, 4, [](int i, int side) { return oracle_segments[i].m_children[side]; }, [](int, int) { return true; }, [](int i) { return oracle_segments[i].m_group; }, [](int) { return true; });
			require(actual.size() == boss.m_teleportSegs.size() && std::equal(actual.begin(), actual.end(), boss.m_teleportSegs.begin()), "Byte label changed source group membership");
			require((std::find(actual.begin(), actual.end(), 2) != actual.end()) == !split, "Open connection bypassed an authored group boundary");
		}
}
int main(int argc, char **argv)
{
	try {
		matrix_controls();
		if (argc != 2) throw std::runtime_error("Expected native boss transport samples");
		std::ifstream input(argv[1]);
		int count = 0;
		input >> count;
		require(count > 0 && count <= 100, "Missing native transport samples");
		for (int sample = 0; sample < count; ++sample) {
			int segment_count, actor, links, destination, draws;
			input >> segment_count >> actor >> links >> destination >> draws;
			require(segment_count > 0 && segment_count <= 9000 && actor >= 0 && actor < 2 && links >= 0 && links <= 10 && draws >= 0 && draws < 1000, "Invalid native transport sample header");
			oracle_segments.assign(segment_count, {});
			gameData.segData.nSegments = segment_count;
			for (auto &s : oracle_segments) {
				input >> s.m_group;
				for (auto &child : s.m_children) input >> child;
				for (auto &flag : s.passable) input >> flag;
				for (auto &component : s.center) input >> component;
				input >> s.fits;
			}
			for (int i = 0; i < 2; ++i) {
				auto &o = objects[i];
				o.index = i;
				o.boss = true;
				input >> o.info.nSegment;
				for (auto &component : o.info.position.vPos) input >> component;
				auto &boss = gameData.bossData[i];
				boss = {};
				boss.m_nObject = i;
				int n;
				input >> n;
				require(n > 0 && n <= 1000, "Invalid prior destination count");
				boss.m_teleportSegs.resize(n);
				for (auto &segment : boss.m_teleportSegs) input >> segment;
			}
			CTrigger action;
			action.m_nLinks = links;
			action.m_info.nTeleportDest = destination;
			for (int i = 0; i < links; ++i) input >> action.m_segments[i];
			random_tokens.resize(draws);
			random_cursor = 0;
			for (auto &token : random_tokens) input >> token;
			action.DoTeleportBot(&objects[actor]);
			int expected_destination;
			input >> expected_destination;
			require(action.m_info.nTeleportDest == expected_destination && random_cursor == random_tokens.size(), "Object teleport selection/RNG differs");
			for (int i = 0; i < 2; ++i) {
				int segment, n;
				CFixVector position{};
				input >> segment;
				for (auto &component : position) input >> component;
				input >> n;
				require(n > 0 && n <= 1000, "Invalid expected destination count");
				CShortArray expected;
				expected.resize(n);
				for (auto &s : expected) input >> s;
				require(objects[i].info.nSegment == segment && objects[i].info.position.vPos == position && gameData.bossData[i].m_teleportSegs == expected, "Native boss position or rebuilt destination order differs from source");
			}
			require(bool(input), "Truncated native boss transport samples");
		}
		std::cout << "Pinned XL boss transport: " << count << " native cases, 20 graph/capacity and 512 group-label controls matched\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
