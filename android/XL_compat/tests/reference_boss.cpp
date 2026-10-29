// Engine/storage adapters around extracted XL release-profile boss functions
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#define ENTER(...)
#define RETVAL(value)          return (value);
#define DBG                    0
#define TRACE                  0
#define NUM_D2_BOSSES          8
#define MAX_SPEW_BOT           3
#define BOSS_D2                21
#define BOSS_GATE_PRODUCER_NUM -1
#define SUSP_ROBOTS            1
#define OBJ_ROBOT              2
#define OBJ_PLAYER             4
#define OBJ_REACTOR            9
#define PF_LEVELLING           1
#define PF_USES_THRUST         2
#define TT_SPAWN_BOT           26
#define ANIM_MORPHING_ROBOT    0
#define I2X(value)             ((value) * 65536)
using fix = std::int32_t;
// Default macros from the hash-pinned maths.h, truncating toward zero
#define FixMul(a, b) (fix(double(b) / 65536.0 * double(a)))
#define FixDiv(a, b) (fix((b) ? double(a) / double(b) * 65536.0 : 1))
std::vector<int> tokens;
std::size_t cursor = 0;
int Rand(int maximum)
{
	if (cursor >= tokens.size()) throw std::runtime_error("Boss reference exhausted RNG transcript");
	return tokens[cursor++] % maximum;
}
int RandShort()
{
	return Rand(32768);
}
int SRandShort()
{
	return 16383 - RandShort();
}
struct CFixVector {
	struct {
		struct {
			fix x = 0, y = 0, z = 0;
		} coord;
	} v;
	CFixVector operator-(const CFixVector &other) const
	{
		return { { { v.coord.x - other.v.coord.x, v.coord.y - other.v.coord.y, v.coord.z - other.v.coord.z } } };
	}
	CFixVector operator+(const CFixVector &other) const
	{
		return { { { v.coord.x + other.v.coord.x, v.coord.y + other.v.coord.y, v.coord.z + other.v.coord.z } } };
	}
	const CFixVector &operator*=(fix scale);
	static fix Dist(const CFixVector &a, const CFixVector &b)
	{
		const auto d = a - b;
		return fix(std::sqrt(double(d.v.coord.x) * d.v.coord.x + double(d.v.coord.y) * d.v.coord.y + double(d.v.coord.z) * d.v.coord.z));
	}
	fix Mag() const
	{
		return Dist(*this, {});
	}
	static const fix Normalize(CFixVector &vector);
};
struct tRobotInfo {
	int nModel = 0;
	fix strength = 0, mass = 0, drag = 0;
	int behavior = 0, bossFlag = 0, companion = 0, scoreValue = 0;
};
std::array<tRobotInfo, 200> robots;
struct CObject {
	struct {
		int nId = 0, nType = OBJ_ROBOT, nSegment = 0, nCreator = 0, nNextInSeg = -1;
		fix xSize = 0;
		struct {
			CFixVector vPos;
		} position;
	} info;
	struct {
		struct {
			fix mass = 0, drag = 0;
			int flags = 0;
			CFixVector velocity, rotThrust;
		} physInfo;
	} mType;
	struct {
		struct {
			std::int8_t SKIP_AI_COUNT = 0;
		} aiInfo;
	} cType;
	struct {
		struct {
			int nModel = 0, nSubObjFlags = 0;
		} polyObjInfo;
	} rType;
	fix life = 0, shields = 0;
	bool morphed = false;
	void SetLife(fix value)
	{
		life = value;
	}
	void SetShield(fix value)
	{
		shields = value;
	}
	void MorphStart()
	{
		morphed = true;
	}
	int Index() const;
	int CreateGatedRobot(std::int16_t segment, std::uint8_t type, CFixVector *position);
	int BossSpewRobot(CFixVector *position, std::int16_t type, int attached);
};
std::array<CObject, 1200> objects;
std::vector<CObject> population;
int CObject::Index() const
{
	return int(this - objects.data());
}
#define OBJ_IDX(object) ((object)->Index())
bool allocated = false, available = false, has_spawn = false;
CObject *OBJECT(int index)
{
	return index >= 0 && index < int(objects.size()) && (index != 1 || allocated) ? &objects[index] : nullptr;
}
tRobotInfo *ROBOTINFO(int index)
{
	return index >= 0 && index < int(robots.size()) ? &robots[index] : nullptr;
}
tRobotInfo *ROBOTINFO(CObject *object)
{
	return ROBOTINFO(object->info.nId);
}
#define FORALL_ROBOT_OBJS(object)      \
	for (auto &candidate : population) \
		if (((object) = &candidate), true)
struct CSegment {
	int m_nVertices = 8, m_objects = 0;
	std::array<std::uint16_t, 8> m_vertices{ { 0, 1, 2, 3, 4, 5, 6, 7 } };
	CFixVector m_vCenter;
	CFixVector &Center()
	{
		return m_vCenter;
	}
	CFixVector RandomPoint();
} segment;
CSegment *SEGMENT(int)
{
	return &segment;
}
struct BossState {
	std::int64_t m_nLastGateTime = 0;
	fix m_nGateInterval = 0;
};
struct {
	struct {
		bool bD1Mission = false;
		int nDifficultyLevel = 0, bGameSuspended = 0;
	} app;
} gameStates;
struct {
	struct {
		std::array<CFixVector, 8> vertices;
	} segData;
	struct {
		std::int64_t xGame = I2X(20);
		fix xFrame = 0;
	} timeData;
	struct {
		BossState state;
		int Find(int index) const
		{
			return index == 0 ? 0 : -1;
		}
		BossState &operator[](int)
		{
			return state;
		}
	} bossData;
	struct {
		struct Model {
			fix radius = 0;
			fix Rad() const
			{
				return radius;
			}
		} polyModels[2][1000];
	} modelData;
	struct {
		struct {
			int nObjNums[1]{};
		} create;
	} multigame;
	struct {
		int nTypes[2]{};
	} botData;
	struct {
		struct {
			int nSound = 0;
		} animations[2][1];
	} effectData;
} gameData;
struct {
	int numRobotsLevel = 0, numRobotsTotal = 0;
} player;
#define LOCALPLAYER player
int FindObjTrigger(int, int, int)
{
	return has_spawn;
}
int FindSegByPos(const CFixVector &, int, int, int)
{
	return 0;
}
int CreateRobot(std::uint8_t type, std::int16_t where, const CFixVector &position)
{
	if (!available) return -1;
	allocated = true;
	objects[1] = {};
	objects[1].info.nId = type;
	objects[1].info.nSegment = where;
	objects[1].info.position.vPos = position;
	return 1;
}
void InitAIObject(int, int, int) {}
void CreateExplosion(int, const CFixVector &, fix, int) {}
struct {
	void CreateSegmentSound(int, int, int, const CFixVector &, int, fix) {}
} audio;
#include "xl_reference_boss.h"

void check(bool result, const char *message)
{
	if (!result) throw std::runtime_error(message);
}
void read_vector(std::istream &file, CFixVector &vector)
{
	file >> vector.v.coord.x >> vector.v.coord.y >> vector.v.coord.z;
}
bool equal(const CFixVector &a, const CFixVector &b)
{
	return a.v.coord.x == b.v.coord.x && a.v.coord.y == b.v.coord.y && a.v.coord.z == b.v.coord.z;
}
int main(int argc, char **argv)
{
	try {
		check(argc == 2, "Boss reference requires production samples");
		std::ifstream file(argv[1]);
		int types = 0;
		file >> types;
		check(types > 0 && types <= int(robots.size()), "Invalid boss sample definitions");
		gameData.botData.nTypes[0] = types;
		for (int i = 0; i < types; ++i) {
			auto &robot = robots[i];
			fix radius;
			file >> robot.nModel >> robot.strength >> robot.mass >> robot.drag >> radius >> robot.behavior >> robot.bossFlag >> robot.companion >> robot.scoreValue;
			check(robot.nModel >= 0 && robot.nModel < 1000, "Invalid boss sample model");
			gameData.modelData.polyModels[0][robot.nModel].radius = radius;
		}
		int cases = 0, requested, attached, count, success, increment, expected_skip = 0, expected_type = 0;
		std::int64_t elapsed, expected_elapsed;
		while (file >> gameStates.app.nDifficultyLevel >> gameData.timeData.xFrame >> elapsed >> gameData.bossData.state.m_nGateInterval >> requested >> attached >> count >> available >> gameStates.app.bGameSuspended >> objects[0].info.nId >> objects[0].info.xSize) {
			objects[0].info.nSegment = 0;
			objects[0].info.nCreator = 0;
			read_vector(file, objects[0].info.position.vPos);
			read_vector(file, segment.m_vCenter);
			for (auto &vertex : gameData.segData.vertices) read_vector(file, vertex);
			int obstacles = 0;
			file >> obstacles;
			check(obstacles >= 0 && obstacles + 2 < int(objects.size()), "Invalid boss obstacles");
			objects[0].info.nNextInSeg = obstacles ? 2 : -1;
			for (int i = 0; i < obstacles; ++i) {
				auto &object = objects[i + 2];
				object = {};
				file >> object.info.xSize;
				read_vector(file, object.info.position.vPos);
				object.info.nNextInSeg = i + 1 < obstacles ? i + 3 : -1;
			}
			file >> success >> expected_elapsed >> increment;
			fix life = 0, shields = 0, mass = 0, drag = 0;
			CFixVector position, velocity, tumble;
			if (success) {
				file >> expected_type >> life >> shields >> mass >> drag >> expected_skip;
				read_vector(file, position);
				read_vector(file, velocity);
				read_vector(file, tumble);
			}
			std::size_t draws = 0;
			file >> draws;
			check(draws <= 10000, "Invalid boss random transcript");
			tokens.resize(draws);
			for (int &token : tokens) file >> token;
			check(bool(file), "Truncated boss sample");
			cursor = 0;
			allocated = false;
			has_spawn = false;
			player = {};
			gameData.bossData.state.m_nLastGateTime = gameData.timeData.xGame - elapsed;
			population.assign(count, {});
			for (auto &object : population) object.info.nCreator = BOSS_GATE_PRODUCER_NUM;
			const int created = objects[0].BossSpewRobot(nullptr, std::int16_t(requested), attached);
			check((created >= 0) == bool(success), "Reference boss creation decision differs");
			check(gameData.timeData.xGame - gameData.bossData.state.m_nLastGateTime == expected_elapsed, "Reference boss cooldown differs");
			check(player.numRobotsLevel == increment && player.numRobotsTotal == increment, "Reference boss counters differ");
			check(cursor == tokens.size(), "Reference boss RNG consumption differs");
			if (success) {
				const auto &object = objects[1];
				check(object.info.nId == expected_type && object.life == life && object.shields == shields && object.mType.physInfo.mass == mass && object.mType.physInfo.drag == drag, "Reference spawned robot properties differ");
				if (object.cType.aiInfo.SKIP_AI_COUNT != expected_skip || !equal(object.info.position.vPos, position) || !equal(object.mType.physInfo.velocity, velocity) || !equal(object.mType.physInfo.rotThrust, tumble)) {
					std::cerr << "case=" << cases << " skip=" << int(object.cType.aiInfo.SKIP_AI_COUNT) << "/" << expected_skip << " pos=" << object.info.position.vPos.v.coord.x << "," << object.info.position.vPos.v.coord.y << "," << object.info.position.vPos.v.coord.z << "/" << position.v.coord.x << "," << position.v.coord.y << "," << position.v.coord.z << " vel=" << object.mType.physInfo.velocity.v.coord.x << "," << object.mType.physInfo.velocity.v.coord.y << "," << object.mType.physInfo.velocity.v.coord.z << "/" << velocity.v.coord.x << "," << velocity.v.coord.y << "," << velocity.v.coord.z << " rot=" << object.mType.physInfo.rotThrust.v.coord.x << "," << object.mType.physInfo.rotThrust.v.coord.y << "," << object.mType.physInfo.rotThrust.v.coord.z << "/" << tumble.v.coord.x << "," << tumble.v.coord.y << "," << tumble.v.coord.z << '\n';
					throw std::runtime_error("Reference spawned robot placement/motion differs");
				}
				check(object.morphed && object.info.nCreator == BOSS_GATE_PRODUCER_NUM, "Reference boss morph/creator differs");
			}
			++cases;
		}
		check(file.eof() && cases == 50, "Boss reference cases omitted");
		std::cout << "Pinned XL boss spawning: " << cases << " selection/timing/caps/placement/failure/motion/RNG cases matched\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
