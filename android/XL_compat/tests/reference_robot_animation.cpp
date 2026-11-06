#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <set>
#include <string>

namespace robot_animation_reference
{
#define ENTER(...)
#define RETVAL(x) return (x);
#define I2X(x)    ((x) * 65536)
constexpr int AS_REST = 0, AS_ALERT = 1, AS_FIRE = 2, AS_RECOIL = 3, AS_FLINCH = 4;
constexpr int AIS_REST = 1, AIS_LOCK = 3, AIS_FLINCH = 4, AIS_FIRE = 5, AIS_RECOVER = 6;
constexpr int ANIM_RATE = 65536 / 16, DELTA_ANG_SCALE = 16;
using fix = int32_t;
struct CAngleVector {
	struct {
		int16_t vec[3]{};
	} v;
};
struct tJointPos {
	int jointnum = 0;
	CAngleVector angles;
} rows[5];
struct tPolyObjInfo {
	CAngleVector animAngles[10];
};
struct tAIStaticInfo {
	int GOAL_STATE = 0, CURRENT_STATE = AIS_REST;
};
struct tAILocalInfo {
	CAngleVector goalAngles[10], deltaAngles[10];
	int achievedState[9]{}, goalState[9]{};
};
struct tRobotInfo {
	int nGuns = 1, attackType = 0;
} robot;
struct CObject {
	struct {
		int nId = 0;
	} info;
	struct {
		tPolyObjInfo polyObjInfo;
	} rType;
	struct {
		tAIStaticInfo aiInfo;
	} cType;
	int Index() const
	{
		return 0;
	}
	int ModelId() const
	{
		return 0;
	}
} object;
struct Model {
	int count = 1;
	int ModelCount() const
	{
		return count;
	}
};
struct {
	struct {
		Model polyModels[1][1];
	} modelData;
	struct {
		tAILocalInfo localInfo[1];
	} aiData;
} gameData;
tRobotInfo *ROBOTINFO(int)
{
	return &robot;
}
int RobotGetAnimState(tJointPos **out, int, int, int)
{
	*out = rows;
	return 5;
}
#include "xl_reference_robot_animation.h"
#undef ENTER
#undef RETVAL
#undef I2X
} // namespace robot_animation_reference

void compare_robot_animation(const char *path)
{
	using namespace robot_animation_reference;
	std::ifstream file(path);
	const auto read = [&] { int value; if (!(file >> value)) throw std::runtime_error("Truncated animation sample"); return value; };
	const int cases = read();
	if (cases != 252) throw std::runtime_error("Incomplete native animation sample set");
	std::set<std::string> seen;
	for (int c = 0; c < cases; ++c) {
		const int count = read(), guns = read(), goal = read(), attack = read();
		if ((count != 1 && count != 3 && count != 10) || (guns != 1 && guns != 2 && guns != 8) || goal < 1 || goal > 7 || attack < 0 || attack > 1)
			throw std::runtime_error("Unexpected animation sample profile");
		gameData.modelData.polyModels[0][0].count = count;
		robot.nGuns = guns;
		robot.attackType = attack;
		object.cType.aiInfo.GOAL_STATE = goal;
		object.cType.aiInfo.CURRENT_STATE = AIS_REST;
		auto &local = gameData.aiData.localInfo[0];
		std::string key = std::to_string(count) + "/" + std::to_string(guns) + "/" + std::to_string(goal) + "/" + std::to_string(attack);
		for (int i = 0; i < 5; ++i) {
			rows[i].jointnum = read();
			key += "/" + std::to_string(rows[i].jointnum);
			if (rows[i].jointnum < 0 || rows[i].jointnum >= 10) throw std::runtime_error("Invalid sample storage index");
			for (int a = 0; a < 3; ++a) rows[i].angles.v.vec[a] = read();
		}
		if (!seen.insert(key).second) throw std::runtime_error("Duplicate animation sample");
		for (int g = 0; g < 9; ++g) {
			local.goalState[g] = g % 2 ? AIS_RECOVER : AIS_FLINCH;
			local.achievedState[g] = AIS_REST;
		}
		for (int i = 0; i < 10; ++i) {
			object.rType.polyObjInfo.animAngles[i] = { { { static_cast<int16_t>(i), 0, -30000 } } };
			local.goalAngles[i] = { { { 123, 456, 789 } } };
			local.deltaAngles[i] = { { { 12, 34, 56 } } };
		}
		const int result = DoSillyAnimation(&object);
		if (result != read() || object.cType.aiInfo.CURRENT_STATE != read()) throw std::runtime_error("Native animation state differs from pinned source");
		for (const auto *values : { object.rType.polyObjInfo.animAngles, local.goalAngles, local.deltaAngles })
			for (int i = 0; i < 10; ++i)
				for (int a = 0; a < 3; ++a)
					if (values[i].v.vec[a] != read()) throw std::runtime_error("Native animation angles differ from pinned source: " + key);
		for (const auto *values : { local.achievedState, local.goalState })
			for (int i = 0; i < 9; ++i)
				if (values[i] != read()) throw std::runtime_error("Native animation completion differs from pinned source");
	}
	std::string extra;
	if (file >> extra) throw std::runtime_error("Unexpected trailing animation evidence");
	std::cout << "Pinned robot animation matched 252 mixed/overlapping/boundary/all-unused cases\n";
}
